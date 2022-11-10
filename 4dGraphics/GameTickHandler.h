#pragma once

#include "GameCore.h"
#include <imgui.h>
#include "ImguiBufferedHelper.h"

struct FrameData
{
	glm::dvec2 start, increment;
	//glm::uvec2 WndSize;
	//bool bVSync;
	//float fTime;
//
	//ImDrawDataBuffered ImGuiDrawData;
};

class GameTickHandler : public GameHandler
{
public:
	using vec2 = glm::vec2;
	using vec3 = glm::vec3;
	using vec4 = glm::vec4;
	using mat2 = glm::mat2;
	using mat3 = glm::mat3;
	using mat4 = glm::mat4;
	using quat = glm::quat;
	////////////////////////////////////////////////////////////////////////////////////////////////
	quat CamRot = glm::identity<quat>();
	vec3 CamPos = vec3( 0, 0, 10 );

	struct computePushConstants{
		glm::dvec2 start;
		glm::dvec2 increment;
	} pc;
	
	vec2 eulerCameraRot = vec2( 0, 0 );

	float lightForce = 0;
	int iShadowMapSizeId = 0;

	bool bVSync = false;
	bool bBoundingBoxes = false;

	vec3 spotPos = vec3( 0, 10, 10 );
	vec3 spotDir = vec3( 0, -10, -10 );

	uint64_t lastTimer;
	float timerInvFreq;

	float FPS;

	vec2 speed = vec2( 0 );

	std::chrono::high_resolution_clock::time_point TimeStart;
	////////////////////////////////////////////////////////////////////////////////////////////////

	virtual void *NewFData();
	virtual void DeleteFData( void *FData );

	virtual bool OnCreate(GLFWwindow*);
	virtual void OnTick( void *_FData, InputHandler *_IData );
	virtual void OnDestroy();

	void Move( glm::vec3 v );	
};
