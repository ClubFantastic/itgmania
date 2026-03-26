#version 450

layout(location = 0) in vec4 v_Color;
layout(location = 1) in vec2 v_TexCoord;

layout(set = 1, binding = 0) uniform sampler2D u_Texture0;

layout(constant_id = 0) const int TEXTURE_MODE = 0;       // 0=modulate, 1=glow, 2=add
layout(constant_id = 1) const bool ALPHA_TEST = false;
layout(constant_id = 2) const bool TEXTURE_ENABLED = true;

layout(location = 0) out vec4 FragColor;

void main() {
    vec4 texel = TEXTURE_ENABLED ? texture(u_Texture0, v_TexCoord) : vec4(1.0);
    vec4 color;

    if (TEXTURE_MODE == 0)          // Modulate
        color = texel * v_Color;
    else if (TEXTURE_MODE == 1)     // Glow: vertex RGB, texture alpha * vertex alpha
        color = vec4(v_Color.rgb, texel.a * v_Color.a);
    else if (TEXTURE_MODE == 2)     // Add
        color = vec4(texel.rgb + v_Color.rgb, texel.a * v_Color.a);
    else
        color = texel * v_Color;

    if (ALPHA_TEST && color.a < 1.0/256.0)
        discard;

    FragColor = color;
}
