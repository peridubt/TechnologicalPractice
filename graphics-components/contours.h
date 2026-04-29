#ifndef TECHNOLOGICALPRACTICE_CONTOURS_H
#define TECHNOLOGICALPRACTICE_CONTOURS_H

#include <Eigen/Core>
#include <glm/glm.hpp>

#include <cmath>
#include <unordered_map>
#include <utility>
#include <vector>

// =====================================================================
// Сечение (пересечение mesh-плоскости).
//
// Плоскость закодирована как vec4(n.xyz, d), уравнение dot(n, p) + d = 0.
// Для каждого треугольника считаем знаковое расстояние трёх его вершин
// до плоскости; если знаки смешанные, плоскость пересекает треугольник
// ровно по двум рёбрам, и мы выдаём один отрезок.
//
// Это прямая замена igl::isolines: без аллокаций Eigen-матриц,
// без дедупликации вершин на каждом вызове, всего один линейный проход
// по F. Примерно на порядок быстрее на плотных мешах -- именно это
// делает интерактивный поворот плоскости среза плавным.
// =====================================================================
inline void computeSlice(const Eigen::MatrixXd &V,
                         const Eigen::MatrixXi &F,
                         const glm::vec4 &plane,
                         std::vector<glm::vec3> &outSegments)
{
    outSegments.clear();
    if (V.rows() == 0 || F.rows() == 0) return;

    // Знаковое расстояние от каждой вершины до плоскости (плоский массив,
    // дружественный к кэшу).
    std::vector<double> S(static_cast<size_t>(V.rows()));
    for (Eigen::Index i = 0; i < V.rows(); ++i)
        S[static_cast<size_t>(i)] =
            plane.x * V(i, 0) + plane.y * V(i, 1) + plane.z * V(i, 2) + plane.w;

    outSegments.reserve(static_cast<size_t>(F.rows()) / 8);

    for (Eigen::Index f = 0; f < F.rows(); ++f)
    {
        int i0 = F(f, 0), i1 = F(f, 1), i2 = F(f, 2);
        double s0 = S[i0], s1 = S[i1], s2 = S[i2];

        // Тривиальный отбраковщик: треугольник целиком по одну сторону
        // от плоскости.
        if ((s0 > 0.0 && s1 > 0.0 && s2 > 0.0) ||
            (s0 < 0.0 && s1 < 0.0 && s2 < 0.0))
            continue;

        // Собираем до двух пересечений ребро-плоскость. Считая ровный нуль
        // положительным, мы избегаем двойного учёта вершины, лежащей на
        // плоскости (иначе генерировался бы вырожденный отрезок).
        glm::vec3 pts[3];
        int n = 0;
        auto add = [&](int ia, int ib, double sa, double sb) {
            bool a_pos = sa >= 0.0;
            bool b_pos = sb >= 0.0;
            if (a_pos == b_pos) return; // одна сторона, пересечения нет
            double t = sa / (sa - sb);  // здесь sa != sb
            double x = V(ia, 0) + (V(ib, 0) - V(ia, 0)) * t;
            double y = V(ia, 1) + (V(ib, 1) - V(ia, 1)) * t;
            double z = V(ia, 2) + (V(ib, 2) - V(ia, 2)) * t;
            pts[n++] = glm::vec3(static_cast<float>(x),
                                 static_cast<float>(y),
                                 static_cast<float>(z));
        };
        add(i0, i1, s0, s1);
        if (n < 2) add(i1, i2, s1, s2);
        if (n < 2) add(i2, i0, s2, s0);

        if (n == 2)
        {
            outSegments.push_back(pts[0]);
            outSegments.push_back(pts[1]);
        }
    }
}

// Проекция = внешний контур (+ настоящие отверстия) проекции вдоль viewDir.
//
// Этапы:
//   1. Классификация рёбер:
//        - граничные рёбра (только одна соседняя грань) - проекция
//        - внутренние рёбра, соседние грани которых имеют разные знаки
//          dot(normal, viewDir) - проекция
//   2. Соединение рёбер проекции в замкнутые петли.
//   3. Проецируем все вершины меша на плоскость, перпендикулярную viewDir,
//      и считаем для каждой петли знаковую 2D-площадь и центроид.
//   4. Петля с наибольшим |area| - ВНЕШНИЙ контур, оставляем всегда.
//      Любая меньшая петля сохраняется ТОЛЬКО если она - настоящее
//      отверстие в проекции: её центроид должен лежать внутри внешней
//      петли И не быть покрыт ни одним 2D-проецированным треугольником.


inline void computeSilhouette(const Eigen::MatrixXd &V,
                              const Eigen::MatrixXi &F,
                              const Eigen::MatrixXd &FN,
                              const Eigen::MatrixXi &TT,
                              const glm::vec3 &viewDir,
                              std::vector<glm::vec3> &outSegments)
{
    outSegments.clear();
    if (F.rows() == 0) return;

    Eigen::Vector3d v(viewDir.x, viewDir.y, viewDir.z);

    // 1) Классифицируем грани
    std::vector<char> front(static_cast<size_t>(F.rows()));
    for (Eigen::Index f = 0; f < F.rows(); ++f)
        front[f] = (FN.row(f).dot(v) > 0.0) ? 1 : 0;

    // 2) Собираем рёбра проекции как пары индексов вершин меша
    std::vector<std::pair<int, int>> edges;
    edges.reserve(static_cast<size_t>(F.rows()) / 4);
    for (Eigen::Index f = 0; f < F.rows(); ++f)
    {
        for (int k = 0; k < 3; ++k)
        {
            int nb = TT(f, k);
            int va = F(f, (k + 1) % 3);
            int vb = F(f, (k + 2) % 3);
            if (nb < 0)
                edges.emplace_back(va, vb);
            else if (f < nb && front[f] != front[nb])
                edges.emplace_back(va, vb);
        }
    }
    if (edges.empty()) return;

    //3) вершина -> список
    std::unordered_map<int, std::vector<std::pair<int, int>>> adj;
    adj.reserve(edges.size() * 2);
    for (size_t i = 0; i < edges.size(); ++i)
    {
        adj[edges[i].first].emplace_back(edges[i].second, static_cast<int>(i));
        adj[edges[i].second].emplace_back(edges[i].first, static_cast<int>(i));
    }

    //4) Жадная соединение петель; каждое ребро используется максимум раз
    std::vector<char> used(edges.size(), 0);
    std::vector<std::vector<int>> loops;
    for (size_t startE = 0; startE < edges.size(); ++startE)
    {
        if (used[startE]) continue;
        std::vector<int> loop;
        int v0       = edges[startE].first;
        int cur      = v0;
        size_t curE  = startE;
        const size_t safetyCap = edges.size() + 4;
        size_t steps = 0;
        while (steps++ < safetyCap)
        {
            used[curE] = 1;
            int nxt = (edges[curE].first == cur) ? edges[curE].second
                                                 : edges[curE].first;
            loop.push_back(cur);
            cur = nxt;
            if (cur == v0) break;

            auto it = adj.find(cur);
            if (it == adj.end()) break;
            int pickEdge = -1;
            for (auto &p : it->second)
            {
                if (!used[p.second]) { pickEdge = p.second; break; }
            }
            if (pickEdge < 0) break;
            curE = static_cast<size_t>(pickEdge);
        }
        if (loop.size() >= 3) loops.push_back(std::move(loop));
    }
    if (loops.empty()) return;

    // 5) Проецируем все вершины меша в 2D один раз (для площадей,
    //         центроидов и тестов покрытия треугольниками).
    glm::vec3 fwd    = glm::normalize(viewDir);
    glm::vec3 helper = std::abs(fwd.y) > 0.95f ? glm::vec3(1, 0, 0)
                                               : glm::vec3(0, 1, 0);
    glm::vec3 right  = glm::normalize(glm::cross(helper, fwd));
    glm::vec3 upv    = glm::cross(fwd, right);

    std::vector<glm::vec2> V2(static_cast<size_t>(V.rows()));
    for (Eigen::Index i = 0; i < V.rows(); ++i)
    {
        glm::vec3 p(static_cast<float>(V(i, 0)),
                    static_cast<float>(V(i, 1)),
                    static_cast<float>(V(i, 2)));
        V2[static_cast<size_t>(i)] =
            glm::vec2(glm::dot(p, right), glm::dot(p, upv));
    }

    // 6) 2D-площадь и центроид для каждой петли
    std::vector<double>   loopArea    (loops.size(), 0.0);
    std::vector<glm::vec2> loopCenter (loops.size(), glm::vec2(0.0f));
    for (size_t i = 0; i < loops.size(); ++i)
    {
        const auto &L = loops[i];
        double a = 0.0;
        glm::vec2 c(0.0f);
        for (size_t j = 0; j < L.size(); ++j)
        {
            glm::vec2 p1 = V2[L[j]];
            glm::vec2 p2 = V2[L[(j + 1) % L.size()]];
            a += static_cast<double>(p1.x) * static_cast<double>(p2.y)
               - static_cast<double>(p2.x) * static_cast<double>(p1.y);
            c += p1;
        }
        loopArea[i]   = std::abs(a) * 0.5;
        loopCenter[i] = c / static_cast<float>(L.size());
    }

    // Внешняя = петля с наибольшим |area|
    size_t outerI = 0;
    for (size_t i = 1; i < loops.size(); ++i)
        if (loopArea[i] > loopArea[outerI]) outerI = i;
    const double outerArea = loopArea[outerI];

    // 7) Обнаружение отверстий.
    // Точка-в-полигоне (чёт-нечёт, луч вдоль +X)
    auto pointInLoop = [&](const std::vector<int> &L, glm::vec2 q) -> bool {
        bool inside = false;
        size_t n = L.size();
        for (size_t j = 0, k = n - 1; j < n; k = j++)
        {
            glm::vec2 p1 = V2[L[j]];
            glm::vec2 p2 = V2[L[k]];
            if ((p1.y > q.y) != (p2.y > q.y))
            {
                float xCross = p1.x + (q.y - p1.y) / (p2.y - p1.y) * (p2.x - p1.x);
                if (q.x < xCross) inside = !inside;
            }
        }
        return inside;
    };

    // Покрыта ли q каким-либо 2D-проецированным треугольником?
    auto pointInAnyTri = [&](glm::vec2 q) -> bool {
        for (Eigen::Index f = 0; f < F.rows(); ++f)
        {
            glm::vec2 a = V2[F(f, 0)];
            glm::vec2 b = V2[F(f, 1)];
            glm::vec2 c = V2[F(f, 2)];
            float xmin = std::min(std::min(a.x, b.x), c.x);
            float xmax = std::max(std::max(a.x, b.x), c.x);
            if (q.x < xmin || q.x > xmax) continue;
            float ymin = std::min(std::min(a.y, b.y), c.y);
            float ymax = std::max(std::max(a.y, b.y), c.y);
            if (q.y < ymin || q.y > ymax) continue;
            float d1 = (q.x - b.x) * (a.y - b.y) - (a.x - b.x) * (q.y - b.y);
            float d2 = (q.x - c.x) * (b.y - c.y) - (b.x - c.x) * (q.y - c.y);
            float d3 = (q.x - a.x) * (c.y - a.y) - (c.x - a.x) * (q.y - a.y);
            bool hasNeg = (d1 < 0.0f) || (d2 < 0.0f) || (d3 < 0.0f);
            bool hasPos = (d1 > 0.0f) || (d2 > 0.0f) || (d3 > 0.0f);
            if (!(hasNeg && hasPos)) return true;
        }
        return false;
    };

    // 8) Решаем, какие петли оставить: внешнюю + отверстия
    std::vector<size_t> keep;
    keep.reserve(4);
    keep.push_back(outerI);

    for (size_t i = 0; i < loops.size(); ++i)
    {
        if (i == outerI) continue;
        // Игнорируем микроскопический шум жадной сшивки на пересечениях.
        if (loopArea[i] < outerArea * 1e-3) continue;
        // Должна быть внутри внешнего контура.
        if (!pointInLoop(loops[outerI], loopCenter[i])) continue;
        // Должна соответствовать пустой области в проекции.
        if (pointInAnyTri(loopCenter[i])) continue;
        keep.push_back(i);
    }

    // 9) Выдаём оставленные петли как пары конечных точек GL_LINES
    size_t totalSeg = 0;
    for (size_t i : keep) totalSeg += loops[i].size() * 2;
    outSegments.reserve(totalSeg);

    for (size_t i : keep)
    {
        const auto &L = loops[i];
        for (size_t j = 0; j < L.size(); ++j)
        {
            int a = L[j];
            int b = L[(j + 1) % L.size()];
            outSegments.emplace_back(static_cast<float>(V(a, 0)),
                                     static_cast<float>(V(a, 1)),
                                     static_cast<float>(V(a, 2)));
            outSegments.emplace_back(static_cast<float>(V(b, 0)),
                                     static_cast<float>(V(b, 1)),
                                     static_cast<float>(V(b, 2)));
        }
    }
}

#endif //TECHNOLOGICALPRACTICE_CONTOURS_H
