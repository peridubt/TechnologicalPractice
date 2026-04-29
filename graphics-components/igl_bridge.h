#ifndef TECHNOLOGICALPRACTICE_IGL_BRIDGE_H
#define TECHNOLOGICALPRACTICE_IGL_BRIDGE_H

#include <Eigen/Core>
#include <glm/glm.hpp>

#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

#include "loader/model.h"

// Объединяет каждый Mesh внутри Model в одну пару Eigen-матриц (V, F),
// чтобы алгоритмы libigl могли работать со всем объектом как с одним
// набором полигонов
inline void buildEigenMesh(const Model &model,
                           Eigen::MatrixXd &V,
                           Eigen::MatrixXi &F)
{
    // хеширования glm::vec3 по точному float-битпаттерну
    struct Vec3Hash {
        size_t operator()(const glm::vec3 &v) const noexcept
        {
            uint32_t a, b, c;
            std::memcpy(&a, &v.x, sizeof(uint32_t));
            std::memcpy(&b, &v.y, sizeof(uint32_t));
            std::memcpy(&c, &v.z, sizeof(uint32_t));
            // Смешиваем три 32-битных слова в одно size_t.
            uint64_t h = static_cast<uint64_t>(a) * 0x9E3779B185EBCA87ull
                       + static_cast<uint64_t>(b);
            h = (h ^ (h >> 32)) * 0xC2B2AE3D27D4EB4Full
              + static_cast<uint64_t>(c);
            return (h ^ (h >> 32));
        }
    };
    struct Vec3Eq {
        bool operator()(const glm::vec3 &a, const glm::vec3 &b) const noexcept
        {
            return a.x == b.x && a.y == b.y && a.z == b.z;
        }
    };

    // подсчёт и резервирование
    size_t totalF = 0;
    size_t totalLocalV = 0;
    for (const Mesh &m : model.meshes)
    {
        totalF      += m.indices.size() / 3;
        totalLocalV += m.vertices.size();
    }

    std::unordered_map<glm::vec3, int, Vec3Hash, Vec3Eq> uniqMap;
    uniqMap.reserve(totalLocalV);
    std::vector<glm::vec3> uniqPositions;
    uniqPositions.reserve(totalLocalV);

    auto canonical = [&](const glm::vec3 &p) -> int {
        auto it = uniqMap.find(p);
        if (it != uniqMap.end()) return it->second;
        int id = static_cast<int>(uniqPositions.size());
        uniqPositions.push_back(p);
        uniqMap.emplace(p, id);
        return id;
    };

    F.resize(static_cast<Eigen::Index>(totalF), 3);
    Eigen::Index fRow = 0;

    for (const Mesh &m : model.meshes)
    {
        std::vector<int> localToCanon(m.vertices.size());
        for (size_t i = 0; i < m.vertices.size(); ++i)
            localToCanon[i] = canonical(m.vertices[i].Position);

        for (size_t i = 0; i + 2 < m.indices.size(); i += 3)
        {
            int a = localToCanon[m.indices[i]];
            int b = localToCanon[m.indices[i + 1]];
            int c = localToCanon[m.indices[i + 2]];
            if (a == b || b == c || a == c) continue;
            F(fRow, 0) = a;
            F(fRow, 1) = b;
            F(fRow, 2) = c;
            ++fRow;
        }
    }
    if (fRow < F.rows()) F.conservativeResize(fRow, 3);

    V.resize(static_cast<Eigen::Index>(uniqPositions.size()), 3);
    for (size_t i = 0; i < uniqPositions.size(); ++i)
    {
        V(static_cast<Eigen::Index>(i), 0) = uniqPositions[i].x;
        V(static_cast<Eigen::Index>(i), 1) = uniqPositions[i].y;
        V(static_cast<Eigen::Index>(i), 2) = uniqPositions[i].z;
    }
}

struct AABB3
{
    glm::vec3 min;
    glm::vec3 max;

    glm::vec3 center() const { return 0.5f * (min + max); }
    float radius() const { return 0.5f * glm::length(max - min); }
};

inline AABB3 computeAABB(const Eigen::MatrixXd &V)
{
    AABB3 bb{glm::vec3(std::numeric_limits<float>::max()),
             glm::vec3(-std::numeric_limits<float>::max())};
    for (Eigen::Index i = 0; i < V.rows(); ++i)
    {
        glm::vec3 p(static_cast<float>(V(i, 0)),
                    static_cast<float>(V(i, 1)),
                    static_cast<float>(V(i, 2)));
        bb.min = glm::min(bb.min, p);
        bb.max = glm::max(bb.max, p);
    }
    return bb;
}

#endif //TECHNOLOGICALPRACTICE_IGL_BRIDGE_H
