#pragma once

#include "GameCore.h"
#include "GLId.h"
#include "MeshExecution.h"
#include "Shader.h"

#include "Collisions.h"

class GameRenderHandler : public RenderHandler
{
public:
	GLBufId Mats, Lights;
	Model object, room, box;

	GLProgram fontProgram, CommonProgram, ShadowProgram, StreamDrawProgram;

	GLProgram PostprocessVertex, PostprocessBlit, PostprocessToon;
	GLPipeline PostprocessPipeline;

	GLVAOId PostprocessVAO;

	GLProgram ImGuiProgram;
	GLVAOId ImGuiVAO;
	GLTexId ImGuiFontAtlas;

	float MSPF;
	//FontRenderer fontArial;
	glm::uvec2 LastWndSize = glm::vec2( -1, -1 );
	glm::vec2 InvWndSize;
	bool LastVSync = 0;

	GLFBOId FrameBuffer;
	GLTexId FrameBufferTextures[3]; // color, normal, depth
	GLRenderbufferId FrameBufferRenderbuffer;

	GLFBOId ShadowFBO;
	GLTexId ShadowMapViewArray, ShadowMapCubeArray;

	StreamArrayDrawer StreamDrawSimple;

	GLuint ShadowMapCount = -1;

	int ShadowMapSize = 1;

	//FT_Library FreeTypeLib;

	//void SetUpShadowPipeline( bool firstTime = false );
	void CreateAndInitFBO( glm::ivec2 size );
	void CreateAndInitShadowFBO( int cnt, int size );

	inline void LowPassFilter( float &r, float n ) { r += ( n - r ) * 0.001f; }

	float
		tMatrices = 0,
		tLights = 0,
		tFrustrumTest = 0,
		tDispachRender = 0,
		tText = 0;

	virtual bool OnCreate();
	virtual void OnDraw( const void *_FData );
	virtual void OnDestroy();
};