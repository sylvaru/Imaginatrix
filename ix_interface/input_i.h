    // input_i.h
	#pragma once
	#include "global_common/ix_key_codes.h"
	
	namespace ix 
	{
    using IxKeyCallback = std::function<void(IxKey key, int scancode, KeyAction action, int mods)>;

    // Input Interface
    struct Input_I 
	{
        virtual ~Input_I() = default;

        // Keyboard & Mouse State
        virtual bool isKeyPressed(IxKey key) const = 0;
        virtual bool isMouseButtonPressed(int button) const = 0;
        
        // Cursor Management
        virtual void lockCursor(bool lock) = 0;
        virtual bool isCursorLocked() const = 0;
        virtual void consumeMouseDelta(double& dx, double& dy) = 0;

        // Callbacks
        virtual void setKeyCallback(IxKeyCallback callback) = 0;
    };
	} // namespace ix