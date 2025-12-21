#pragma once
#include <entt/entt.hpp>
#include <string>
#include "entity.h"

namespace ix {
    // The Scene doesn't have much logic. It is a container for entities and components. 
    class Scene {
    public:
        Scene() = default;
        ~Scene() = default;

        Entity createEntity(const std::string& name = "Entity");
        void destroyEntity(Entity entity);
        void update(float dt);

        template<typename... Components>
        auto getAllEntitiesWith()
        {
            return m_registry.view<Components...>();
        }

    private:
        entt::registry m_registry;
        friend class Entity;
    };

}

#include "entity_methods.inl"