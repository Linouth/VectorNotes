#version 450 core

layout (location = 0) in vec2 aPos;

uniform vec2 view_size;

void main() {
    gl_Position = vec4(aPos.x*(2/view_size.x) - 1, -aPos.y*(2/view_size.y) + 1, 0.0, 1.0);
}
