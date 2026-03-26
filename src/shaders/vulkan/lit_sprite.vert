#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec4 a_Color;    // VK_FORMAT_B8G8R8A8_UNORM — hardware swizzles BGRA→RGBA
layout(location = 3) in vec2 a_TexCoord;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat4 texMatrix;
} pc;

// ModelView is needed separately for lighting (normal transform + view-space position)
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

layout(location = 0) out vec4 v_Color;
layout(location = 1) out vec2 v_TexCoord;
layout(location = 2) out vec3 v_Normal;
layout(location = 3) out vec3 v_Position;

void main() {
    gl_Position = pc.mvp * vec4(a_Position, 1.0);
    // RageVColor is stored as BGRA in memory; swizzle to RGBA
    v_Color = a_Color.zyxw;
    v_TexCoord = (pc.texMatrix * vec4(a_TexCoord, 0.0, 1.0)).xy;
    v_Normal = normalize(mat3(ubo.modelView) * a_Normal);
    v_Position = (ubo.modelView * vec4(a_Position, 1.0)).xyz;
}
