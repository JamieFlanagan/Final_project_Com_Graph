#version 330 core

in vec3 color;       // Interpolated vertex color
in vec2 uv;          // Interpolated UV coordinates
in vec3 FragPos;     // Fragment position in world space
in vec3 Normal;      // Normal at the fragment

uniform sampler2D textureSampler; // Texture sampler
uniform bool useTexture;          // Toggle for texture usage
uniform vec3 lightPos;            // Light position
uniform vec3 lightColor;          // Light color
uniform vec3 viewPos;             // Camera/viewer position

out vec3 finalColor;

void main() {
    // Normalize the interpolated normal
    vec3 norm = normalize(Normal);

    // Compute the light direction
    vec3 lightDir = normalize(lightPos - FragPos);

    // Lambertian diffuse shading
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Combine texture color with diffuse lighting
    vec3 textureColor = useTexture ? texture(textureSampler, uv).rgb : vec3(1.0);
    vec3 result = diffuse * textureColor;

    finalColor = result;
}
