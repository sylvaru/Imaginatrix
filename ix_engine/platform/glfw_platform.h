// glfw_platform.h
#pragma once
#include <bitset>
#include <string>
#include <GLFW/glfw3.h>

#include "window_i.h"
#include "input_i.h"


namespace ix
{

    class GlfwPlatform : public Window_I, public Input_I
    {
    public:
        GlfwPlatform(const WindowSpecification& spec, const std::string& title);
        ~GlfwPlatform() override;
        // Window
        void pollEvents() override;
        void waitEvents() override;
        bool isWindowShouldClose() const override;
        void requestWindowClose() override;
        void getFramebufferSize(int& width, int& height) const override;

        bool wasWindowResized() const override { return m_framebufferResized; }
        void resetWindowResizedFlag() override { m_framebufferResized = false; }
        void setWindowResizedFlag(bool wasResized) override { m_framebufferResized = wasResized; }

        void setUserPointer(void* ptr) override;
        void* getWindowUserPointer() const override;

        // Native access
        void* getNativeHandle() const override { return m_window; }
        GLFWwindow* nativeGLFW() const { return m_window; }

        // Input
        void lockCursor(bool lock) override;
        bool isCursorLocked() const override { return m_cursorLocked; }
        bool isKeyPressed(IxKey key) const override;
        bool isMouseButtonPressed(int button) const override;
        void consumeMouseDelta(double& dx, double& dy) override;
        void setKeyCallback(IxKeyCallback callback) override;
 

    private:
        void initGLFW(const WindowSpecification& spec, const std::string& title);
        int mapKeyToGLFW(IxKey key) const;
        int toGlfwKey(IxKey key) const;
        IxKey fromGlfwKey(int glfwKey) const;
        KeyAction translateAction(int action) const;

        static void framebufferResizeCallback(GLFWwindow* window, int w, int h);
        static void cursorPosCallback(GLFWwindow* window, double x, double y);
        static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

    private:
        // Window
        GLFWwindow* m_window = nullptr;

        uint32_t m_width = 0;
        uint32_t m_height = 0;
        bool m_framebufferResized = false;
        bool m_cursorLocked = false;

        // Input
        std::bitset<512> m_keyStates;
        double m_lastX = 0.0;
        double m_lastY = 0.0;
        double m_accumDX = 0.0;
        double m_accumDY = 0.0;
        bool m_firstMouse = true;

        IxKeyCallback m_keyCallback;
    };

}



