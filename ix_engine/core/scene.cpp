#include "common/engine_pch.h"
#include "scene.h"


namespace ix
{
    Entity Scene::createEntity(const std::string& name)
    {
        // Create the raw EnTT handle
        entt::entity handle = m_registry.create();

        // Wrap it in our Entity handle
        Entity entity(handle, this);

        // Every engine entity should at least have 
        // a name and a transform by default.
        // entity.addComponent<TagComponent>(name);
        // entity.addComponent<TransformComponent>();

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

        /* Example: Logic/Scripting System
        We iterate over all entities that have a 'NativeScript' component.
        EnTT ensures these are contiguous in memory for maximum cache hits.

        auto view = m_registry.view<NativeScriptComponent>();
        for (auto entity : view)
        {
            auto& script = view.get<NativeScriptComponent>(entity);
            if (script.instance)
                script.instance->onUpdate(dt);
        }
        */
    }
}