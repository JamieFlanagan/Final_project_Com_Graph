#version 330 core

in vec3 color;    // Vertex color passed from the vertex shader
in vec2 uv;       // UV coordinates passed from the vertex shader

uniform sampler2D textureSampler;
uniform bool useTexture; // New uniform to toggle texture usage



out vec3 finalColor;

void main() {
    if (useTexture) {
        finalColor = texture(textureSampler, uv).rgb; // Use texture color
    } else {
        finalColor = vec3(0.5, 0.5, 0.5);
    }
}
