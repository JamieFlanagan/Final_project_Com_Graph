#version 330 core

// Input
layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexColor;
layout(location = 2) in vec2 vertexUV;
layout(location = 3) in vec3 vertexNormal;

// Output data, to be interpolated for each fragment
out vec3 color;
out vec2 uv;
out vec3 FragPos; // Position of the vertex in world space
out vec3 Normal;  // Normal for the vertex
out vec4 FragPosLightSpace;

// Matrices for transformation
uniform mat4 MVP;      // Combined model-view-projection matrix
uniform mat4 model;    // Model matrix
uniform mat4 lightSpaceMatrix;

void main() {
    // Transform vertex
    gl_Position = MVP * vec4(vertexPosition, 1.0);

    // Pass color and UV to the fragment shader
    color = vertexColor;
    uv = vertexUV;

    // Pass fragment position and normal
    FragPos = vec3(model * vec4(vertexPosition, 1.0)); // Convert to world space
    Normal = mat3(transpose(inverse(model))) * vertexNormal; // Transform normal
    FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);
}
