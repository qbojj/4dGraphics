#pragma once

#include <string>
#include <cppHelpers.hpp>

struct SDL_Window;
struct ImGuiContext;
struct ImFontAtlas;

class ImGuiRIIAContext : cpph::move_only
{
public:
    ImGuiRIIAContext(ImGuiRIIAContext&&);
	ImGuiRIIAContext(ImFontAtlas*font = nullptr);
	~ImGuiRIIAContext();

    ImGuiRIIAContext &operator=(ImGuiRIIAContext&&);
	operator ImGuiContext*();

private:
    ImGuiContext *context;
};

// DO NOT CREATE TWO OF THOSE
class GLSLRIIAContext : cpph::move_only
{
public:
	GLSLRIIAContext(GLSLRIIAContext&&);
    GLSLRIIAContext();
    ~GLSLRIIAContext();

private:
	bool active;
};
class GameEngine
{
public:
	GameEngine( const char *pName = "4dGraphics", SDL_Window *window = nullptr );

	virtual ~GameEngine();

	std::string m_szName;
	SDL_Window *m_hWindow;
	ImGuiRIIAContext m_hImGui;

private:
	SDL_Window *Initialize();
	GLSLRIIAContext m_hGlsltools;
};