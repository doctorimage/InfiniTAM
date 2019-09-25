#version 430 core

in vec3 fs_normal;
in vec2 fs_tex_pos;

uniform float sh[9];
uniform sampler2D corresTexture;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_shading;
layout(location = 2) out vec2 out_tex_pos;


float calcShLight(vec3 n) {
    float res = sh[0];
    res += sh[1] * n.z;
    res += sh[2] * n.y;
    res += sh[3] * n.x;
    res += sh[4] * (2 * n.z * n.z - n.x * n.x - n.y * n.y);
    res += sh[5] * n.y * n.z;
    res += sh[6] * n.x * n.z;
    res += sh[7] * n.x * n.y;
    res += sh[8] * (n.x * n.x - n.y * n.y);
    return res;
}

void main() {
    out_color = texture(corresTexture, fs_tex_pos);
    out_tex_pos = fs_tex_pos;
    vec3 albedo = vec3(0.75);
    out_shading = vec4(albedo*calcShLight(fs_normal), 1.0f);
}
