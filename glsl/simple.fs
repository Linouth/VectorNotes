#version 450 core

out vec4 FragColor;

uniform vec4 color = vec4(1.0, 0.5, 0.2, 1.0);

void main() {
    FragColor = color;
}
