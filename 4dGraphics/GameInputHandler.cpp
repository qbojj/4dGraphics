#include "stdafx.h"
#include "GameInputHandler.h"

#include <functional>
#include <iostream>
using namespace std;

#include <imgui_impl_glfw.h>

// try to guess Args by type of function ptr that must be returned 
// (and assert Args are the same as member function's args)
template<typename F, F f, typename... Args, typename=enable_if_t<is_same_v<remove_cvref_t<F>, void(GameInputHandler:: *)(GLFWwindow *, Args...)>> >
void nonMember_helper( GLFWwindow *wnd, Args... args )
{
	(((GameInputHandler *)glfwGetWindowUserPointer( wnd ))->*f)(wnd, args...);
}
#define NON_MEMBER( function_name ) nonMember_helper< decltype(&GameInputHandler::  function_name), &GameInputHandler:: function_name >

void GameInputHandler::onResize( GLFWwindow *window, int width, int height )
{
	WndSize = { max( width,1 ), max( height,1 ) };
}


void GameInputHandler::onKey( GLFWwindow *window, int key, int scancode, int action, int mods )
{
	if( action == GLFW_REPEAT ) return;
	
	if( key == GLFW_KEY_ESCAPE && action == GLFW_PRESS )
	{
		static bool CursorEnabled = false;
		//glfwSetInputMode(window, GLFW_CURSOR, CursorEnabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL );
		
		CursorEnabled = !CursorEnabled;
		
		//glfwSetWindowShouldClose( window, true );
		//return;
	}

	Keyboard.set( key, action == GLFW_PRESS );
}

void GameInputHandler::onMouseButton( GLFWwindow *window, int button, int action, int mods )
{
	MouseButtons.set( button, action != GLFW_RELEASE );
}

void GameInputHandler::onMouseMove( GLFWwindow *window, double xpos, double ypos )
{
	MousePos = glm::vec2( xpos, WndSize.y - ypos );
}

void GameInputHandler::onMouseEnter( GLFWwindow *window, int entered )
{
	MouseInside = entered;
}


bool GameInputHandler::OnCreate( GLFWwindow *window )
{
	OPTICK_EVENT();

	glfwSetWindowUserPointer( window, this );
	glfwSetKeyCallback( window, NON_MEMBER( onKey ) );
	glfwSetFramebufferSizeCallback( window, NON_MEMBER( onResize ) );
	glfwSetMouseButtonCallback( window, NON_MEMBER( onMouseButton ) );
	glfwSetCursorPosCallback( window, NON_MEMBER( onMouseMove ) );
	glfwSetCursorEnterCallback( window, NON_MEMBER( onMouseEnter ) );
	glfwSetWindowTitle( window, "4dGraphics" );
	glfwSetWindowSize( window, 1024, 1024 );
	glfwSetWindowPos( window, 128, 128 );
	glfwShowWindow( window );
	//glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	int w, h;
	glfwGetWindowSize( window, &w, &h );
	WndSize = { max( w,1 ), max( h,1 ) };
	double x, y;
	glfwGetCursorPos( window, &x, &y );
	MousePos = { x, WndSize.y - y };

	return ImGui_ImplGlfw_InitForOpenGL( window, true );
}

void GameInputHandler::OnPreTick()
{
	ImGui_ImplGlfw_NewFrame();
}

void GameInputHandler::OnPostTick()
{
	Keyboard.moveNext();
	MouseButtons.moveNext();
	MousePosStart = MousePos;
}

void GameInputHandler::OnDestroy( GLFWwindow *window )
{
	ImGui_ImplGlfw_Shutdown();
}
