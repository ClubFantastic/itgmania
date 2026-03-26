#version 450
layout(location = 0) in vec4 v_Color;
layout(location = 1) in vec2 v_TexCoord;
layout(set = 1, binding = 0) uniform sampler2D u_Texture0;
layout(constant_id = 2) const bool TEXTURE_ENABLED = true;
layout(location = 0) out vec4 FragColor;
float colorBurn(float base, float blend) {
    return (blend == 0.0) ? 0.0 : max(0.0, 1.0 - (1.0 - base) / blend);
}
float colorDodge(float base, float blend) {
    return (blend >= 1.0) ? 1.0 : min(1.0, base / (1.0 - blend));
}
void main() {
    vec4 base = v_Color;
    vec4 blend = TEXTURE_ENABLED ? texture(u_Texture0, v_TexCoord) : vec4(1.0);
    vec3 result;
    result.r = (blend.r <= 0.5) ? colorBurn(base.r, 2.0*blend.r) : colorDodge(base.r, 2.0*(blend.r-0.5));
    result.g = (blend.g <= 0.5) ? colorBurn(base.g, 2.0*blend.g) : colorDodge(base.g, 2.0*(blend.g-0.5));
    result.b = (blend.b <= 0.5) ? colorBurn(base.b, 2.0*blend.b) : colorDodge(base.b, 2.0*(blend.b-0.5));
    FragColor = vec4(result, base.a * blend.a);
}
