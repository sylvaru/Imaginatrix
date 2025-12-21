#pragma once
#include "scene.h"

namespace ix {

    // The Manager handles the lifecycle: loading from JSON and switching between levels. (owner of scenes)
    class SceneManager {
    public:
        // Load a scene and make it active
        static void load(const std::string& fileName);

        // Access the currently active scene
        static Scene& getActiveScene() { return *s_instance->s_activeScene; }
        static void init();
        static void shutdown();

    private:
        static std::unique_ptr<Scene> s_activeScene;
        static SceneManager* s_instance;
    };
}