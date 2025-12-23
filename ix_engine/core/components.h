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

        MeshComponent() = default;
        MeshComponent(AssetHandle handle) : meshHandle(handle) {}
    };

    // Transform data for the entity
    struct TransformComponent {
        glm::vec3 position{ 0.0f, 0.0f, 0.0f };
        glm::vec3 rotation{ 0.0f, 0.0f, 0.0f };
        glm::vec3 scale{ 1.0f, 1.0f, 1.0f };

        TransformComponent() = default;

        // Compute the model matrix
        glm::mat4 getTransform() const {
            glm::mat4 m = glm::translate(glm::mat4(1.0f), position);
            m = glm::rotate(m, rotation.x, { 1, 0, 0 });
            m = glm::rotate(m, rotation.y, { 0, 1, 0 });
            m = glm::rotate(m, rotation.z, { 0, 0, 1 });
            return glm::scale(m, scale);
        }
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
}