#version 450

layout(location = 0) in vec4 v_Color;
layout(location = 1) in vec2 v_TexCoord;
layout(location = 2) in vec3 v_Normal;
layout(location = 3) in vec3 v_Position;

layout(set = 0, binding = 0) uniform PerFrameUBO {
    mat4 modelView;
    vec4 lightDir;
    vec4 lightAmbient;
    vec4 lightDiffuse;
    vec4 lightSpecular;
    vec4 matEmissive;
    vec4 matAmbient;
    vec4 matDiffuse;
    vec4 matSpecular;
    float matShininess;
} ubo;

layout(set = 1, binding = 0) uniform sampler2D u_Texture0;

layout(constant_id = 0) const int TEXTURE_MODE = 0;
layout(constant_id = 1) const bool ALPHA_TEST = false;
layout(constant_id = 2) const bool TEXTURE_ENABLED = true;
layout(constant_id = 3) const bool LIGHTING_ENABLED = true;

layout(location = 0) out vec4 FragColor;

void main() {
    vec4 texel = TEXTURE_ENABLED ? texture(u_Texture0, v_TexCoord) : vec4(1.0);

    vec4 baseColor;
    if (LIGHTING_ENABLED) {
        vec3 N = normalize(v_Normal);
        vec3 L = normalize(-ubo.lightDir.xyz);

        float NdotL = max(dot(N, L), 0.0);

        vec3 ambient = ubo.lightAmbient.rgb * ubo.matAmbient.rgb;
        vec3 diffuse = ubo.lightDiffuse.rgb * ubo.matDiffuse.rgb * NdotL;
        vec3 emissive = ubo.matEmissive.rgb;

        vec3 specular = vec3(0.0);
        if (NdotL > 0.0 && ubo.matShininess > 0.0) {
            vec3 V = normalize(-v_Position);
            vec3 H = normalize(L + V);
            float spec = pow(max(dot(N, H), 0.0), ubo.matShininess);
            specular = ubo.lightSpecular.rgb * ubo.matSpecular.rgb * spec;
        }

        baseColor = vec4(emissive + ambient + diffuse + specular, ubo.matDiffuse.a);
    } else {
        baseColor = v_Color;
    }

    vec4 color;
    if (TEXTURE_MODE == 0)
        color = texel * baseColor;
    else if (TEXTURE_MODE == 1)
        color = vec4(baseColor.rgb, texel.a * baseColor.a);
    else if (TEXTURE_MODE == 2)
        color = vec4(texel.rgb + baseColor.rgb, texel.a * baseColor.a);
    else
        color = texel * baseColor;

    if (ALPHA_TEST && color.a < 1.0/256.0)
        discard;

    FragColor = color;
}
