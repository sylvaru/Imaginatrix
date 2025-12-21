#pragma once
#include "global_common/ix_key_codes.h"
#include <functional>

namespace ix 
{
    enum class WindowMode {
        Windowed,
        BorderlessFullscreen,
        ExclusiveFullscreen
    };

    struct WindowSpecification {
        uint32_t width = 800;
        uint32_t height = 600;
        WindowMode mode = WindowMode::Windowed;
    };

    using IxKeyCallback = std::function<void(IxKey key, int scancode, KeyAction action, int mods)>;

    struct Window_I {
        virtual ~Window_I() = default;
        virtual void* getNativeHandle() const = 0;
        virtual void lockCursor(bool lock) = 0;
        virtual bool isCursorLocked() const = 0;
        virtual void getFramebufferSize(int& width, int& height) const = 0;
        virtual bool isKeyPressed(IxKey key) const = 0;
        virtual bool isMouseButtonPressed(int button) const = 0;
        virtual void setUserPointer(void* ptr) = 0;
        virtual void setKeyCallback(IxKeyCallback callback) = 0;
        virtual bool isWindowShouldClose() const = 0;
        virtual void requestWindowClose() = 0;
        virtual void pollEvents() = 0;
        virtual void* getWindowUserPointer() const = 0;
        virtual void consumeMouseDelta(double& dx, double& dy) = 0;
        virtual bool wasWindowResized() const = 0;
        virtual void resetWindowResizedFlag() = 0;
        virtual void waitEvents() = 0;
    };
}
