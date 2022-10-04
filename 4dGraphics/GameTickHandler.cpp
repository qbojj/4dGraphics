#include "stdafx.h"
#include "GameTickHandler.h"
#include "GameInputHandler.h"

#include "Constants.h"
#include "GameCore.h"
#include "Debug.h"
#include "CommonUtility.h"

using namespace std;

void *GameTickHandler::NewFData()
{
	return new FrameData;
}

void GameTickHandler::DeleteFData( void *FData )
{
	delete (FrameData *)FData;
}

bool GameTickHandler::OnCreate(GLFWwindow*)
{
	lastTimer = glfwGetTimerValue();
	timerInvFreq = 1.f / glfwGetTimerFrequency();

	TimeStart = std::chrono::high_resolution_clock::now();

	if( !IMGUI_CHECKVERSION() ) return false;

	//ImGui::GetIO().Fonts->AddFontDefault();
	//ImGui::GetIO().Fonts->Build();

	return true;
}

void GameTickHandler::OnDestroy()
{
}

inline void GameTickHandler::Move( glm::vec3 v )
{
	CamPos += CamRot * v;
}

int modulo(int v, int m)
{
	v = v % m;
	if (v < 0) v = (v + m) % m;
	return v;
}

constexpr ImGuiKey ImGuiASCIIidx(char c)
{
	if (c >= '0' && c <= '9') return ImGuiKey_0 + (c - '0');
	if (c >= 'a' && c <= 'z') return ImGuiKey_A + (c - 'a');
	if (c >= 'A' && c <= 'Z') return ImGuiKey_A + (c - 'A');

	switch (c)
	{
	case '\t':return ImGuiKey_Tab;
	case ' ': return ImGuiKey_Space;
	case '\n':return ImGuiKey_Enter;
	 
	case '\'':return ImGuiKey_Apostrophe;
	case ',': return ImGuiKey_Comma;
	case '-': return ImGuiKey_Minus;
	case '.': return ImGuiKey_Period;
	case '/': return ImGuiKey_Slash;
	case ';': return ImGuiKey_Semicolon;
	case '=': return ImGuiKey_Equal;
	case '[': return ImGuiKey_LeftBracket;
	case '\\':return ImGuiKey_Backslash;
	case ']': return ImGuiKey_RightBracket;
	case '`': return ImGuiKey_GraveAccent;
	}

	assert(0 && "Character given to ImGuiASCIIidx doesn't correspond to a key");
	return 0;
}

void GameTickHandler::OnTick( void *, InputHandler * )
{
	/*
	OPTICK_EVENT();
	ImGui::NewFrame();
	ImGuiIO &io = ImGui::GetIO();

	FrameData *FData = (FrameData *)_FData;
	GameInputHandler *IData = (GameInputHandler *)_IData;

	uint64_t StartTickTime = glfwGetTimerValue();
	float dt = (StartTickTime - lastTimer) * timerInvFreq;
	lastTimer = StartTickTime;

	const auto KeyDown = []( char c ) { return ImGui::IsKeyDown(ImGuiASCIIidx(c) ); };
	const auto KeyPressed = []( char c ) { return ImGui::IsKeyPressed(ImGuiASCIIidx(c),false); };

	dt = glm::clamp( dt, 0.00000001f, 1.0f );

	float dist = dt * 4.f;

	{
		if( io.KeyShift ) dist *= 4;

		if( KeyDown( 'W' ) ) Move( vFront * dist );
		if( KeyDown( 'S' ) ) Move( vBack * dist );
		if( KeyDown( 'A' ) ) Move( vLeft * dist );
		if( KeyDown( 'D' ) ) Move( vRight * dist );
		if( KeyDown( ' ' ) ) Move( vUp * dist );
		if( io.KeyCtrl ) Move( vDown * dist );
	}

	lightForce = glm::clamp( lightForce + dt * (KeyDown( 'P' ) ? 1 : -1), 0.f, 1.f );


	{
		glm::vec2 mouseDiff = IData->MousePos - IData->MousePosStart;
		if( 1 )//IData->MouseButtons.curr[0] )
		{
			eulerCameraRot += vec2( -mouseDiff.x, mouseDiff.y ) * 0.002f;

			while( eulerCameraRot.x > glm::pi<float>() ) eulerCameraRot.x -= 2 * glm::pi<float>();
			while( eulerCameraRot.x < -glm::pi<float>() ) eulerCameraRot.x += 2 * glm::pi<float>();

			eulerCameraRot.y = glm::clamp( eulerCameraRot.y, -glm::pi<float>() / 2, glm::pi<float>() / 2 );

			CamRot = glm::angleAxis( eulerCameraRot.x, vUp ) * glm::angleAxis( eulerCameraRot.y, vRight ) * glm::identity<quat>();
		}
	}

	if( KeyDown('Q') ) CamRot *= glm::angleAxis( -dt * 2, vFront );
	if( KeyDown('E') ) CamRot *= glm::angleAxis( dt * 2, vFront );


	if( KeyDown('Q') ) CamRot *= glm::angleAxis( -dt * 2, vFront );
	if( KeyDown('E') ) CamRot *= glm::angleAxis( dt * 2, vFront );

	//bTextureView ^= pressed['T'];
	bVSync ^= KeyPressed( 'V' );
	bBoundingBoxes ^= KeyPressed( 'B' );

	static bool ShowDebugWindow = false;

	if( ImGui::BeginMainMenuBar() )
	{
		if( ImGui::BeginMenu( "View" ) )
		{
			ImGui::MenuItem( "debug", NULL, &ShowDebugWindow );
			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}

	if( ShowDebugWindow )
	{
		if( ImGui::Begin( "debug info", &ShowDebugWindow ) )
		{
			ImGui::Text( "%.3f FPS (%.3f ms)", io.Framerate, 1000 / io.Framerate );
		}
		ImGui::End();
	}

	ImGui::Render();

	FData->ImGuiDrawData.CopyDrawData( ImGui::GetDrawData() );
	*/
}