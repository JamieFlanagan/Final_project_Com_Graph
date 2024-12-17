#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <render/shader.h>
#include <stb/stb_image.h>
#include <vector>
#include <iostream>
#define _USE_MATH_DEFINES
#include <algorithm>
#include <math.h>
#include <bits/random.h>
#include <random>
#include <iostream>

#include "components/skyBox.h"
#include "components/drone.h"
#include "components/floor.h"
#include "components/Building.h"
#include "components/animation_model.h"

#include <tiny_gltf.h>

static GLFWwindow *window;
static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode);

// OpenGL camera view parameters
static glm::vec3 eye_center(0.0f, 10.0f, 0.f);
static glm::vec3 lookat(0, 0, 0);
static glm::vec3 up(0, 1, 0);
static glm:: vec3 forward;
static glm::vec3 right;

// View control
static float viewAzimuth = -90.0f; //was 0
static float viewPolar = 0.f;
static float viewDistance = 600.0f;
static float movementSpeed = 2.5f;
static float rotationSpeed = 3.0f;

static GLuint LoadTextureTileBox(const char *texture_file_path) {
    int w, h, channels;
    uint8_t* img = stbi_load(texture_file_path, &w, &h, &channels, 3);
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // To tile textures on a box, we set wrapping to repeat
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    if (img) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, img);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        std::cout << "Failed to load texture " << texture_file_path << std::endl;
    }
    stbi_image_free(img);

    return texture;
}

//Struct that is used to intro to the city, here I used a guiness.
// Different way of making cube as it is a slender cube with a wider cube on top
struct WelcomeSign {
	// For shaders
	GLuint vertexArrayID, vertexBufferID, colorBufferID, normalBufferID, uvBufferID;
    GLuint programID, mvpMatrixID, modelMatrixID, lightSpaceMatrixID, useTexture, textureID;

	// Struct data
    std::vector<float> vertices;
    std::vector<float> colors;
    std::vector<float> normals;
	std::vector<float> uvs;
    glm::vec3 position;

    void initialize(glm::vec3 pos, GLuint texture) {
        position = pos;
    	this->textureID=texture;

        // Create pole (skinny cube)
        float poleWidth = 2.0f;
        float poleHeight = 20.0f;
        float poleDepth = 2.0f;

        // Create sign (larger cube on top)
        float signWidth = 30.0f;
        float signHeight = 15.0f;
        float signDepth = 3.0f;

        // Add pole vertices
        addCube(-poleWidth/2, 0, -poleDepth/2,
                poleWidth, poleHeight, poleDepth);

        // Add sign vertices
        addCube(-signWidth/2, poleHeight, -signDepth/2,
                signWidth, signHeight, signDepth);

    	//Set grey
    	for (int i = 0; i < vertices.size(); i += 3) {
    		colors.push_back(0.7f); // R
    		colors.push_back(0.7f); // G
    		colors.push_back(0.7f); // B
    	}


    	for (int i = 0; i < vertices.size(); i += 9) {
    		// Calculate normal for each triangle
    		glm::vec3 v1(vertices[i], vertices[i+1], vertices[i+2]);
    		glm::vec3 v2(vertices[i+3], vertices[i+4], vertices[i+5]);
    		glm::vec3 v3(vertices[i+6], vertices[i+7], vertices[i+8]);

    		glm::vec3 normal = glm::normalize(glm::cross(v2 - v1, v3 - v1));

    		// Add normal for each vertex of the triangle
    		for (int j = 0; j < 3; j++) {
    			normals.push_back(normal.x);
    			normals.push_back(normal.y);
    			normals.push_back(normal.z);
    		}
    	}

        // Generate and bind VAO
        glGenVertexArrays(1, &vertexArrayID);
        glBindVertexArray(vertexArrayID);

        // Vertex buffer
        glGenBuffers(1, &vertexBufferID);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

        // Color buffer (grey color)
        std::vector<float> colors(vertices.size(), 0.7f);
        glGenBuffers(1, &colorBufferID);
        glBindBuffer(GL_ARRAY_BUFFER, colorBufferID);
        glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(float), colors.data(), GL_STATIC_DRAW);

    	glGenBuffers(1, &uvBufferID);
    	glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
    	glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(float), uvs.data(), GL_STATIC_DRAW);

        // Normal buffer
    	glGenBuffers(1, &normalBufferID);
    	glBindBuffer(GL_ARRAY_BUFFER, normalBufferID);
    	glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(float), normals.data(), GL_STATIC_DRAW);

        // Load shaders and get uniform locations
        programID = LoadShadersFromFile("../lab2/shaders/BillboardShaders/billboard.vert", "../lab2/shaders/BillboardShaders/billboard.frag");
        mvpMatrixID = glGetUniformLocation(programID, "MVP");
        modelMatrixID = glGetUniformLocation(programID, "model");
        lightSpaceMatrixID = glGetUniformLocation(programID, "lightSpaceMatrix");
    }

    void addCube(float x, float y, float z, float width, float height, float depth) {
        // Front face
        addQuad(
            glm::vec3(x, y, z+depth),
            glm::vec3(x+width, y, z+depth),
            glm::vec3(x+width, y+height, z+depth),
            glm::vec3(x, y+height, z+depth),
            glm::vec3(0, 0, 1), width, height
        );

        // Back face
        addQuad(
            glm::vec3(x+width, y, z),
            glm::vec3(x, y, z),
            glm::vec3(x, y+height, z),
            glm::vec3(x+width, y+height, z),
            glm::vec3(0, 0, -1), width, height
        );

        // Top face
        addQuad(
            glm::vec3(x, y+height, z),
            glm::vec3(x+width, y+height, z),
            glm::vec3(x+width, y+height, z+depth),
            glm::vec3(x, y+height, z+depth),
            glm::vec3(0, 1, 0),width, height
        );

        // Bottom face
        addQuad(
            glm::vec3(x, y, z+depth),
            glm::vec3(x+width, y, z+depth),
            glm::vec3(x+width, y, z),
            glm::vec3(x, y, z),
            glm::vec3(0, -1, 0), width, height
        );

        // Right face
        addQuad(
            glm::vec3(x+width, y, z+depth),
            glm::vec3(x+width, y, z),
            glm::vec3(x+width, y+height, z),
            glm::vec3(x+width, y+height, z+depth),
            glm::vec3(1, 0, 0), width, height
        );

        // Left face
        addQuad(
            glm::vec3(x, y, z),
            glm::vec3(x, y, z+depth),
            glm::vec3(x, y+height, z+depth),
            glm::vec3(x, y+height, z),
            glm::vec3(-1, 0, 0), width, height
        );
    }

	void addQuad(glm::vec3 v1, glm::vec3 v2, glm::vec3 v3, glm::vec3 v4, glm::vec3 normal, float faceWidth, float faceHeight) {
    	// Calculate UV coordinates based on which face we're on
    	float uvWidth = 1.0f;  // Full texture width
    	float uvHeight = 1.0f; // Full texture height

    	// For top/bottom faces, adjust UV based on sign's aspect ratio
    	if (normal.y != 0) {  // Top or bottom face
    		uvWidth = 1.0f;   // Full width (30 units)
    		uvHeight = 0.1f;  // Scaled for depth (3 units)
    	}

    	// First triangle
    	vertices.push_back(v1.x); vertices.push_back(v1.y); vertices.push_back(v1.z);
    	vertices.push_back(v2.x); vertices.push_back(v2.y); vertices.push_back(v2.z);
    	vertices.push_back(v3.x); vertices.push_back(v3.y); vertices.push_back(v3.z);

    	uvs.push_back(0.0f);    uvs.push_back(1.0f);   // Top-left
    	uvs.push_back(uvWidth); uvs.push_back(1.0f);     // Top-right
    	uvs.push_back(uvWidth); uvs.push_back(0.0f);     // Bottom-right

    	// Second triangle
    	vertices.push_back(v1.x); vertices.push_back(v1.y); vertices.push_back(v1.z);
    	vertices.push_back(v3.x); vertices.push_back(v3.y); vertices.push_back(v3.z);
    	vertices.push_back(v4.x); vertices.push_back(v4.y); vertices.push_back(v4.z);

    	uvs.push_back(0.0f);    uvs.push_back(1.0f);     // Top-left
    	uvs.push_back(uvWidth); uvs.push_back(0.0f);     // Bottom-right
    	uvs.push_back(0.0f);    uvs.push_back(0.0f); /// Bottom-left

    	// Add normals for lighting
    	for (int i = 0; i < 6; i++) {
    		normals.push_back(normal.x);
    		normals.push_back(normal.y);
    		normals.push_back(normal.z);
    	}
    }

    void render(glm::mat4 cameraMatrix) {
        glUseProgram(programID);

        glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 mvp = cameraMatrix * modelMatrix;


    	glUniform1i(glGetUniformLocation(programID, "useTexture"), GL_TRUE);
        glUniformMatrix4fv(mvpMatrixID, 1, GL_FALSE, &mvp[0][0]);
        glUniformMatrix4fv(modelMatrixID, 1, GL_FALSE, &modelMatrix[0][0]);

    	glActiveTexture(GL_TEXTURE0);
    	glBindTexture(GL_TEXTURE_2D, textureID);
    	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    	glUniform1i(glGetUniformLocation(programID, "textureSampler"), 0);

        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, colorBufferID);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

    	glEnableVertexAttribArray(2); // UV attribute location
    	glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
    	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);

    	glEnableVertexAttribArray(3); // Normal attribute
    	glBindBuffer(GL_ARRAY_BUFFER, normalBufferID);
    	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 0, 0);

        glDrawArrays(GL_TRIANGLES, 0, vertices.size() / 3);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
    	glDisableVertexAttribArray(2);
        glDisableVertexAttribArray(3);
    }

    void cleanup() {
        glDeleteBuffers(1, &vertexBufferID);
        glDeleteBuffers(1, &colorBufferID);
        glDeleteBuffers(1, &normalBufferID);
        glDeleteVertexArrays(1, &vertexArrayID);
        glDeleteProgram(programID);
    }
};


//Shadow Map
struct ShadowMap {
	GLuint depthMapFBO;
	GLuint depthMap;
	const unsigned int SHADOW_WIDTH = 2048;
	const unsigned int SHADOW_HEIGHT = 2048;

	void initialize() {
		// Create framebuffer
		glGenFramebuffers(1, &depthMapFBO);

		// Create depth texture
		glGenTextures(1, &depthMap);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
			SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

		// Attach depth texture to framebuffer
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
};


//Sphere
struct Sphere {
    GLuint vertexArrayID, vertexBufferID, indexBufferID, colorBufferID, uvBufferID, programID;
    GLuint textureID;
    GLuint mvpMatrixID, textureSamplerID, useTextureID;
    std::vector<float> vertices, colors, uvs;
    std::vector<unsigned int> indices;

	glm::vec3 position;  // Sphere position

	//Movement for the sphere to make it like a drone
	float time =0.0f;
	float radius=100.0f;
	float height = 300.0f;
	float speed = 1.0f;
	float base;

    void generateSphere(float radius, int sectorCount, int stackCount) {
        float x, y, z, xy;                             // Vertex positions
        float nx, ny, nz, lengthInv = 1.0f / radius;  // Normals
        float s, t;                                   // UV coordinates

        float sectorStep = 2 * M_PI / sectorCount;
        float stackStep = M_PI / stackCount;
        float sectorAngle, stackAngle;

        // Generate vertices, normals, and UVs
        for (int i = 0; i <= stackCount; ++i) {
            stackAngle = M_PI / 2 - i * stackStep;        // From π/2 to -π/2
            xy = radius * cosf(stackAngle);              // r * cos(u)
            z = radius * sinf(stackAngle);               // r * sin(u)

            // Add (sectorCount+1) vertices per stack
            for (int j = 0; j <= sectorCount; ++j) {
                sectorAngle = j * sectorStep;            // From 0 to 2π

                // Vertex position
                x = xy * cosf(sectorAngle);              // r * cos(u) * cos(v)
                y = xy * sinf(sectorAngle);              // r * cos(u) * sin(v)
                vertices.push_back(x);
                vertices.push_back(y);
                vertices.push_back(z);

                // UV coordinates
                s = (float)j / sectorCount;
                t = (float)i / stackCount;
                uvs.push_back(s);
                uvs.push_back(t);

                // Vertex colors (blue)
                colors.push_back(0.0f);  // Red
                colors.push_back(0.0f);  // Green
                colors.push_back(1.0f);  // Blue
            }
        }

        // Generate indices
        for (int i = 0; i < stackCount; ++i) {
            int k1 = i * (sectorCount + 1);     // Beginning of current stack
            int k2 = k1 + sectorCount + 1;     // Beginning of next stack

            for (int j = 0; j < sectorCount; ++j, ++k1, ++k2) {
                // Two triangles per sector
                if (i != 0) {
                    indices.push_back(k1);
                    indices.push_back(k2);
                    indices.push_back(k1 + 1);
                }
                if (i != (stackCount - 1)) {
                    indices.push_back(k1 + 1);
                    indices.push_back(k2);
                    indices.push_back(k2 + 1);
                }
            }
        }
    }

    void initialize(glm::vec3 position,float radius, int sectorCount, int stackCount, GLuint textureID) {
		this->position=position;
    	this->height = position.y;
    	this->radius = radius*2.0f;
    	this->base = position.y;
    	this->textureID = textureID;

        // Generate sphere geometry
        generateSphere(radius, sectorCount, stackCount);

        // Generate and bind VAO
        glGenVertexArrays(1, &vertexArrayID);
        glBindVertexArray(vertexArrayID);

        // Vertex buffer
        glGenBuffers(1, &vertexBufferID);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

        // Color buffer
        glGenBuffers(1, &colorBufferID);
        glBindBuffer(GL_ARRAY_BUFFER, colorBufferID);
        glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(float), colors.data(), GL_STATIC_DRAW);

        // UV buffer
        glGenBuffers(1, &uvBufferID);
        glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
        glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(float), uvs.data(), GL_STATIC_DRAW);

        // Index buffer
        glGenBuffers(1, &indexBufferID);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        // Load and compile shaders
        programID = LoadShadersFromFile("../lab2/shaders/box.vert", "../lab2/shaders/box.frag");
        mvpMatrixID = glGetUniformLocation(programID, "MVP");
        textureSamplerID = glGetUniformLocation(programID, "textureSampler");
    	useTextureID = glGetUniformLocation(programID, "useTexture");

    }

    void render(glm::mat4 cameraMatrix) {
        glUseProgram(programID);

        // Pass the MVP matrix
    	glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), position);
    	glm::mat4 mvp = cameraMatrix * modelMatrix;
        glUniformMatrix4fv(mvpMatrixID, 1, GL_FALSE, &mvp[0][0]);

    	// Set 'useTexture' to false for the sphere
    	glUniform1i(useTextureID, GL_FALSE);

        // Enable vertex attributes
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, colorBufferID);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

        glEnableVertexAttribArray(2);
        glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);

        // Bind the texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glUniform1i(textureSamplerID, 0);

        // Draw sphere using indices
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);

        // Disable attributes
        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
    }

	void updatePos(float deltaTime) {
    	float baseX = radius * cos(time * speed);
    	float baseZ = radius * sin(time * speed);

    	// fogure 8 patter
    	float figure8X = 30.0f * sin(2 * time * speed);
    	float figure8Z = 30.0f * sin(time * speed);

    	//combine these movemnets
    	position.x = baseX + figure8X;
    	position.z = baseZ + figure8Z;

    	position.y = height + 20.0f * sin(time * 1.5f);

    	time+=deltaTime;
    }
};


int main(void)
{
	// Initialise GLFW
	if (!glfwInit())
	{
		std::cerr << "Failed to initialize GLFW." << std::endl;
		return -1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // For MacOS
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Open a window and create its OpenGL context
	window = glfwCreateWindow(1024, 768, "Lab 2", NULL, NULL);
	if (window == NULL)
	{
		std::cerr << "Failed to open a GLFW window." << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSetKeyCallback(window, key_callback);

	// Load OpenGL functions, gladLoadGL returns the loaded version, 0 on error.
	int version = gladLoadGL(glfwGetProcAddress);
	if (version == 0)
	{
		std::cerr << "Failed to initialize OpenGL context." << std::endl;
		return -1;
	}

	// Background
	glClearColor(0.2f, 0.2f, 0.25f, 0.0f);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	//The ground
	Floor floor;
	GLuint floorTexture = LoadTextureTileBox("../lab2/cityGround.jpg");
	floor.initialize(floorTexture);

	GLuint buildingTexture = LoadTextureTileBox("../lab2/nightCity.jpg");

	//My buildings
	int rows =7;
	int cols = 7;
	float spacing =65.0f;

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<> height_dist(30.0f, 150.0f);
	std::uniform_real_distribution<> offset_dist(-5.0f, 5.0f);
	//std::uniform_int_distribution<> texture_dist(0, textures.size() - 1);
	std::vector<Building> buildings;


	for (int i = 0; i < rows; ++i) {
		for (int j = 0; j < cols; ++j) {
			Building b;

			// Randomize position with slight offset
			float x = i * spacing + offset_dist(gen);
			float z = j * spacing + offset_dist(gen);
			glm::vec3 position = glm::vec3(x, 0, z);

			// Randomize height while keeping width and depth constant
			float height = height_dist(gen);
			glm::vec3 scale = glm::vec3(16.0f, height, 16.0f);
			//GLuint random_texture = textures[texture_dist(gen)];
			position.y=height;
			b.initialize(position, scale, buildingTexture);
			buildings.push_back(b);
		}
	}

	//Drone creation stemming from sphere Idea
	Drone drone;
	GLuint guinessTexture = LoadTextureTileBox("../lab2/guinessAddFix.jpg");

	drone.initialize(glm::vec3(100.0f, 300.0f, 60.0f), guinessTexture);

	//My Sphere ----------------
	// TBE drones
	Sphere sphere;
	glm::vec3 spherePosition = glm::vec3(100.0f, 300.0f, 60.0f);  // Position in the air
	float sphereRadius = 25.0f;                               // Sphere radius
	sphere.initialize(spherePosition,sphereRadius, 36, 18, 0);

	//sphere movement
	float lastFrame =0.0f;
	float currentFrame;

	//SkyBox
	glm::vec3 cityCenterSky = glm::vec3((rows - 1) * spacing / 2.0f, 0, (cols - 1) * spacing / 2.0f);
	SkyBox skybox;
	skybox.initialize(cityCenterSky, glm::vec3(rows * spacing, rows * spacing, rows * spacing), "../lab2/sky.png");

	animationModel bot;
	bot.initialize();

	//Billboard sign
	WelcomeSign sign;
	glm::vec3 signPosition = glm::vec3(0.0f, 0.0f, -200.0f);

	sign.initialize(signPosition, guinessTexture);



	glm::vec3 lightPos = cityCenterSky + glm::vec3(200.0f, 400.0f, 200.0f);
	glm::vec3 lightColor(1.0f, 1.0f, 1.0f);     // Green light
	glm::vec3 viewPos = eye_center;              // Camera position

	// Camera setup
	forward = glm::normalize(lookat - eye_center);
	right = glm::normalize(glm::cross(forward, up));
	glm::mat4 viewMatrix, projectionMatrix;
    glm::float32 FoV = 100.0f;
	glm::float32 zNear = 0.1f;
	glm::float32 zFar = 3000.0f;
	projectionMatrix = glm::perspective(glm::radians(FoV), 4.0f / 3.0f, zNear, zFar);

	ShadowMap shadowMap;
	shadowMap.initialize();

	glm::mat4 lightProjection = glm::ortho(-800.0f, 800.0f, -800.0f, 800.0f, 1.0f, 1000.0f);
	glm::mat4 lightView = glm::lookAt(lightPos, cityCenterSky, glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 lightSpaceMatrix = lightProjection * lightView;

	float time = 0.0f;

	do
	{
		//For sphere movement get delta time
		float currentTime = glfwGetTime();
		float deltaTime = currentTime - time;
		time = currentTime;
		//character update
		bot.update(time);

	// Shadow pass
		glViewport(0, 0, shadowMap.SHADOW_WIDTH, shadowMap.SHADOW_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, shadowMap.depthMapFBO);
		glClear(GL_DEPTH_BUFFER_BIT);
		glCullFace(GL_FRONT);
		for (auto& building : buildings) {
			building.renderDepth(lightSpaceMatrix);
		}
		floor.renderDepth(lightSpaceMatrix);

		drone.updatePosition(deltaTime);

		//Render normally
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, 1024, 768);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glCullFace(GL_BACK);

		viewMatrix = glm::lookAt(eye_center, lookat, up);
		glm::mat4 vp = projectionMatrix * viewMatrix;

		glDisable(GL_DEPTH_TEST);
		skybox.render(vp);
		glEnable(GL_DEPTH_TEST);

		floor.render(vp, lightPos, lightColor, eye_center, shadowMap.depthMap, lightSpaceMatrix);

		for (auto& building : buildings) {
			building.render(vp, lightPos, lightColor, eye_center, shadowMap.depthMap, lightSpaceMatrix);
		}

		bot.render(vp);

		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		drone.render(vp, lightPos, lightColor, eye_center, shadowMap.depthMap, lightSpaceMatrix);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		sign.render(vp);

		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();

	} // Check if the ESC key was pressed or the window was closed
	while (!glfwWindowShouldClose(window));


	for (auto& building : buildings) {
		building.cleanup();
	}

	floor.cleanup();
	sign.cleanup();
	skybox.cleanup();
	bot.cleanup();

	//Clear the shadow resources
	glDeleteFramebuffers(1, &shadowMap.depthMapFBO);
	glDeleteTextures(1, &shadowMap.depthMap);
	// Close OpenGL window and terminate GLFW
	glfwTerminate();

	return 0;
}

// Is called whenever a key is pressed/released via GLFW
static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode) {
    // Reset the camera position and orientation
    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        viewAzimuth = -90.0f;
        viewPolar = 0.0f;
        eye_center = glm::vec3(0.0f, 10.0f, 0.0f);
        lookat = glm::vec3(0.0f, 0.0f, 0.0f);
        std::cout << "Camera reset." << std::endl;
    }

    // Movement controls
    if (key == GLFW_KEY_W && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        eye_center += forward * movementSpeed;
        lookat += forward * movementSpeed;
    }
    if (key == GLFW_KEY_S && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        eye_center -= forward * movementSpeed;
        lookat -= forward * movementSpeed;
    }
    if (key == GLFW_KEY_A && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        eye_center -= right * movementSpeed;
        lookat -= right * movementSpeed;
    }
    if (key == GLFW_KEY_D && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        eye_center += right * movementSpeed;
        lookat += right * movementSpeed;
    }

    // Rotation controls
    if (key == GLFW_KEY_DOWN && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        viewPolar -= glm::radians(rotationSpeed);
        if (viewPolar < -glm::radians(89.0f)) viewPolar = -glm::radians(89.0f); // Prevent looking too far up
    }
    if (key == GLFW_KEY_UP && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        viewPolar += glm::radians(rotationSpeed);
        if (viewPolar > glm::radians(89.0f)) viewPolar = glm::radians(89.0f); // Prevent looking too far down
    }
    if (key == GLFW_KEY_LEFT && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        viewAzimuth -= glm::radians(rotationSpeed);
    }
    if (key == GLFW_KEY_RIGHT && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        viewAzimuth += glm::radians(rotationSpeed);
    }

    // Update lookat direction based on new azimuth and polar angles
    lookat.x = eye_center.x + cos(viewPolar) * cos(viewAzimuth);
    lookat.y = eye_center.y + sin(viewPolar);
    lookat.z = eye_center.z + cos(viewPolar) * sin(viewAzimuth);

    // Update forward and right vectors
    forward = glm::normalize(lookat - eye_center);
    right = glm::normalize(glm::cross(forward, up));

    // Exit application
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }
}
