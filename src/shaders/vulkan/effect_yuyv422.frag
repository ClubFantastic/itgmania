#version 450
layout(location = 0) in vec4 v_Color;
layout(location = 1) in vec2 v_TexCoord;
layout(set = 1, binding = 0) uniform sampler2D u_Texture0;
layout(constant_id = 2) const bool TEXTURE_ENABLED = true;
layout(location = 0) out vec4 FragColor;
void main() {
    vec4 yuyv = texture(u_Texture0, v_TexCoord);
    float y, u, v;
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
