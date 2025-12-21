// entity_methods.inl
#pragma once
#include "scene.h"

namespace ix 
{

    template<typename T, typename... Args>
    T& Entity::addComponent(Args&&... args) {
        return m_scene->m_registry.emplace<T>(m_entityHandle, std::forward<Args>(args)...);
    }

    template<typename T>
    T& Entity::getComponent() {
        return m_scene->m_registry.get<T>(m_entityHandle);
    }

}