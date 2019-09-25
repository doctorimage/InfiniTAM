#version 430 core

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_tex_pos;
layout(location = 2) in vec3 in_normal;

out vec3 fs_normal;
out vec2 fs_tex_pos;

uniform mat4 mvpMat;
uniform mat4 normalMat;

void main() {
    gl_Position = mvpMat*vec4(in_position, 1.0f);
    fs_normal = (normalMat*vec4(in_normal, 0.0f)).xyz;
    fs_tex_pos = in_tex_pos;
}