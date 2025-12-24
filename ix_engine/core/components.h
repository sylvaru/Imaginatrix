// components.h
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <string>
#include "common/handles.h"

namespace ix 
{
    struct MeshComponent 
    {
        AssetHandle meshHandle = 0;
        TextureHandle textureHandle = 0;

        MeshComponent() = default;
        MeshComponent(AssetHandle handle) : meshHandle(handle) {}
        MeshComponent(AssetHandle handle, TextureHandle tex) : meshHandle(handle), textureHandle(tex) {}
    };

    // Transform data for the entity
    struct TransformComponent 
    {
        glm::vec3 position{ 0.0f, 0.0f, 0.0f };
        glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
        glm::vec3 scale{ 1.0f, 1.0f, 1.0f };

        TransformComponent() = default;

        glm::mat4 getTransform() const {
            return glm::translate(glm::mat4(1.0f), position) * glm::toMat4(rotation) * glm::scale(glm::mat4(1.0f), scale);
        }

        glm::vec3 getForward() const { return rotation * glm::vec3(0, 0, -1); }
        glm::vec3 getRight()   const { return rotation * glm::vec3(1, 0, 0); }
        glm::vec3 getUp()      const { return rotation * glm::vec3(0, 1, 0); }
    };

    // Tag component for the Scene hierarchy
    struct TagComponent {
        std::string tag;

        TagComponent() = default;
        TagComponent(const std::string& name) : tag(name) {}
    };

    struct CameraComponent 
    {
        float fov = 75.0f;
        float nearPlane = 0.1f;
        float farPlane = 1000.0f;
        bool primary = true; // Flag to identify which camera to render from
    };

    struct CameraControlComponent
    {
        float moveSpeed = 5.0f;
        float lookSensitivity = 0.1f;
    };
}