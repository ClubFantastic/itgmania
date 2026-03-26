#version 450
layout(location = 0) in vec4 v_Color;
layout(location = 1) in vec2 v_TexCoord;
layout(set = 1, binding = 0) uniform sampler2D u_Texture0;
layout(constant_id = 2) const bool TEXTURE_ENABLED = true;
layout(location = 0) out vec4 FragColor;
void main() {
    float dist = texture(u_Texture0, v_TexCoord).a;
    float width = fwidth(dist);
    float alpha = smoothstep(0.5 - width, 0.5 + width, dist);
    FragColor = vec4(v_Color.rgb, v_Color.a * alpha);
}
