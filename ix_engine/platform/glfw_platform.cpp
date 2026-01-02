// glfw_platform.cpp
#include "common/engine_pch.h"
#include "glfw_platform.h"


namespace ix
{
    GlfwPlatform::GlfwPlatform(
        const WindowSpecification& spec,
        const std::string& title)
    {
        initGLFW(spec, title);
    }

    GlfwPlatform::~GlfwPlatform()
    {

        if (m_window) {
            glfwDestroyWindow(m_window);
            m_window = nullptr;
        }
        glfwTerminate();
    }

    void GlfwPlatform::initGLFW(
        const WindowSpecification& spec,
        const std::string& title)
    {
        if (!glfwInit())
            throw std::runtime_error("Failed to initialize GLFW");

        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        GLFWmonitor* monitor = nullptr;
        int width = static_cast<int>(spec.width);
        int height = static_cast<int>(spec.height);

        switch (spec.mode) {
        case WindowMode::Windowed:
            glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
            monitor = nullptr;
            break;

        case WindowMode::BorderlessFullscreen: {
            glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
            glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

            monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            width = mode->width;
            height = mode->height;
            break;
        }
        case WindowMode::ExclusiveFullscreen: {
            glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

            monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);

            width = mode->width;
            height = mode->height;

            glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
            break;
        }
        }

        m_window = glfwCreateWindow(
            width,
            height,
            title.c_str(),
            monitor,
            nullptr
        );

        if (!m_window)
            throw std::runtime_error("Failed to create GLFW window");

        // Center window if in Windowed mode
        if (spec.mode == WindowMode::Windowed) {
            GLFWmonitor* primary = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(primary);

            int posX = (mode->width - width) / 2;
            int posY = (mode->height - height) / 2;
            glfwSetWindowPos(m_window, posX, posY);
        }

        // Now show the window in its correct position
        glfwShowWindow(m_window);

        m_width = width;
        m_height = height;

        glfwSetWindowUserPointer(m_window, this);
        glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
        glfwSetCursorPosCallback(m_window, cursorPosCallback);
        glfwSetKeyCallback(m_window, keyCallback);

        if (glfwRawMouseMotionSupported()) {
            glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }
    }

    // Window API
    void GlfwPlatform::pollEvents() {
        glfwPollEvents();
    }
    void GlfwPlatform::waitEvents() {
        glfwWaitEvents();
    }

    bool GlfwPlatform::isWindowShouldClose() const {
        return glfwWindowShouldClose(m_window);
    }

    void GlfwPlatform::requestWindowClose() 
    {
        glfwSetWindowShouldClose(m_window, GLFW_TRUE);
    }

    void GlfwPlatform::getFramebufferSize(int& width, int& height) const 
    {
        glfwGetFramebufferSize(m_window, &width, &height);
    }

    void GlfwPlatform::setUserPointer(void* ptr) 
    {
        glfwSetWindowUserPointer(m_window, ptr);
    }

    void* GlfwPlatform::getWindowUserPointer() const 
    {
        return glfwGetWindowUserPointer(m_window);
    }


    // Input API
    static const std::unordered_map<IxKey, int> IX_TO_GLFW =
    {
        { IxKey::W, GLFW_KEY_W },
        { IxKey::A, GLFW_KEY_A },
        { IxKey::S, GLFW_KEY_S },
        { IxKey::D, GLFW_KEY_D },
        { IxKey::E, GLFW_KEY_E },
        { IxKey::Q, GLFW_KEY_Q },
        { IxKey::X, GLFW_KEY_X },
        { IxKey::ESCAPE, GLFW_KEY_ESCAPE },
        { IxKey::SPACE,  GLFW_KEY_SPACE },
        { IxKey::LEFT_SHIFT, GLFW_KEY_LEFT_SHIFT },
        { IxKey::LEFT_CONTROL, GLFW_KEY_LEFT_CONTROL },
        { IxKey::LEFT_ALT, GLFW_KEY_LEFT_ALT }
    };

    int GlfwPlatform::toGlfwKey(IxKey key) const {
        auto it = IX_TO_GLFW.find(key);
        return (it != IX_TO_GLFW.end()) ? it->second : -1;
    }

    IxKey GlfwPlatform::fromGlfwKey(int glfwKey) const 
    {
        for (const auto& [ix, glfw] : IX_TO_GLFW) 
        {
            if (glfw == glfwKey) return ix;
        }
        return static_cast<IxKey>(glfwKey);
    }

    KeyAction GlfwPlatform::translateAction(int action) const 
    {
        if (action == GLFW_PRESS) return KeyAction::PRESS;
        if (action == GLFW_RELEASE) return KeyAction::RELEASE;
        return KeyAction::REPEAT;
    }

    void GlfwPlatform::lockCursor(bool lock) 
    {
        m_cursorLocked = lock;
        glfwSetInputMode(
            m_window,
            GLFW_CURSOR,
            lock ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL
        );
        m_firstMouse = true;
    }

    bool GlfwPlatform::isKeyPressed(IxKey key) const
    {
        int glfwKey = static_cast<int>(key);
        if (glfwKey < 0 || glfwKey >= 512) return false;
        return m_keyStates.test(glfwKey);
    }

    bool GlfwPlatform::isMouseButtonPressed(int button) const 
    {
        return glfwGetMouseButton(m_window, button) == GLFW_PRESS;
    }

    void GlfwPlatform::consumeMouseDelta(double& dx, double& dy) 
    {
        dx = m_accumDX;
        dy = m_accumDY;
        m_accumDX = 0.0;
        m_accumDY = 0.0;
    }

    // Callbacks
    void GlfwPlatform::setKeyCallback(IxKeyCallback callback) 
    {
        m_keyCallback = std::move(callback);
    }

    void GlfwPlatform::framebufferResizeCallback(GLFWwindow* window, int w, int h) 
    {
        auto* self = static_cast<GlfwPlatform*>(glfwGetWindowUserPointer(window));
        if (!self) return;

        self->m_framebufferResized = true;
        self->m_width = static_cast<uint32_t>(w);
        self->m_height = static_cast<uint32_t>(h);
    }

    void GlfwPlatform::cursorPosCallback(GLFWwindow* window, double x, double y) 
    {
        auto* self = static_cast<GlfwPlatform*>(glfwGetWindowUserPointer(window));
        if (!self || !self->m_cursorLocked) return;

        if (self->m_firstMouse) {
            self->m_lastX = x;
            self->m_lastY = y;
            self->m_firstMouse = false;
            return;
        }

        self->m_accumDX += (x - self->m_lastX);
        self->m_accumDY += (y - self->m_lastY);

        self->m_lastX = x;
        self->m_lastY = y;
    }

    void GlfwPlatform::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) 
    {
        auto* self = static_cast<GlfwPlatform*>(glfwGetWindowUserPointer(window));
        if (!self || key < 0 || key >= 512) return;

        // Update bitset via polling
        if (action == GLFW_PRESS) self->m_keyStates.set(key);
        else if (action == GLFW_RELEASE) self->m_keyStates.reset(key);

        // Dispatch to engine callback
        if (self->m_keyCallback) {
            IxKey ixKey = self->fromGlfwKey(key);
            KeyAction keyAction = self->translateAction(action);
            self->m_keyCallback(ixKey, scancode, keyAction, mods);
        }
    }
}