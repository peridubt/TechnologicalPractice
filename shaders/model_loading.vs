#version 330 core

// Стандартный набор атрибутов из Vertex (см. mesh.h):
//   0 -- позиция, 1 -- нормаль, 2 -- UV, 3 -- касательная, 4 -- битангент.
// Для нормал-маппинга помимо позиции и UV пробрасываем во фрагментный
// шейдер мировую точку и базис тангент-пространства TBN.
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 TexCoords;
out vec3 FragPos;
out mat3 TBN;

void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);
    FragPos       = worldPos.xyz;
    TexCoords     = aTexCoords;

    // Нормальная матрица -- transpose(inverse(model)). Для модели без
    // неравномерного масштаба это эквивалентно mat3(model).
    mat3 N = transpose(inverse(mat3(model)));
    vec3 T = normalize(N * aTangent);
    vec3 B = normalize(N * aBitangent);
    vec3 Nv = normalize(N * aNormal);
    TBN = mat3(T, B, Nv);

    gl_Position = projection * view * worldPos;
}
