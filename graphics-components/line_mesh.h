#ifndef TECHNOLOGICALPRACTICE_LINE_MESH_H
#define TECHNOLOGICALPRACTICE_LINE_MESH_H

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <vector>

// Обёртка над VAO/VBO, хранящая поток конечных точек отрезков.
// Каждая последовательная пара (p0, p1) рендерится как один отрезок
// через GL_LINES.
class LineMesh
{
public:
    unsigned int VAO = 0;
    unsigned int VBO = 0;
    GLsizei vertexCount = 0;

    void init()
    {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
        glBindVertexArray(0);
    }

    // Заменить содержимое заданным потоком конечных точек. Длина `segments`
    // должна быть чётной (последовательные пары точек образуют отрезки)
    void upload(const std::vector<glm::vec3> &segments)
    {
        vertexCount = static_cast<GLsizei>(segments.size());
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(segments.size() * sizeof(glm::vec3)),
                     segments.empty() ? nullptr : segments.data(),
                     GL_DYNAMIC_DRAW);
    }

    void draw() const
    {
        if (vertexCount == 0) return;
        glBindVertexArray(VAO);
        glDrawArrays(GL_LINES, 0, vertexCount);
        glBindVertexArray(0);
    }
};

#endif //TECHNOLOGICALPRACTICE_LINE_MESH_H
