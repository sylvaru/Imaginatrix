// scene.cpp
#include "common/engine_pch.h"
#include "scene.h"
#include "components.h"
#include "global_common/ix_key_codes.h"


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

    void Scene::update(float dt, const Input_I& input)
    {
        auto view = m_registry.view<TransformComponent, CameraControlComponent>();

        view.each([&](const entt::entity entity, TransformComponent& transform, CameraControlComponent& control) {
            // Rotation
            double dx, dy;
            // Clear the accumulation buffer
            const_cast<Input_I&>(input).consumeMouseDelta(dx, dy);

            if (dx != 0.0 || dy != 0.0) {
                // Yaw
                glm::quat yaw = glm::angleAxis(static_cast<float>(-dx) * control.lookSensitivity, glm::vec3(0, 1, 0));
                // Pitch
                glm::quat pitch = glm::angleAxis(static_cast<float>(-dy) * control.lookSensitivity, glm::vec3(1, 0, 0));

                // Quaternion orientation: Yaw * Current * Pitch
                transform.rotation = yaw * transform.rotation * pitch;
                transform.rotation = glm::normalize(transform.rotation);
            }

            // Translation
            float speedMultiplier = input.isKeyPressed(IxKey::LEFT_SHIFT) ? 2.0f : 1.0f;
            float currentSpeed = control.moveSpeed * speedMultiplier;

            glm::vec3 moveDir{ 0.0f };

            if (input.isCursorLocked() == true)
            {
                if (input.isKeyPressed(IxKey::W)) moveDir += transform.getForward();
                if (input.isKeyPressed(IxKey::S)) moveDir -= transform.getForward();
                if (input.isKeyPressed(IxKey::A)) moveDir -= transform.getRight();
                if (input.isKeyPressed(IxKey::D)) moveDir += transform.getRight();

                if (input.isKeyPressed(IxKey::SPACE)) {
                    moveDir += glm::vec3(0.0f, 1.0f, 0.0f);
                }
                if (input.isKeyPressed(IxKey::LEFT_CONTROL)) {
                    moveDir += glm::vec3(0.0f, -1.0f, 0.0f);
                }
            }

         

            if (glm::length(moveDir) > 0.001f) {
                transform.position += glm::normalize(moveDir) * currentSpeed * dt;
            }
            });
    }
    Scene::CameraMatrices Scene::getActiveCameraMatrices(float aspect) {
        CameraMatrices result{};
        auto view = m_registry.view<TransformComponent, CameraComponent>();

        view.each([&](const auto entity, const auto& transform, const auto& camera) {
            if (camera.primary) {
                result.position = transform.position;

                glm::vec3 forward = transform.rotation * glm::vec3(0, 0, -1);
                glm::vec3 up = transform.rotation * glm::vec3(0, 1, 0);

                result.view = glm::lookAt(transform.position, transform.position + forward, up);

                glm::mat4 proj = glm::perspective(glm::radians(camera.fov), aspect, camera.nearPlane, camera.farPlane);

                result.clusterProjection = proj;

                proj[1][1] *= -1;
                result.projection = proj;
            }
            });

        return result;
    }

}