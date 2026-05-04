#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "graphics-components/shader.h"
#include "graphics-components/camera.h"
#include "graphics-components/line_mesh.h"
#include "graphics-components/igl_bridge.h"
#include "graphics-components/contours.h"
#include "loader/model.h"
#include "loader/filesystem.h"

#include <iostream>
#include <memory>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

enum class Mode
{
    Normal     = 1,
    Slice      = 2,
    Projection = 3,
};

void framebuffer_size_callback(GLFWwindow *window, int width, int height);

void mouse_callback(GLFWwindow *window, double xpos, double ypos);

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);

void drop_callback(GLFWwindow *window, int count, const char **paths);

void processInput(GLFWwindow *window);

// Запрос на загрузку другой модели. Заполняется из callback-функций GLFW
// (drop_callback, key_callback) и обрабатывается в основном цикле.
static std::string g_pendingModelPath;

// Открывает системный диалог выбора файла Windows и возвращает выбранный
// путь, либо пустую строку, если пользователь отменил действие.
static std::string pickModelFile()
{
#ifdef _WIN32
    char buf[MAX_PATH] = {0};
    std::string initialDir = FileSystem::getPath("models");

    OPENFILENAMEA ofn{};
    ofn.lStructSize  = sizeof(ofn);
    ofn.hwndOwner    = nullptr;
    ofn.lpstrFile    = buf;
    ofn.nMaxFile     = MAX_PATH;
    ofn.lpstrFilter  = "OBJ models (*.obj)\0*.obj\0All files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle   = "Select 3D model";
    ofn.lpstrInitialDir = initialDir.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn)) return std::string(buf);
#endif
    return {};
}

const unsigned int SCR_WIDTH  = 800;
const unsigned int SCR_HEIGHT = 600;

// Текущий размер фреймбуфера, обновляется в framebuffer_size_callback.
// Используется чтобы согласовать FBO силуэта с экраном (и обновлять
// размер тексела для edge-detection).
int fbWidth  = static_cast<int>(SCR_WIDTH);
int fbHeight = static_cast<int>(SCR_HEIGHT);

Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX     = SCR_WIDTH / 2.0f;
float lastY     = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

Mode mode = Mode::Normal;

// Плоскость среза: n_base = (0,0,1) вращается на yaw вокруг Y
// и на pitch вокруг X; sliceOffset двигает плоскость вдоль её нормали.
float sliceYaw    = 0.0f;
float slicePitch  = 0.0f;
float sliceOffset = 0.0f;

// Направление проекции силуэта. По умолчанию - из +Z.
float silYaw   = 0.0f;
float silPitch = 0.0f;

bool sliceDirty = true;

// Pitch ограничен примерно 85 градусами, чтобы направление обзора
// никогда не схлопывалось на ось +/-Y
static constexpr float kPitchLimit = 1.4835298f; // glm::radians(85.0f)
static const glm::vec3 kWorldUp{0.0f, 1.0f, 0.0f};

struct MaskFBO
{
    GLuint fbo   = 0;
    GLuint color = 0;
    GLuint depth = 0;
    int w        = 0;
    int h        = 0;

    void destroy()
    {
        if (fbo)
        {
            glDeleteFramebuffers(1, &fbo);
            fbo = 0;
        }
        if (color)
        {
            glDeleteTextures(1, &color);
            color = 0;
        }
        if (depth)
        {
            glDeleteRenderbuffers(1, &depth);
            depth = 0;
        }
        w = h = 0;
    }

    void recreate(int newW, int newH)
    {
        if (newW <= 0 || newH <= 0) return;
        if (newW == w && newH == h && fbo != 0) return;
        destroy();
        w = newW;
        h = newH;

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        // Одноканального color attachment достаточно для бинарной маски.
        glGenTextures(1, &color);
        glBindTexture(GL_TEXTURE_2D, color);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0,
                     GL_RED, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, color, 0);

        glGenRenderbuffers(1, &depth);
        glBindRenderbuffer(GL_RENDERBUFFER, depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, depth);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "WARNING: projection FBO incomplete" << std::endl;

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
};

int main(int argc, char **argv)
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4); // MSAA для более гладких линий среза

    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Technological practice", nullptr, nullptr);
    if (window == nullptr)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetDropCallback(window, drop_callback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    stbi_set_flip_vertically_on_load(true);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glLineWidth(2.0f);

    Shader ourShader(FileSystem::getPath("shaders/model_loading.vs").c_str(),
                     FileSystem::getPath("shaders/model_loading.fs").c_str());
    Shader lineShader(FileSystem::getPath("shaders/line.vs").c_str(),
                      FileSystem::getPath("shaders/line.fs").c_str());
    Shader outlineShader(FileSystem::getPath("shaders/outline.vs").c_str(),
                         FileSystem::getPath("shaders/outline.fs").c_str());

    // Стартовая модель: либо argv[1], либо stanford-bunny по умолчанию.
    std::string initialPath = (argc > 1)
                                  ? std::string(argv[1])
                                  : FileSystem::getPath("models/backpack/backpack.obj");

    auto ourModel = std::make_unique<Model>(initialPath);

    // libigl-представление + AABB-параметры пересчитываются при каждой
    // загрузке новой модели, поэтому вынесены в лямбду.
    Eigen::MatrixXd V;
    Eigen::MatrixXi F;
    glm::vec3 center{};
    float radius     = 1.0f;
    float halfExtent = 1.0f;
    float camDist    = 3.0f;

    auto rebuildFromModel = [&]() {
        buildEigenMesh(*ourModel, V, F);
        AABB3 bbox = computeAABB(V);
        center     = bbox.center();
        radius     = std::max(bbox.radius(), 0.001f);
        halfExtent = radius * 1.1f;
        camDist    = radius * 3.0f;
        sliceDirty = true;
    };
    rebuildFromModel();

    LineMesh sliceLines;
    sliceLines.init();

    std::vector<glm::vec3> tmp;

    // Offscreen-маска + пустой VAO для отрисовки фуллскрин-треугольника.
    MaskFBO maskFBO;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    maskFBO.recreate(fbWidth, fbHeight);

    GLuint dummyVAO = 0;
    glGenVertexArrays(1, &dummyVAO);

    std::cout << "Controls:\n"
            << "  1 -- 3D view (WASD + mouse)\n"
            << "  2 -- 2D cross-section (slice)\n"
            << "  3 -- 2D projection / shadow projection\n"
            << "  Mode 2:  arrows = rotate plane, [ / ] = slide along normal\n"
            << "  Mode 3:  arrows = rotate projection direction\n"
            << "  O       -- open 3D model file dialog\n"
            << "  Drag&drop a .obj onto the window also loads it\n";

    while (!glfwWindowShouldClose(window))
    {
        auto currentFrame = static_cast<float>(glfwGetTime());
        deltaTime         = currentFrame - lastFrame;
        lastFrame         = currentFrame;

        processInput(window);

        // Обрабатываем отложенный запрос на загрузку другой модели,
        // поступивший из callback-функций GLFW (drag&drop или клавиша O).
        if (!g_pendingModelPath.empty())
        {
            std::string path = std::move(g_pendingModelPath);
            g_pendingModelPath.clear();
            try
            {
                auto newModel = std::make_unique<Model>(path);
                ourModel      = std::move(newModel);
                rebuildFromModel();
                std::cout << "[model] loaded: " << path << "\n";
            } catch (const std::exception &e)
            {
                std::cout << "[model] failed to load '" << path << "': " << e.what() << "\n";
            }
        }

        // Убеждаемся, что FBO соответствует текущему размеру фреймбуфера.
        if (maskFBO.w != fbWidth || maskFBO.h != fbHeight)
            maskFBO.recreate(fbWidth, fbHeight);

        auto modelMat = glm::mat4(1.0f);

        if (mode == Mode::Normal)
        {
            glViewport(0, 0, fbWidth, fbHeight);
            glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            ourShader.use();
            glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom),
                                                    (float) fbWidth / (float) fbHeight,
                                                    0.1f, 100.0f);
            glm::mat4 view = camera.GetViewMatrix();
            ourShader.setMat4("projection", projection);
            ourShader.setMat4("view", view);
            ourShader.setMat4("model", modelMat);

            // Освещение (Blinn-Phong + нормал-маппинг). Источник --
            // фиксированное направленное "солнце" сверху-спереди, цвет
            // тёплый белый, плюс мягкий ambient, чтобы тёмные стороны
            // не проваливались в чёрное.
            ourShader.setVec3("uViewPos",     camera.Position);
            ourShader.setVec3("uLightDir",    glm::normalize(glm::vec3(0.5f, 1.0f, 0.7f)));
            ourShader.setVec3("uLightColor",  glm::vec3(1.0f, 0.97f, 0.92f));
            ourShader.setVec3("uAmbient",     glm::vec3(0.18f));

            ourModel->Draw(ourShader);
        } else if (mode == Mode::Slice)
        {
            // Нормаль плоскости: вращаем +Z сначала на yaw(Y), затем на pitch(X).
            glm::mat4 rot = glm::rotate(glm::mat4(1.0f), sliceYaw, glm::vec3(0, 1, 0))
                            * glm::rotate(glm::mat4(1.0f), slicePitch, glm::vec3(1, 0, 0));
            glm::vec3 n     = glm::normalize(glm::vec3(rot * glm::vec4(0, 0, 1, 0)));
            float d         = -(glm::dot(n, center) + sliceOffset);
            glm::vec4 plane = glm::vec4(n, d);

            if (sliceDirty)
            {
                computeSlice(V, F, plane, tmp);
                sliceLines.upload(tmp);
                sliceDirty = false;
            }

            glm::vec3 eye  = center + n * camDist;
            glm::mat4 view = glm::lookAt(eye, center, kWorldUp);
            glm::mat4 proj = glm::ortho(-halfExtent, halfExtent,
                                        -halfExtent, halfExtent,
                                        0.01f, camDist * 4.0f);

            glViewport(0, 0, fbWidth, fbHeight);
            glClearColor(0.97f, 0.97f, 0.95f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            lineShader.use();
            lineShader.setMat4("projection", proj);
            lineShader.setMat4("view", view);
            lineShader.setMat4("model", modelMat);
            lineShader.setVec3("uColor", glm::vec3(0.05f, 0.05f, 0.05f));
            sliceLines.draw();
        } else // Проекция
        {
            // Направление, вдоль которого смотрит камера (от глаза к модели)
            glm::mat4 rot = glm::rotate(glm::mat4(1.0f), silYaw, glm::vec3(0, 1, 0))
                            * glm::rotate(glm::mat4(1.0f), silPitch, glm::vec3(1, 0, 0));
            glm::vec3 dir = glm::normalize(glm::vec3(rot * glm::vec4(0, 0, 1, 0)));

            glm::vec3 eye  = center + dir * camDist;
            glm::mat4 view = glm::lookAt(eye, center, kWorldUp);
            glm::mat4 proj = glm::ortho(-halfExtent, halfExtent,
                                        -halfExtent, halfExtent,
                                        0.01f, camDist * 4.0f);

            // Проход 1: рендерим модель сплошным белым пятном в FBO
            glBindFramebuffer(GL_FRAMEBUFFER, maskFBO.fbo);
            glViewport(0, 0, maskFBO.w, maskFBO.h);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            lineShader.use();
            lineShader.setMat4("projection", proj);
            lineShader.setMat4("view", view);
            lineShader.setMat4("model", modelMat);
            lineShader.setVec3("uColor", glm::vec3(1.0f, 1.0f, 1.0f));
            ourModel->Draw(lineShader);

            // Проход 2: edge-detection маски в стандартный фреймбуфер
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, fbWidth, fbHeight);
            glClearColor(0.97f, 0.97f, 0.95f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glDisable(GL_DEPTH_TEST);
            outlineShader.use();
            outlineShader.setInt("uMask", 0);
            outlineShader.setVec2("uTexel",
                                  1.0f / static_cast<float>(maskFBO.w),
                                  1.0f / static_cast<float>(maskFBO.h));
            outlineShader.setFloat("uRadius", 1.5f);
            outlineShader.setVec3("uOutlineColor", glm::vec3(0.05f, 0.05f, 0.05f));
            outlineShader.setVec3("uBgColor", glm::vec3(0.97f, 0.97f, 0.95f));

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, maskFBO.color);

            glBindVertexArray(dummyVAO);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glBindVertexArray(0);
            glEnable(GL_DEPTH_TEST);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &dummyVAO);
    maskFBO.destroy();

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (mode == Mode::Normal)
    {
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camera.ProcessKeyboard(FORWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camera.ProcessKeyboard(BACKWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camera.ProcessKeyboard(LEFT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camera.ProcessKeyboard(RIGHT, deltaTime);
    } else if (mode == Mode::Slice)
    {
        const float rotSpeed = 1.0f;
        const float movSpeed = 0.6f;
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        {
            sliceYaw   -= rotSpeed * deltaTime;
            sliceDirty = true;
        }
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        {
            sliceYaw   += rotSpeed * deltaTime;
            sliceDirty = true;
        }
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        {
            slicePitch -= rotSpeed * deltaTime;
            sliceDirty = true;
        }
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        {
            slicePitch += rotSpeed * deltaTime;
            sliceDirty = true;
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS)
        {
            sliceOffset -= movSpeed * deltaTime;
            sliceDirty  = true;
        }
        if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS)
        {
            sliceOffset += movSpeed * deltaTime;
            sliceDirty  = true;
        }
        slicePitch = glm::clamp(slicePitch, -kPitchLimit, kPitchLimit);
    } else // projection
    {
        const float rotSpeed = 1.0f;
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            silYaw -= rotSpeed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            silYaw += rotSpeed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            silPitch -= rotSpeed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            silPitch += rotSpeed * deltaTime;
        silPitch = glm::clamp(silPitch, -kPitchLimit, kPitchLimit);
    }
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    fbWidth  = width;
    fbHeight = height;
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow *window, double xposIn, double yposIn)
{
    auto xpos = static_cast<float>(xposIn);
    auto ypos = static_cast<float>(yposIn);

    if (firstMouse)
    {
        lastX      = xpos;
        lastY      = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;

    lastX = xpos;
    lastY = ypos;

    if (mode == Mode::Normal)
        camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
    if (mode == Mode::Normal)
        camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_1)
    {
        mode = Mode::Normal;
        std::cout << "[mode] 3D view\n";
    }
    if (key == GLFW_KEY_2)
    {
        mode       = Mode::Slice;
        sliceDirty = true;
        std::cout << "[mode] slice\n";
    }
    if (key == GLFW_KEY_3)
    {
        mode = Mode::Projection;
        std::cout << "[mode] projection (image-space)\n";
    }
    if (key == GLFW_KEY_O)
    {
        std::string picked = pickModelFile();
        if (!picked.empty()) g_pendingModelPath = picked;
    }
}

void drop_callback(GLFWwindow *window, int count, const char **paths)
{
    // Берём последний из перетянутых файлов.
    if (count > 0 && paths && paths[count - 1])
        g_pendingModelPath = paths[count - 1];
}
