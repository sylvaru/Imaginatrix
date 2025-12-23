// scene.cpp
#include "common/engine_pch.h"
#include "scene.h"
#include "components.h"

namespace ix
{
    Entity Scene::createEntity(const std::string& name)
    {
        // Create the raw EnTT handle
        entt::entity handle = m_registry.create();

        // Wrap it in the Entity handle
        Entity entity(handle, this);


        spdlog::trace("Created entity: {} ({})", name, (uint32_t)handle);
        return entity;
    }

    void Scene::destroyEntity(Entity entity)
    {
        m_registry.destroy(entity);
    }

    void Scene::update(float dt)
    {
        // Data oriented systems will live here
    }
    Scene::CameraMatrices Scene::getActiveCameraMatrices(float aspect) {
        auto view = m_registry.view<TransformComponent, CameraComponent>();

        CameraMatrices result{ glm::mat4(1.0f), glm::mat4(1.0f), glm::vec3(0.0f) };
        bool found = false;

        // .each() is the safest and most performant way to iterate EnTT views
        view.each([&](const auto entity, auto& transform, auto& camera) {
            if (!found && camera.primary) 
            {
                result.position = transform.position;

                // Inverse of the camera's world transform is the View Matrix
                result.view = glm::inverse(transform.getTransform());

                result.projection = glm::perspective(
                    glm::radians(camera.fov),
                    aspect,
                    camera.nearPlane,
                    camera.farPlane
                );

                // Vulkan Y-axis flip
                result.projection[1][1] *= -1;

                found = true;
            }
            });

        return result;
    }

}