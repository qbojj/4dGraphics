#include "GameCore.h"
#include "GameInputHandler.h"
#include "GameRenderHandler.h"
#include "GameTickHandler.h"

#include "Debug.h"
#include "optick.h"
#include <stdlib.h>
#include <filesystem>
#include <chrono>
#include <thread>

#ifdef _WIN32
#include "Windows.h"
#endif

#include "Shader.h"

void* ImGuiAlloc(size_t size, void* dat)
{
	if( dat ) (*(int*)dat)++;
	return malloc(size);
}

void ImGuiFree(void* ptr, void* dat)
{
	if (ptr)
	{
		if(dat) (*(int*)dat)--;
		free(ptr);
	}
}

int Entry()
{
	OPTICK_APP( "4dGraphics" );

	//LogLevel = DebugLevel::Warning;

	srand( (unsigned int)time( NULL ) );

	{
		time_t rawtime;
		struct tm *timeinfo;
		char buffer[80];

		time( &rawtime );
		timeinfo = localtime( &rawtime );

		strftime( buffer, sizeof( buffer ), "%d-%m-%Y %H:%M:%S", timeinfo );

		TRACE( DebugLevel::Log, "Program started (%s)\n", buffer );
		TRACE( DebugLevel::Log, "Compiled with: %u\n", __cplusplus );
	}

	TRACE( DebugLevel::Log, "working dir: %s\n", std::filesystem::current_path().string().c_str() );
	GameEngine engine;
	
	bool ok = false;
	
	if( engine.Init( 
		new GameInputHandler(),
		new GameTickHandler(),
		new GameRenderHandler() )
		)
	{
		int allocCnt = 0;
		ImGui::SetAllocatorFunctions(ImGuiAlloc, ImGuiFree, &allocCnt);

		if (ImGui::CreateContext() == NULL)
		{
			TRACE(DebugLevel::FatalError, "Cannot initialize imgui context");
			return 0;
		}

		if (!glslang::InitializeProcess())
		{
			ImGui::DestroyContext();
			TRACE(DebugLevel::FatalError, "Cannot initialize glslang context");
			return 0;
		}

		ok = engine.Start();
		glslang::FinalizeProcess();
		ImGui::DestroyContext();

		if( allocCnt != 0 )
			TRACE( DebugLevel::Error, "ImGui made %d %s %s than frees\n", 
				abs( allocCnt ), 
				abs( allocCnt ) > 1 ? "allocations" : "allocation", 
				allocCnt > 0 ? "more" : "less" );
	}

	return ok ? 0 : 1;
}

#if defined(REQUIRE_WINMAIN)
int WINAPI WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nShowCmd
)
{
	(void)hInstance, hPrevInstance, lpCmdLine, nShowCmd;
	return Entry();
}

#else
int main()
{
	return Entry();
}
#endif
