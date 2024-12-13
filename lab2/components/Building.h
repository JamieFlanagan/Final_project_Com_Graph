//
// Created by JAMIE on 13/12/2024.
//

#ifndef BUILDING_H
#define BUILDING_H

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Building {
public:
    glm::vec3 position;		// Position of the box
    glm::vec3 scale;		// Size of the box in each axis
    GLuint vertexArrayID;
    GLuint vertexBufferID;
    GLuint indexBufferID;
    GLuint colorBufferID;
    GLuint uvBufferID;
    GLuint textureID;
    GLuint normalBufferID;
    GLuint mvpMatrixID;
    GLuint textureSamplerID;
    GLuint programID, useTextureID, shadowMapID;
    GLuint lightSpaceMatrixID;
    GLuint shadowMapSamplerID;

    void initialize(glm::vec3 position, glm::vec3 scale, GLuint textureID);
    void render(glm::mat4 cameraMatrix, glm::vec3 lightPos, glm::vec3 lightColor, glm::vec3 viewPos, GLuint shadowMap, const glm::mat4& lightSpaceMatrix);
    void renderDepth(const glm::mat4& lightSpaceMatrix);
    void cleanup();

};



#endif //BUILDING_H
