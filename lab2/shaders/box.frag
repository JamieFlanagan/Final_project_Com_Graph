#version 330 core

in vec3 color;       // Interpolated vertex color
in vec2 uv;          // Interpolated UV coordinates
in vec3 FragPos;     // Fragment position in world space
in vec3 Normal;      // Normal at the fragment
in vec4 FragPosLightSpace; // The fragment position in light space


uniform sampler2D textureSampler; // Texture sampler
uniform bool useTexture;          // Toggle for texture usage
uniform vec3 lightPos;            // Light position
uniform vec3 lightColor;          // Light color
uniform vec3 viewPos;             // Camera/viewer position
uniform sampler2D shadowMap;


out vec3 finalColor;

//function to do the shadow calculations:
float ShadowCalculation(vec4 fragPosLightSpace) {
    // Perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    // Transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;

    // Get closest depth stored in shadow map
    float closestDepth = texture(shadowMap, projCoords.xy).r;

    // Get current fragment depth
    float currentDepth = projCoords.z;

    // Add bias to prevent shadow acne
    float bias = 0.005;

    // Check if fragment is in shadow
    return currentDepth - bias > closestDepth ? 1.0 : 0.0;
}

void main() {
    // Normalize the interpolated normal
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);

    //Adding in some ambient lighting:
    float ambientStrength = 0.2;
    vec3 ambient = ambientStrength * lightColor;

    // Compute the light direction, for Lambertian diffuse shading
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    //Shadow Factor
    float shadow = ShadowCalculation(FragPosLightSpace);

    // Combine texture color with diffuse lighting
    vec3 textureColor = useTexture ? texture(textureSampler, uv).rgb : vec3(1.0);
    vec3 result = (ambient + (1.0 - shadow) * diffuse) * textureColor;

    finalColor = result;
}
