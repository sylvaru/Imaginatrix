#pragma once
#include <entt/entt.hpp>
#include <string>
#include "entity.h"
#include "input_i.h"
#include "common/handles.h"


namespace ix 
{

    // The Scene doesn't have much logic. It is a container for entities and components. 
    class Scene
    {
    public:
        Scene() = default;
        ~Scene() = default;

        Entity createEntity(const std::string& name = "Entity");
        void destroyEntity(Entity entity);
        void update(float dt, const Input_I& input);

        template<typename... Components>
        auto getAllEntitiesWith()
        {
            return m_registry.view<Components...>();
        }

        entt::registry& getRegistry() { return m_registry; }
        const entt::registry& getRegistry() const { return m_registry; }

        struct CameraMatrices 
        {
            glm::mat4 view;
            glm::mat4 projection;
            glm::vec3 position;

            const glm::mat4& getView() const { return view; }
            const glm::mat4& getProj() const { return projection; }
            const glm::vec3& getPos() const { return position; }

        };
        CameraMatrices getActiveCameraMatrices(float aspect);

        void setSkybox(TextureHandle handle) { m_skybox = handle; }
        TextureHandle getSkybox() const { return m_skybox; }
        void setSkyboxIntensity(float intensity) { m_skyboxIntensity = intensity; }
        float getSkyboxIntensity() const { return m_skyboxIntensity; }

    private:

        TextureHandle m_skybox = 0;
        float m_skyboxIntensity = 1.0f;
        entt::registry m_registry;
        friend class Entity;
    };

}

#include "entity_methods.inl"