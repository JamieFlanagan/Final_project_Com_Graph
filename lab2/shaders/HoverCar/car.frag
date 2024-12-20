#version 330 core
out vec4 fragColor;

uniform vec3 carColor;

void main() {
    fragColor = vec4(carColor, 1.0);
}
