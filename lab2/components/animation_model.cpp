//
// Created by JAMIE on 03/12/2024.
//

#include "animation_model.h"
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <render/shader.h>
#define BUFFER_OFFSET(offset) ((char*)nullptr + (offset))
#include <tiny_gltf.h>

glm::mat4 animationModel::getNodeTransform(const tinygltf::Node& node) {
    glm::mat4 transform(1.0f);

    if (node.matrix.size() == 16) {
        transform = glm::make_mat4(node.matrix.data());
    } else {
        if (node.translation.size() == 3) {
            transform = glm::translate(transform, glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
        }
        if (node.rotation.size() == 4) {
            glm::quat q(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
            transform *= glm::mat4_cast(q);
        }
        if (node.scale.size() == 3) {
            transform = glm::scale(transform, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
        }
    }
    return transform;
}

void animationModel::computeLocalNodeTransform(const tinygltf::Model& model,
        int nodeIndex,
        std::vector<glm::mat4> &localTransforms)
{
    // ---------------------------------------
    // TODO: your code here
    const tinygltf::Node& node = model.nodes[nodeIndex];

    glm::mat4 localMatrix = getNodeTransform(node);
    localTransforms[nodeIndex] = localMatrix;

    for (const int childIndex : node.children) {
        computeLocalNodeTransform(model, childIndex, localTransforms);
    }
    // ---------------------------------------
}

void animationModel::computeGlobalNodeTransform(const tinygltf::Model& model,
        const std::vector<glm::mat4> &localTransforms,
        int nodeIndex, const glm::mat4& parentTransform,
        std::vector<glm::mat4> &globalTransforms)
{
    // ----------------------------------------
    // TODO: your code here
    globalTransforms[nodeIndex] = parentTransform * localTransforms[nodeIndex];

    const tinygltf::Node& node = model.nodes[nodeIndex];

    // Recursively compute the global transforms for child nodes.
    for (int childIndex : node.children) {
        computeGlobalNodeTransform(model, localTransforms, childIndex, globalTransforms[nodeIndex], globalTransforms);
    }
    // ----------------------------------------
}

std::vector<animationModel::SkinObject> animationModel::prepareSkinning(const tinygltf::Model &model) {
		std::vector<SkinObject> skinObjects;

		// In our Blender exporter, the default number of joints that may influence a vertex is set to 4, just for convenient implementation in shaders.
		for (size_t i = 0; i < model.skins.size(); i++) {
			SkinObject skinObject;

			const tinygltf::Skin &skin = model.skins[i];

			// Read inverseBindMatrices
			const tinygltf::Accessor &accessor = model.accessors[skin.inverseBindMatrices];
			assert(accessor.type == TINYGLTF_TYPE_MAT4);
			const tinygltf::BufferView &bufferView = model.bufferViews[accessor.bufferView];
			const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
			const float *ptr = reinterpret_cast<const float *>(
            	buffer.data.data() + accessor.byteOffset + bufferView.byteOffset);

			skinObject.inverseBindMatrices.resize(accessor.count);
			for (size_t j = 0; j < accessor.count; j++) {
				float m[16];
				memcpy(m, ptr + j * 16, 16 * sizeof(float));
				skinObject.inverseBindMatrices[j] = glm::make_mat4(m);
			}

			assert(skin.joints.size() == accessor.count);

			skinObject.globalJointTransforms.resize(skin.joints.size());
			skinObject.jointMatrices.resize(skin.joints.size());

			// ----------------------------------------------
			// TODO: your code here to compute joint matrices
			// ----------------------------------------------

			std::vector<glm::mat4> localTransforms(model.nodes.size(), glm::mat4(1.0f));
			std::vector<glm::mat4> globalTransforms(model.nodes.size(), glm::mat4(1.0f));

			///int root = skin.joints[0];
			int root =24;
			computeLocalNodeTransform(model, root, localTransforms);
			computeGlobalNodeTransform(model, localTransforms, 24, glm::mat4(1.0f), globalTransforms);

			for (size_t j = 0; j < skin.joints.size(); j++) {
				int jointIndex = skin.joints[j];
				skinObject.jointMatrices[j] = globalTransforms[jointIndex] * skinObject.inverseBindMatrices[j];
			}
			// ----------------------------------------------

			skinObjects.push_back(skinObject);
		}
		return skinObjects;
	}

int animationModel::findKeyframeIndex(const std::vector<float>& times, float animationTime)
{
    int left = 0;
    int right = times.size() - 1;

    while (left <= right) {
        int mid = (left + right) / 2;

        if (mid + 1 < times.size() && times[mid] <= animationTime && animationTime < times[mid + 1]) {
            return mid;
        }
        else if (times[mid] > animationTime) {
            right = mid - 1;
        }
        else { // animationTime >= times[mid + 1]
            left = mid + 1;
        }
    }

    // Target not found
    return times.size() - 2;
}

std::vector<animationModel::AnimationObject> animationModel::prepareAnimation(const tinygltf::Model &model)
	{
		std::vector<AnimationObject> animationObjects;
		for (const auto &anim : model.animations) {
			AnimationObject animationObject;

			for (const auto &sampler : anim.samplers) {
				SamplerObject samplerObject;

				const tinygltf::Accessor &inputAccessor = model.accessors[sampler.input];
				const tinygltf::BufferView &inputBufferView = model.bufferViews[inputAccessor.bufferView];
				const tinygltf::Buffer &inputBuffer = model.buffers[inputBufferView.buffer];

				assert(inputAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
				assert(inputAccessor.type == TINYGLTF_TYPE_SCALAR);

				// Input (time) values
				samplerObject.input.resize(inputAccessor.count);

				const unsigned char *inputPtr = &inputBuffer.data[inputBufferView.byteOffset + inputAccessor.byteOffset];
				const float *inputBuf = reinterpret_cast<const float*>(inputPtr);

				// Read input (time) values
				int stride = inputAccessor.ByteStride(inputBufferView);
				for (size_t i = 0; i < inputAccessor.count; ++i) {
					samplerObject.input[i] = *reinterpret_cast<const float*>(inputPtr + i * stride);
				}

				const tinygltf::Accessor &outputAccessor = model.accessors[sampler.output];
				const tinygltf::BufferView &outputBufferView = model.bufferViews[outputAccessor.bufferView];
				const tinygltf::Buffer &outputBuffer = model.buffers[outputBufferView.buffer];

				assert(outputAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

				const unsigned char *outputPtr = &outputBuffer.data[outputBufferView.byteOffset + outputAccessor.byteOffset];
				const float *outputBuf = reinterpret_cast<const float*>(outputPtr);

				int outputStride = outputAccessor.ByteStride(outputBufferView);

				// Output values
				samplerObject.output.resize(outputAccessor.count);

				for (size_t i = 0; i < outputAccessor.count; ++i) {

					if (outputAccessor.type == TINYGLTF_TYPE_VEC3) {
						memcpy(&samplerObject.output[i], outputPtr + i * 3 * sizeof(float), 3 * sizeof(float));
					} else if (outputAccessor.type == TINYGLTF_TYPE_VEC4) {
						memcpy(&samplerObject.output[i], outputPtr + i * 4 * sizeof(float), 4 * sizeof(float));
					} else {
						std::cout << "Unsupport accessor type ..." << std::endl;
					}

				}

				animationObject.samplers.push_back(samplerObject);
			}

			animationObjects.push_back(animationObject);
		}
		return animationObjects;
	}

void animationModel::updateAnimation(
    const tinygltf::Model &model,
    const tinygltf::Animation &anim,
    const AnimationObject &animationObject,
    float time,
    std::vector<glm::mat4> &nodeTransforms)
{
    for (const auto &channel : anim.channels) {
        int targetNodeIndex = channel.target_node;

        // Get sampler for the channel
        const auto &sampler = anim.samplers[channel.sampler];
        const std::vector<float> &times = animationObject.samplers[channel.sampler].input;
        float animationTime = fmod(time, times.back());

        // TODO Find keyframes
        int keyframeIndex = findKeyframeIndex(times, animationTime);
        int nextKeyframeIndex = (keyframeIndex + 1) % times.size();

        // Calculate interpolation factor
        float t0 = times[keyframeIndex];
        float t1 = times[nextKeyframeIndex];
        float factor = (animationTime - t0) / (t1 - t0);

        // Get output buffer data
        const auto &samplerOutput = animationObject.samplers[channel.sampler].output;

        if (channel.target_path == "translation") {
            glm::vec3 translation0, translation1;
            memcpy(&translation0, &samplerOutput[keyframeIndex], sizeof(glm::vec3));
            memcpy(&translation1, &samplerOutput[nextKeyframeIndex], sizeof(glm::vec3));
            glm::vec3 translation = glm::mix(translation0, translation1, factor);
            nodeTransforms[targetNodeIndex] = glm::translate(nodeTransforms[targetNodeIndex], translation);
        }
        else if (channel.target_path == "rotation") {
            glm::quat rotation0, rotation1;
            memcpy(&rotation0, &samplerOutput[keyframeIndex], sizeof(glm::quat));
            memcpy(&rotation1, &samplerOutput[nextKeyframeIndex], sizeof(glm::quat));
            glm::quat rotation = glm::slerp(rotation0, rotation1, factor);
            nodeTransforms[targetNodeIndex] *= glm::mat4_cast(rotation);
        }
        else if (channel.target_path == "scale") {
            glm::vec3 scale0, scale1;
            memcpy(&scale0, &samplerOutput[keyframeIndex], sizeof(glm::vec3));
            memcpy(&scale1, &samplerOutput[nextKeyframeIndex], sizeof(glm::vec3));
            glm::vec3 scale = glm::mix(scale0, scale1, factor);
            nodeTransforms[targetNodeIndex] = glm::scale(nodeTransforms[targetNodeIndex], scale);
        }
    }
}

void animationModel::updateSkinning(const tinygltf::Skin &skin,const std::vector<glm::mat4> &nodeTransforms) {

    // -------------------------------------------------
    // TODO: Recompute joint matrices
    for (SkinObject &skinObject : skinObjects) {
        for (size_t i = 0; i < skinObject.jointMatrices.size(); i++) {
            int jointIndex = skin.joints[i];
            skinObject.jointMatrices[i] = nodeTransforms[jointIndex] * skinObject.inverseBindMatrices[i];
        }
    }
    // -------------------------------------------------
}

void animationModel::update(float time) {

    // -------------------------------------------------
    // TODO: your code here
    if (model.animations.empty() || model.skins.empty()) {
        return;
    }

    // Animation and skin objects
    const tinygltf::Animation &animation = model.animations[0];
    const AnimationObject &animationObject = animationObjects[0];
    const tinygltf::Skin &skin = model.skins[0];

    // Initialize node transforms (identity for all nodes)
    std::vector<glm::mat4> nodeTransforms(model.nodes.size(), glm::mat4(1.0f));

    // Update node transforms with animation data
    updateAnimation(model, animation, animationObject, time, nodeTransforms);

    // Global transforms for all nodes
    std::vector<glm::mat4> globalTransforms(model.nodes.size(), glm::mat4(1.0f));

    // Compute global transforms starting from the root joint
    int rootNodeIndex = skin.joints[0];
    glm::mat4 parentTransform = glm::mat4(1.0f); // Identity matrix for the root's parent
    computeGlobalNodeTransform(model, nodeTransforms, rootNodeIndex, parentTransform, globalTransforms);

    // Update skinning joint matrices
    for (SkinObject &skinObject : skinObjects) {
        for (size_t i = 0; i < skin.joints.size(); ++i) {
            int jointIndex = skin.joints[i];
            skinObject.jointMatrices[i] = globalTransforms[jointIndex] * skinObject.inverseBindMatrices[i];
        }
    }
}

bool animationModel::loadModel(tinygltf::Model &model, const char *filename) {
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool res = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
    if (!warn.empty()) {
        std::cout << "WARN: " << warn << std::endl;
    }

    if (!err.empty()) {
        std::cout << "ERR: " << err << std::endl;
    }

    if (!res)
        std::cout << "Failed to load glTF: " << filename << std::endl;
    else
        std::cout << "Loaded glTF: " << filename << std::endl;

    return res;
}

void animationModel::initialize() {
    lightPosition = glm::vec3(-275.0f, 500.0f, 800.0f);
    lightIntensity = glm::vec3(5e6f, 5e6f, 5e6f);
    if (!loadModel(model, "../lab2/models/bot.gltf")) return;
    primitiveObjects = bindModel(model);
    skinObjects = prepareSkinning(model);
    animationObjects = prepareAnimation(model);

    programID = LoadShadersFromFile("../lab2/shaders/bot.vert", "../lab2/shaders/bot.frag");
    if (programID == 0) std::cerr << "Failed to load shaders." << std::endl;

    mvpMatrixID = glGetUniformLocation(programID, "MVP");
    lightPositionID = glGetUniformLocation(programID, "lightPosition");
    lightIntensityID = glGetUniformLocation(programID, "lightIntensity");
}

void animationModel::bindMesh(std::vector<PrimitiveObject> &primitiveObjects,
              tinygltf::Model &model, tinygltf::Mesh &mesh) {

    std::map<int, GLuint> vbos;
    for (size_t i = 0; i < model.bufferViews.size(); ++i) {
        const tinygltf::BufferView &bufferView = model.bufferViews[i];

        int target = bufferView.target;

        if (bufferView.target == 0) {
            continue;
        }

        const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
        GLuint vbo;
        glGenBuffers(1, &vbo);
        glBindBuffer(target, vbo);
        glBufferData(target, bufferView.byteLength,
                     &buffer.data.at(0) + bufferView.byteOffset, GL_STATIC_DRAW);

        vbos[i] = vbo;
    }

    for (size_t i = 0; i < mesh.primitives.size(); ++i) {

        tinygltf::Primitive primitive = mesh.primitives[i];
        tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];

        GLuint vao;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        for (auto &attrib : primitive.attributes) {
            tinygltf::Accessor accessor = model.accessors[attrib.second];
            int byteStride =
                accessor.ByteStride(model.bufferViews[accessor.bufferView]);
            glBindBuffer(GL_ARRAY_BUFFER, vbos[accessor.bufferView]);

            int size = 1;
            if (accessor.type != TINYGLTF_TYPE_SCALAR) {
                size = accessor.type;
            }

            if (attrib.first.compare("POSITION") == 0) {
                int vaa = 0;
                glEnableVertexAttribArray(vaa);
                glVertexAttribPointer(vaa, size, accessor.componentType,
                                      accessor.normalized ? GL_TRUE : GL_FALSE,
                                      byteStride, BUFFER_OFFSET(accessor.byteOffset));
            } else if (attrib.first.compare("NORMAL") == 0) {
                int vaa = 1;
                glEnableVertexAttribArray(vaa);
                glVertexAttribPointer(vaa, size, accessor.componentType,
                                      accessor.normalized ? GL_TRUE : GL_FALSE,
                                      byteStride, BUFFER_OFFSET(accessor.byteOffset));
            } else if (attrib.first.compare("TEXCOORD_0") == 0) {
                int vaa = 2;
                glEnableVertexAttribArray(vaa);
                glVertexAttribPointer(vaa, size, accessor.componentType,
                                      accessor.normalized ? GL_TRUE : GL_FALSE,
                                      byteStride, BUFFER_OFFSET(accessor.byteOffset));
            } else if (attrib.first.compare("JOINTS_0") == 0) {
                int vaa = 3; // Attribute location for JOINTS_0
                glEnableVertexAttribArray(vaa);
                glVertexAttribIPointer(vaa, size, accessor.componentType,
                                       byteStride, BUFFER_OFFSET(accessor.byteOffset));
            } else if (attrib.first.compare("WEIGHTS_0") == 0) {
                int vaa = 4; // Attribute location for WEIGHTS_0
                glEnableVertexAttribArray(vaa);
                glVertexAttribPointer(vaa, size, accessor.componentType,
                                      accessor.normalized ? GL_TRUE : GL_FALSE,
                                      byteStride, BUFFER_OFFSET(accessor.byteOffset));
            } else {
                std::cout << "Unrecognized attribute: " << attrib.first << std::endl;
            }
        }

        // Record VAO for later use
        PrimitiveObject primitiveObject;
        primitiveObject.vao = vao;
        primitiveObject.vbos = vbos;
        primitiveObjects.push_back(primitiveObject);

        glBindVertexArray(0);
    }
}

void animationModel::bindModelNodes(std::vector<PrimitiveObject> &primitiveObjects,
                        tinygltf::Model &model,
                        tinygltf::Node &node) {
    // Bind buffers for the current mesh at the node
    if ((node.mesh >= 0) && (node.mesh < model.meshes.size())) {
        bindMesh(primitiveObjects, model, model.meshes[node.mesh]);
    }

    // Recursive into children nodes
    for (size_t i = 0; i < node.children.size(); i++) {
        assert((node.children[i] >= 0) && (node.children[i] < model.nodes.size()));
        bindModelNodes(primitiveObjects, model, model.nodes[node.children[i]]);
    }
}
std::vector<animationModel::PrimitiveObject> animationModel::bindModel(tinygltf::Model &model) {
    std::vector<PrimitiveObject> primitiveObjects;

    const tinygltf::Scene &scene = model.scenes[model.defaultScene];
    for (size_t i = 0; i < scene.nodes.size(); ++i) {
        assert((scene.nodes[i] >= 0) && (scene.nodes[i] < model.nodes.size()));
        bindModelNodes(primitiveObjects, model, model.nodes[scene.nodes[i]]);
    }

    return primitiveObjects;
}

void animationModel::drawMesh(const std::vector<PrimitiveObject> &primitiveObjects,
                tinygltf::Model &model, tinygltf::Mesh &mesh) {

    for (size_t i = 0; i < mesh.primitives.size(); ++i)
    {
        GLuint vao = primitiveObjects[i].vao;
        std::map<int, GLuint> vbos = primitiveObjects[i].vbos;

        glBindVertexArray(vao);

        tinygltf::Primitive primitive = mesh.primitives[i];
        tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbos.at(indexAccessor.bufferView));

        glDrawElements(primitive.mode, indexAccessor.count,
                    indexAccessor.componentType,
                    BUFFER_OFFSET(indexAccessor.byteOffset));

        glBindVertexArray(0);
    }
}

void animationModel::drawModelNodes(const std::vector<PrimitiveObject>& primitiveObjects,
                        tinygltf::Model &model, tinygltf::Node &node) {
    // Draw the mesh at the node, and recursively do so for children nodes
    if ((node.mesh >= 0) && (node.mesh < model.meshes.size())) {
        drawMesh(primitiveObjects, model, model.meshes[node.mesh]);
    }
    for (size_t i = 0; i < node.children.size(); i++) {
        drawModelNodes(primitiveObjects, model, model.nodes[node.children[i]]);
    }
}

void animationModel::drawModel(const std::vector<PrimitiveObject>& primitiveObjects,
                tinygltf::Model &model) {
    // Draw all nodes
    const tinygltf::Scene &scene = model.scenes[model.defaultScene];
    for (size_t i = 0; i < scene.nodes.size(); ++i) {
        drawModelNodes(primitiveObjects, model, model.nodes[scene.nodes[i]]);
    }
}

void animationModel::render(glm::mat4 cameraMatrix) {
    glUseProgram(programID);

    // Set camera
    glm::mat4 mvp = cameraMatrix;
    glUniformMatrix4fv(mvpMatrixID, 1, GL_FALSE, &mvp[0][0]);

    // -----------------------------------------------------------------
    // TODO: Set animation data for linear blend skinning in shader
    // -----------------------------------------------------------------
    glUniformMatrix4fv(jointMatricesID, static_cast<GLsizei>(skinObjects[0].jointMatrices.size()), GL_FALSE, glm::value_ptr(skinObjects[0].jointMatrices[0]));

    // -----------------------------------------------------------------

    // Set light data
    glUniform3fv(lightPositionID, 1, &lightPosition[0]);
    glUniform3fv(lightIntensityID, 1, &lightIntensity[0]);

    // Draw the GLTF model
    drawModel(primitiveObjects, model);
}
void animationModel::cleanup() {
    glDeleteProgram(programID);
}


