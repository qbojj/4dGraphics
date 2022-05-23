#pragma once

#include "GameCore.h"

#include <bitset>

class GameInputHandler : public InputHandler
{
public:
	template< int siz >
	struct buttons
	{
		void set( int p, bool v = true ) { curr[p] = v; ( v ? pressed : released )[p] = true; }
		void moveNext() { pressed.reset(); released.reset(); }
		std::bitset<siz> curr, pressed, released;
	};

	buttons<GLFW_KEY_LAST + 1> Keyboard;
	buttons<GLFW_MOUSE_BUTTON_LAST + 1 > MouseButtons;

	glm::vec2 WndSize;
	glm::vec2 MousePos, MousePosStart;
	bool MouseInside;

	void onResize( GLFWwindow *window, int width, int height );
	void onKey( GLFWwindow *window, int key, int scancode, int action, int mods );
	void onMouseButton( GLFWwindow *window, int button, int action, int mods );
	void onMouseMove( GLFWwindow *window, double xpos, double ypos );
	void onMouseEnter( GLFWwindow *window, int entered );

protected:
	virtual bool OnCreate( GLFWwindow *window );
	virtual void OnPreTick();
	virtual void OnPostTick();
	virtual void OnDestroy( GLFWwindow *window );
};