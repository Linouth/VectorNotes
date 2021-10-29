#version 450 core

layout (location = 0) in vec2 aPos;

uniform vec2 viewSize;

void main() {
    gl_Position = vec4(aPos.x*(2/viewSize.x) - 1, -aPos.y*(2/viewSize.y) + 1, 0.0, 1.0);
}
