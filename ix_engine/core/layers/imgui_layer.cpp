// imgui_layer.cpp
#include "common/engine_pch.h"
#include "imgui_layer.h"
#include "engine.h"
#include "platform/rendering/vk/vk_renderer.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

namespace ix
{
    void ImGuiLayer::onAttach()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        auto& window = Engine::get().getWindow();
        m_renderer = static_cast<VulkanRenderer*>(&Engine::get().getRenderer());

        ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)window.getNativeHandle(), true);

        m_renderer->setupImGui();
    }

    void ImGuiLayer::onUpdate(float dt)
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowSize(ImVec2(300, 100), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);

        float currentFps = Engine::get().getFPS();
        if (ImGui::Begin("Renderer Stats"))
        {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Performance");
            ImGui::Separator();

            ImGui::Text("FPS:         %.1f", currentFps);
            ImGui::Text("Frame Time:  %.3f ms", 1000.0f / currentFps);

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.0f, 0.7f, 1.0f, 1.0f), "Settings");
            ImGui::Separator();

            bool vsync = m_renderer->isVsyncEnabled();
            if (ImGui::Checkbox("Enable VSync", &vsync))
            {
                m_renderer->setVsync(vsync);
            }

            if (vsync) {
                ImGui::TextWrapped("(Capped to Monitor Refresh Rate)");
            }
            else {
                ImGui::TextWrapped("(Uncapped - Maximum Hardware Usage)");
            }
        }
        ImGui::End();
        ImGui::Render();
    }

}