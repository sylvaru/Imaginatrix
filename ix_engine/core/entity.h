// entity.h
#pragma once
#include <entt/entt.hpp>

namespace ix 
{
    class Scene;

    class Entity {
    public:
        Entity() = default;
        Entity(entt::entity handle, Scene* scene) : m_entityHandle(handle), m_scene(scene) {}

        template<typename T, typename... Args>
        T& addComponent(Args&&... args);

        template<typename T>
        T& getComponent();

        operator entt::entity() const { return m_entityHandle; }
        bool isValid() const { return m_entityHandle != entt::null && m_scene != nullptr; }

    private:
        entt::entity m_entityHandle{ entt::null };
        Scene* m_scene = nullptr;
    };
}

