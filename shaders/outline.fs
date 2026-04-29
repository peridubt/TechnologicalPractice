#version 330 core

// Контур проекции в пространстве.
//
// Цвет контура выдаётся там, где статус "покрыт/не покрыт" меняется
// между центральным пикселем и любым из четырёх семплируемых соседей.
// Расстояние до соседей задаётся `uRadius` в пикселях - это и есть
// толщина итоговой линии.

in  vec2 vUV;
out vec4 FragColor;

uniform sampler2D uMask;
uniform vec2  uTexel;       // 1.0 / размер_фреймбуфера
uniform float uRadius;      // толщина контура в пикселях (>= 1.0)
uniform vec3  uOutlineColor;
uniform vec3  uBgColor;

float covered(vec2 uv)
{
    return step(0.5, texture(uMask, uv).r);
}

void main()
{
    vec2 d = uTexel * uRadius;

    float c = covered(vUV);
    float l = covered(vUV + vec2(-d.x, 0.0));
    float r = covered(vUV + vec2( d.x, 0.0));
    float u = covered(vUV + vec2( 0.0,  d.y));
    float v = covered(vUV + vec2( 0.0, -d.y));

    // если в окрестности есть и покрытые, и непокрытые пиксели
    // (переход между ними), иначе 0.
    float maxN = max(max(l, r), max(max(u, v), c));
    float minN = min(min(l, r), min(min(u, v), c));
    float edge = maxN - minN;

    FragColor = vec4(mix(uBgColor, uOutlineColor, edge), 1.0);
}
