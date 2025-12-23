#pragma once
#include "scene.h"

namespace ix 
{

    // The Manager handles the lifecycle: loading from JSON and switching between levels. (owner of scenes)
    class SceneManager 
    {
    public:
        static void load(const std::string& fileName);

        static Scene& getActiveScene() { return *s_instance->s_activeScene; }
        static void init();
        static void shutdown();
        static Scene::CameraMatrices getActiveCameraMatrices(float aspect);
        static void setSceneRoot(const std::string& rootPath) { s_sceneRoot = rootPath; }
    private:
        static std::string s_sceneRoot;
        static std::unique_ptr<Scene> s_activeScene;
        static SceneManager* s_instance;
    };
}