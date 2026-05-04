#version 330 core

// Фрагментный шейдер с моделью освещения Blinn-Phong и нормал-маппингом
// в тангент-пространстве. Ambient + diffuse + specular считаются в мире;
// нормаль из карты нормалей переводится из [0,1] в [-1,1] и
// поворачивается базисом TBN в мировое пространство.
//
// Если у модели нет соответствующей карты, флаги uHasNormalMap /
// uHasSpecularMap отключают её использование, и шейдер откатывается
// на вершинную нормаль / постоянный коэффициент бликовости.

in vec2 TexCoords;
in vec3 FragPos;
in mat3 TBN;

out vec4 FragColor;

uniform sampler2D texture_diffuse1;
uniform sampler2D texture_normal1;
uniform sampler2D texture_specular1;

uniform vec3 uViewPos;
uniform vec3 uLightDir;     // направление НА источник света (нормализованное)
uniform vec3 uLightColor;
uniform vec3 uAmbient;

uniform bool uHasNormalMap;
uniform bool uHasSpecularMap;

void main()
{
    vec3 albedo = texture(texture_diffuse1, TexCoords).rgb;

    vec3 N;
    if (uHasNormalMap)
    {
        vec3 tn = texture(texture_normal1, TexCoords).rgb * 2.0 - 1.0;
        N = normalize(TBN * tn);
    }
    else
    {
        N = normalize(TBN[2]);
    }

    vec3 L = normalize(uLightDir);
    vec3 V = normalize(uViewPos - FragPos);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 32.0);

    vec3 specularStrength = uHasSpecularMap
        ? texture(texture_specular1, TexCoords).rgb
        : vec3(0.3);

    vec3 ambient  = uAmbient * albedo;
    vec3 diffuse  = uLightColor * diff * albedo;
    vec3 specular = uLightColor * spec * specularStrength;

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
