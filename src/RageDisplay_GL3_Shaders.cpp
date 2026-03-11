/* Embedded GLSL 3.30 shader source for RageDisplay_GL3. */

#include "global.h"
#include "RageDisplay_GL3_Shaders.h"

namespace GL3Shaders
{

// ============================================================
// Sprite vertex shader — used for all RageSpriteVertex rendering
// ============================================================
const char *g_SpriteVertSrc = R"GLSL(
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec4 a_Color;
layout(location = 3) in vec2 a_TexCoord;

uniform mat4 u_Projection;
uniform mat4 u_ModelView;
uniform mat4 u_TextureMatrix;

out vec4 v_Color;
out vec2 v_TexCoord;
out vec3 v_Normal;
out vec3 v_Position;

void main() {
    gl_Position = u_Projection * u_ModelView * vec4(a_Position, 1.0);
    // RageVColor is stored as BGRA in memory; swizzle to RGBA
    v_Color = a_Color.zyxw;
    v_TexCoord = (u_TextureMatrix * vec4(a_TexCoord, 0.0, 1.0)).xy;
    v_Normal = mat3(u_ModelView) * a_Normal;
    v_Position = (u_ModelView * vec4(a_Position, 1.0)).xyz;
}
)GLSL";

// ============================================================
// Sprite fragment shader — modulate/glow/add + alpha test
// ============================================================
const char *g_SpriteFragSrc = R"GLSL(
#version 330 core

in vec4 v_Color;
in vec2 v_TexCoord;

uniform sampler2D u_Texture0;
uniform int u_TextureMode;    // 0=modulate, 1=glow, 2=add
uniform bool u_AlphaTestEnabled;
uniform bool u_TextureEnabled;

out vec4 FragColor;

void main() {
    vec4 texel = u_TextureEnabled ? texture(u_Texture0, v_TexCoord) : vec4(1.0);
    vec4 color;

    if (u_TextureMode == 0)          // Modulate
        color = texel * v_Color;
    else if (u_TextureMode == 1)     // Glow: use vertex RGB, texture alpha * vertex alpha
        color = vec4(v_Color.rgb, texel.a * v_Color.a);
    else if (u_TextureMode == 2)     // Add
        color = vec4(texel.rgb + v_Color.rgb, texel.a * v_Color.a);
    else
        color = texel * v_Color;

    if (u_AlphaTestEnabled && color.a < 1.0/256.0)
        discard;

    FragColor = color;
}
)GLSL";

// ============================================================
// Lit sprite vertex shader — same as sprite but passes normal/position for lighting
// ============================================================
const char *g_LitSpriteVertSrc = R"GLSL(
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec4 a_Color;
layout(location = 3) in vec2 a_TexCoord;

uniform mat4 u_Projection;
uniform mat4 u_ModelView;
uniform mat4 u_TextureMatrix;

out vec4 v_Color;
out vec2 v_TexCoord;
out vec3 v_Normal;
out vec3 v_Position;

void main() {
    gl_Position = u_Projection * u_ModelView * vec4(a_Position, 1.0);
    v_Color = a_Color.zyxw;
    v_TexCoord = (u_TextureMatrix * vec4(a_TexCoord, 0.0, 1.0)).xy;
    v_Normal = normalize(mat3(u_ModelView) * a_Normal);
    v_Position = (u_ModelView * vec4(a_Position, 1.0)).xyz;
}
)GLSL";

// ============================================================
// Lit sprite fragment shader — Phong lighting + texture modes
// ============================================================
const char *g_LitSpriteFragSrc = R"GLSL(
#version 330 core

in vec4 v_Color;
in vec2 v_TexCoord;
in vec3 v_Normal;
in vec3 v_Position;

uniform sampler2D u_Texture0;
uniform int u_TextureMode;
uniform bool u_AlphaTestEnabled;
uniform bool u_TextureEnabled;

uniform bool u_LightEnabled;
uniform vec4 u_LightDir;
uniform vec4 u_LightAmbient;
uniform vec4 u_LightDiffuse;
uniform vec4 u_LightSpecular;

uniform vec4 u_MatEmissive;
uniform vec4 u_MatAmbient;
uniform vec4 u_MatDiffuse;
uniform vec4 u_MatSpecular;
uniform float u_MatShininess;

out vec4 FragColor;

void main() {
    vec4 texel = u_TextureEnabled ? texture(u_Texture0, v_TexCoord) : vec4(1.0);

    vec4 baseColor;
    if (u_LightEnabled) {
        vec3 N = normalize(v_Normal);
        vec3 L = normalize(-u_LightDir.xyz);

        float NdotL = max(dot(N, L), 0.0);

        vec3 ambient = u_LightAmbient.rgb * u_MatAmbient.rgb;
        vec3 diffuse = u_LightDiffuse.rgb * u_MatDiffuse.rgb * NdotL;
        vec3 emissive = u_MatEmissive.rgb;

        vec3 specular = vec3(0.0);
        if (NdotL > 0.0 && u_MatShininess > 0.0) {
            vec3 V = normalize(-v_Position);
            vec3 H = normalize(L + V);
            float spec = pow(max(dot(N, H), 0.0), u_MatShininess);
            specular = u_LightSpecular.rgb * u_MatSpecular.rgb * spec;
        }

        baseColor = vec4(emissive + ambient + diffuse + specular, u_MatDiffuse.a);
    } else {
        baseColor = v_Color;
    }

    vec4 color;
    if (u_TextureMode == 0)
        color = texel * baseColor;
    else if (u_TextureMode == 1)
        color = vec4(baseColor.rgb, texel.a * baseColor.a);
    else if (u_TextureMode == 2)
        color = vec4(texel.rgb + baseColor.rgb, texel.a * baseColor.a);
    else
        color = texel * baseColor;

    if (u_AlphaTestEnabled && color.a < 1.0/256.0)
        discard;

    FragColor = color;
}
)GLSL";

// ============================================================
// Effect fragment shaders
// ============================================================

const char *g_EffectFragUnpremultiply = R"GLSL(
#version 330 core
in vec4 v_Color;
in vec2 v_TexCoord;
uniform sampler2D u_Texture0;
uniform bool u_TextureEnabled;
out vec4 FragColor;
void main() {
    vec4 c = u_TextureEnabled ? texture(u_Texture0, v_TexCoord) : vec4(1.0);
    c *= v_Color;
    if (c.a > 0.0)
        c.rgb /= c.a;
    FragColor = c;
}
)GLSL";

const char *g_EffectFragColorBurn = R"GLSL(
#version 330 core
in vec4 v_Color;
in vec2 v_TexCoord;
uniform sampler2D u_Texture0;
uniform bool u_TextureEnabled;
out vec4 FragColor;
void main() {
    vec4 base = v_Color;
    vec4 blend = u_TextureEnabled ? texture(u_Texture0, v_TexCoord) : vec4(1.0);
    vec3 result;
    result.r = (blend.r == 0.0) ? 0.0 : max(0.0, 1.0 - (1.0 - base.r) / blend.r);
    result.g = (blend.g == 0.0) ? 0.0 : max(0.0, 1.0 - (1.0 - base.g) / blend.g);
    result.b = (blend.b == 0.0) ? 0.0 : max(0.0, 1.0 - (1.0 - base.b) / blend.b);
    FragColor = vec4(result, base.a * blend.a);
}
)GLSL";

const char *g_EffectFragColorDodge = R"GLSL(
#version 330 core
in vec4 v_Color;
in vec2 v_TexCoord;
uniform sampler2D u_Texture0;
uniform bool u_TextureEnabled;
out vec4 FragColor;
void main() {
    vec4 base = v_Color;
    vec4 blend = u_TextureEnabled ? texture(u_Texture0, v_TexCoord) : vec4(1.0);
    vec3 result;
    result.r = (blend.r >= 1.0) ? 1.0 : min(1.0, base.r / (1.0 - blend.r));
    result.g = (blend.g >= 1.0) ? 1.0 : min(1.0, base.g / (1.0 - blend.g));
    result.b = (blend.b >= 1.0) ? 1.0 : min(1.0, base.b / (1.0 - blend.b));
    FragColor = vec4(result, base.a * blend.a);
}
)GLSL";

const char *g_EffectFragVividLight = R"GLSL(
#version 330 core
in vec4 v_Color;
in vec2 v_TexCoord;
uniform sampler2D u_Texture0;
uniform bool u_TextureEnabled;
out vec4 FragColor;
float colorBurn(float base, float blend) {
    return (blend == 0.0) ? 0.0 : max(0.0, 1.0 - (1.0 - base) / blend);
}
float colorDodge(float base, float blend) {
    return (blend >= 1.0) ? 1.0 : min(1.0, base / (1.0 - blend));
}
void main() {
    vec4 base = v_Color;
    vec4 blend = u_TextureEnabled ? texture(u_Texture0, v_TexCoord) : vec4(1.0);
    vec3 result;
    result.r = (blend.r <= 0.5) ? colorBurn(base.r, 2.0*blend.r) : colorDodge(base.r, 2.0*(blend.r-0.5));
    result.g = (blend.g <= 0.5) ? colorBurn(base.g, 2.0*blend.g) : colorDodge(base.g, 2.0*(blend.g-0.5));
    result.b = (blend.b <= 0.5) ? colorBurn(base.b, 2.0*blend.b) : colorDodge(base.b, 2.0*(blend.b-0.5));
    FragColor = vec4(result, base.a * blend.a);
}
)GLSL";

const char *g_EffectFragHardMix = R"GLSL(
#version 330 core
in vec4 v_Color;
in vec2 v_TexCoord;
uniform sampler2D u_Texture0;
uniform bool u_TextureEnabled;
out vec4 FragColor;
void main() {
    vec4 base = v_Color;
    vec4 blend = u_TextureEnabled ? texture(u_Texture0, v_TexCoord) : vec4(1.0);
    vec3 result = step(vec3(1.0), base.rgb + blend.rgb);
    FragColor = vec4(result, base.a * blend.a);
}
)GLSL";

const char *g_EffectFragOverlay = R"GLSL(
#version 330 core
in vec4 v_Color;
in vec2 v_TexCoord;
uniform sampler2D u_Texture0;
uniform bool u_TextureEnabled;
out vec4 FragColor;
void main() {
    vec4 base = v_Color;
    vec4 blend = u_TextureEnabled ? texture(u_Texture0, v_TexCoord) : vec4(1.0);
    vec3 result;
    result.r = (base.r < 0.5) ? 2.0*base.r*blend.r : 1.0 - 2.0*(1.0-base.r)*(1.0-blend.r);
    result.g = (base.g < 0.5) ? 2.0*base.g*blend.g : 1.0 - 2.0*(1.0-base.g)*(1.0-blend.g);
    result.b = (base.b < 0.5) ? 2.0*base.b*blend.b : 1.0 - 2.0*(1.0-base.b)*(1.0-blend.b);
    FragColor = vec4(result, base.a * blend.a);
}
)GLSL";

const char *g_EffectFragScreen = R"GLSL(
#version 330 core
in vec4 v_Color;
in vec2 v_TexCoord;
uniform sampler2D u_Texture0;
uniform bool u_TextureEnabled;
out vec4 FragColor;
void main() {
    vec4 base = v_Color;
    vec4 blend = u_TextureEnabled ? texture(u_Texture0, v_TexCoord) : vec4(1.0);
    vec3 result = 1.0 - (1.0 - base.rgb) * (1.0 - blend.rgb);
    FragColor = vec4(result, base.a * blend.a);
}
)GLSL";

const char *g_EffectFragYUYV422 = R"GLSL(
#version 330 core
in vec4 v_Color;
in vec2 v_TexCoord;
uniform sampler2D u_Texture0;
uniform bool u_TextureEnabled;
out vec4 FragColor;
void main() {
    // YUYV422 decoding: the texture contains YUYV packed as RGBA
    vec4 yuyv = texture(u_Texture0, v_TexCoord);
    float y, u, v;
    // Determine if we're on an even or odd pixel
    float texelWidth = 1.0 / textureSize(u_Texture0, 0).x;
    float xInTexels = v_TexCoord.x / texelWidth;
    if (mod(xInTexels, 2.0) < 1.0) {
        y = yuyv.r;
    } else {
        y = yuyv.b;
    }
    u = yuyv.g - 0.5;
    v = yuyv.a - 0.5;
    FragColor = vec4(
        y + 1.402 * v,
        y - 0.344136 * u - 0.714136 * v,
        y + 1.772 * u,
        1.0
    ) * v_Color;
}
)GLSL";

const char *g_EffectFragDistanceField = R"GLSL(
#version 330 core
in vec4 v_Color;
in vec2 v_TexCoord;
uniform sampler2D u_Texture0;
uniform bool u_TextureEnabled;
out vec4 FragColor;
void main() {
    float dist = texture(u_Texture0, v_TexCoord).a;
    float width = fwidth(dist);
    float alpha = smoothstep(0.5 - width, 0.5 + width, dist);
    FragColor = vec4(v_Color.rgb, v_Color.a * alpha);
}
)GLSL";

} // namespace GL3Shaders
