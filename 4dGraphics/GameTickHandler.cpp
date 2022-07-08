#include "stdafx.h"
#include "GameTickHandler.h"
#include "GameInputHandler.h"
#include "Objects.h"

#include "GameCore.h"
#include "Debug.h"
#include "CommonUtility.h"
#include "GLUniformBlockDefinitions.h"

using namespace std;

template< typename T >
const glm::mat<4, 4, T> ReverseDepthMatrix{
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0,-1, 0,
	0, 0, 1, 1
};

const float lightNearValue = 0.1f;
const float lightFarValue = 20000;

struct LightsContext
{
public:
	glsl::Lights &LightsDat;
	int FirstFreeLight;
	int FirstFreeShadowMap;
	int MaxUsedShadowIdx;

	CameraCollider lightCameras[MAX_SHADOW_MAPS];

	LightsContext( glsl::Lights &lightData ) : LightsDat( lightData )
	{
		memset( &LightsDat, 0, sizeof( LightsDat ) );
		FirstFreeLight = FirstFreeShadowMap = 0;
		MaxUsedShadowIdx = -1;
	}

protected:
	void SetShadowMapBinding( int pos, int cnt, const glm::mat4 transform[] )
	{
		if( pos == -1 ) return;

		assert( pos >= 0 && pos + cnt <= MAX_SHADOW_MAPS );

		for( int p = 0; p < cnt; p++ )
		{
			int i = pos + p;
			LightsDat.amShadowMatrices[i] = transform[p];
			PACKED_BITS_SET( LightsDat.baShadowMapActive, i );
		}

		MaxUsedShadowIdx = max( MaxUsedShadowIdx, pos + cnt - 1 );
		//glNamedBufferSubData( Lights, offsetof( GLLights, amShadowMatrices[pos] ), cnt * sizeof( glm::mat4p ), transform );
	}

	void SetLight( const glsl::LightSpec &ls )
	{
		if( FirstFreeLight < MAX_LIGHTS ) LightsDat.aoLights[FirstFreeLight++] = ls;
		else TRACE( DebugLevel::Warning, "Not enough light slots!\n" );
	}

	int GetShadowMapFreeBlock( int size, int align )
	{
		while(
			FirstFreeShadowMap < MAX_SHADOW_MAPS &&
			PACKED_BITS_GET( LightsDat.baShadowMapActive, FirstFreeShadowMap ) ) FirstFreeShadowMap++;

		int Checked = AlignValNonPOT( FirstFreeShadowMap, align );
		for( bool OK = false; Checked + size <= MAX_SHADOW_MAPS; Checked += align )
		{
			OK = true;
			for( int i = 0; i < align; i++ ) OK &= !PACKED_BITS_GET( LightsDat.baShadowMapActive, Checked + i );

			if( OK ) break;
		}

		return Checked + size <= MAX_SHADOW_MAPS ? Checked : -1;
	}

public:
	void pointlight(
		glm::vec3 amb, glm::vec3 dif, glm::vec3 spec,
		glm::vec3 pos,
		float constAtt, float linAtt, float cubicAtt )
	{
		//RemoveShadowMap( id );

		glsl::LightSpec ls;
		ls.bEnabled = true;
		ls.iLightType = 2;

		ls.iShadowMapId = GetShadowMapFreeBlock( 6, 6 );

		const glm::mat4 pointLightProj = ReverseDepthMatrix<float> *glm::perspective(//ReversedDepthPerspective(
			glm::radians( 90.f ), 1.f, lightNearValue, lightFarValue
		);

		// < facing, up >
		const glm::vec3 cubemapDirs[6][2] = {
			{vRight, vDown }, // +X
			{vLeft , vDown }, // -X
			{vUp   , vBack }, // +Y
			{vDown , vFront}, // -Y
			{vBack , vDown }, // +Z
			{vFront, vDown }, // -Z
		};

		if( ls.iShadowMapId != -1 )
		{
			glm::mat4 transforms[6];
			for( int i = 0; i < 6; i++ )
			{
				glm::mat4 view = glm::lookAt( pos, pos + cubemapDirs[i][0], cubemapDirs[i][1] );
				transforms[i] = pointLightProj * view;
				lightCameras[ls.iShadowMapId + i].Create( view, glm::radians( 90.f ), 1.f, lightNearValue, lightFarValue );
			}

			SetShadowMapBinding( ls.iShadowMapId, 6, transforms );
		}
		else TRACE( DebugLevel::Warning, "Non enough shadow maps!\n" );

		ls.vColAmb = glm::vec4( amb, 1 );
		ls.vColDif = glm::vec4( dif, 1 );
		ls.vColSpec = glm::vec4( spec, 1 );
		ls.vPos = glm::vec4( pos, 1 );
		//	ls.vDir = glm::vec3( 0 );
		ls.vAttenuation = glm::vec4( constAtt, linAtt, cubicAtt, 1 );

		SetLight( ls );
	}

	void spotlight(
		glm::vec3 amb, glm::vec3 dif, glm::vec3 spec,
		glm::vec3 pos, glm::vec3 dir,
		float innerCutoffAngle, float outerutoffAngle,
		float constAtt, float linAtt, float cubicAtt )
	{
		//RemoveShadowMap( id );

		glsl::LightSpec ls;
		ls.bEnabled = true;
		ls.iLightType = 1;

		ls.iShadowMapId = GetShadowMapFreeBlock( 1, 1 );

		glm::mat4 view = glm::lookAt( pos, pos + dir, vUp );

		glm::mat4 transform =
			ReverseDepthMatrix<float> *glm::perspective(//ReversedDepthPerspective( 
				glm::radians( outerutoffAngle * 2 ), 1.f, lightNearValue, lightFarValue ) * view;

		if( ls.iShadowMapId != -1 )
		{
			lightCameras[ls.iShadowMapId].Create( view, glm::radians( outerutoffAngle * 2 ), 1.f, lightNearValue, lightFarValue );
			SetShadowMapBinding( ls.iShadowMapId, 1, &transform );
		}
		else TRACE( DebugLevel::Warning, "Non enough shadow maps!\n" );

		ls.vColAmb = glm::vec4( amb, 1 );
		ls.vColDif = glm::vec4( dif, 1 );
		ls.vColSpec = glm::vec4( spec, 1 );
		ls.vPos = glm::vec4( pos, 1 );
		ls.vDir = glm::vec4( glm::normalize( dir ), 1 );
		ls.vCutoff = glm::vec2(
			glm::cos( glm::radians( innerCutoffAngle ) ),
			glm::cos( glm::radians( outerutoffAngle ) ) );
		ls.vAttenuation = glm::vec4( constAtt, linAtt, cubicAtt, 1 );

		SetLight( ls );
	}

	void directional(
		glm::vec3 amb, glm::vec3 dif, glm::vec3 spec,
		glm::vec3 dir )
	{
		//RemoveShadowMap( id );

		glsl::LightSpec ls;
		ls.bEnabled = true;
		ls.iLightType = 0;

		ls.iShadowMapId = GetShadowMapFreeBlock( 1, 1 );

		glm::mat4 view = glm::scale( glm::lookAt( vZero, -dir * 100000.f, vUp ), glm::vec3( 1000.f, 1000.f, 1.f ) );
		glm::mat4 transform =
			glm::ortho( -1.f, 1.f, -1000.f, 1000.f ) * //ReversedDepthPerspective( glm::radians( 179.9f ), 1.f, lightNearValue ) *
			view;

		if( ls.iShadowMapId != -1 )
		{
			lightCameras[ls.iShadowMapId].Create( view, glm::radians( 179.9f ), 1.f, -1000.f, 1000.f );
			SetShadowMapBinding( ls.iShadowMapId, 1, &transform );
		}
		else TRACE( DebugLevel::Warning, "Non enough shadow maps!\n" );

		ls.vColAmb = glm::vec4( amb, 1 );
		ls.vColDif = glm::vec4( dif, 1 );
		ls.vColSpec = glm::vec4( spec, 1 );
		ls.vDir = glm::vec4( glm::normalize( dir ), 1 );
		//	ls.vCutoff = glm::vec2( -1, 0 );

		SetLight( ls );
	}

	void RemoveNotColliding( const CameraCollider &cam )
	{
		for( int i = 0; i < MaxUsedShadowIdx; i++ )
			if( PACKED_BITS_GET( LightsDat.baShadowMapActive, i ) &&
				Intersects( &cam.projectionHull, &lightCameras[i].projectionHull ) == IntersectResult::NoCollision )
				PACKED_BITS_RESET( LightsDat.baShadowMapActive, i );
	}
};

void *GameTickHandler::NewFData()
{
	return new FrameData;
}

void GameTickHandler::DeleteFData( void *FData )
{
	delete (FrameData *)FData;
}

bool GameTickHandler::OnCreate()
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

void GameTickHandler::OnTick( void *_FData, InputHandler *_IData )
{
	OPTICK_EVENT();
	ImGui::NewFrame();
	ImGuiIO &io = ImGui::GetIO();

	GSceneNode scene;

	FrameData *FData = (FrameData *)_FData;
	GameInputHandler *IData = (GameInputHandler *)_IData;

	uint64_t StartTickTime = glfwGetTimerValue();
	float dt = (StartTickTime - lastTimer) * timerInvFreq;
	lastTimer = StartTickTime;

	//*FData = {}; // default construct


	const auto KeyDown = []( char c ) { return ImGui::IsKeyDown(ImGuiASCIIidx(c) ); };
	const auto KeyPressed = []( char c ) { return ImGui::IsKeyPressed(ImGuiASCIIidx(c),false); };


	dt = glm::clamp( dt, 0.00000001f, 1.0f );

	float dist = dt * 4.f; //  * glm::length( CamPos )

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

	/*
	{
		glm::vec2 mouseDiff = IData->MousePos - IData->MousePosStart;
		glm::vec2 mouseWordDiff = mouseDiff * -dist * 0.02f;
	
		if( IData->MouseButtons.released[0] ) speed = mouseDiff / dt;
		if( IData->MouseButtons.curr[0] )
		{
			Move( vec3( mouseWordDiff, 0 ) );
			speed = vec2( 0 );
		}
	
		Move( vec3( speed * -dist * 0.02f, 0 ) * dt );
	}

	CamRot = glm::quatLookAt( glm::normalize( -CamPos ), CamRot * vUp ); // look at center
	*/

	{
		glm::vec2 mouseDiff = IData->MousePos - IData->MousePosStart;
		//glm::vec2 mouseWordDiff = mouseDiff * -dist * 0.02f;

		//if( IData->MouseButtons.released[0] ) speed = mouseDiff / dt;
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
	//TextureViewTexNum = TextureViewTexNum + pressed['='] - pressed['-'];
	iShadowMapSizeId = modulo( iShadowMapSizeId + KeyPressed( '=' ) - KeyPressed( '-' ), 10 );

	/////////////////////////////////////////////////////////////////////////////////////////

	FData->WndSize = IData->WndSize;
	FData->bVSync = bVSync;
	FData->bBoundingBoxes = bBoundingBoxes;
	FData->bReload = KeyPressed( 'R' );
	FData->iShadowMapSize = 1 << ( iShadowMapSizeId + 4 );

	float fTime = FData->fTime = (float)ImGui::GetTime();//std::chrono::duration<float>( std::chrono::high_resolution_clock::now() - TimeStart ).count();
	//FData->MSPT = 1000 * ( glfwGetTimerValue() - StartTickTime ) * timerInvFreq;
	//FData->FPS = ( FPS += ( 1 / dt - FPS ) * dt * 10 );

	CameraCollider camera;
	{
		const float Near = 0.00001f, Far = 800000;

		glm::mat4 View, Proj;

		float fovy = glm::radians( 40.f );
		float aspect = (float)FData->WndSize.x / FData->WndSize.y;

		Proj = ReverseDepthMatrix<float> *glm::perspective( fovy, aspect, Near, Far );
		//ReversedDepthPerspective( fovy, aspect, near )

		glm::dmat4 WordRot = glm::mat4_cast( glm::inverse( glm::dquat( CamRot ) ) );
		View = glm::translate( WordRot, glm::dvec3( -CamPos ) );

		glsl::MatsVP mats;
		mats.mView = View;
		mats.mViewIT = glm::inverse( View );
		mats.mProj = Proj;
		mats.mVP = Proj * View;
		mats.vViewWorldPos = vec4( CamPos, 1 );// glm::vec4( 0, 0, 0, 1 ) *mats.mViewIT;
		mats.fTime = fTime;

		FData->oMats = mats;

		camera.Create( View, fovy, aspect, Near, Far );
	}

	//ImGui::ShowMetricsWindow();

	static bool ShowLightsWindow = false, ShowDebugWindow = false;

	if( ImGui::BeginMainMenuBar() )
	{
		if( ImGui::BeginMenu( "View" ) )
		{
			ImGui::MenuItem( "lights", NULL, &ShowLightsWindow );
			ImGui::MenuItem( "debug", NULL, &ShowDebugWindow );
			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}

	{
		LightsContext lights( FData->oLightsDat ); // updates LightsDat

		glm::vec3 LightPos( 0, 30, 0 );//1000 * sin(T*10), 1000 * cos(T*10) );
		lights.pointlight( vec3( .02f ), vec3( 0.3f ), vec3( 0.5f ), LightPos, 1, 0.0001f, 0 );

		{
			//float q = glm::smoothstep( 0.f, 1.f, lightForce );

			vec3 col = vOne;
			col *= 1;// q;

			float outerCutoff = 12.5f;

			if( ShowLightsWindow )
			{
				if( ImGui::Begin( "Lights", &ShowLightsWindow ) )
				{
					ImGui::DragFloat3( "light pos", glm::value_ptr( spotPos ) );
					ImGui::DragFloat3( "light dir", glm::value_ptr( spotDir ) );
				}
				ImGui::End();
			}

			lights.spotlight(
				vZero, col * 0.8f, col * 0.3f,
				//FData->CamPos, FData->CamRot * vFront,
				spotPos, spotDir,
				10, outerCutoff, 1, /*0.045f*/0, 0 );
		}

		//lights.RemoveNotColliding( camera );

		FData->iUsedShadowMapCnt = lights.MaxUsedShadowIdx + 1;
	}

	if( ShowDebugWindow )
	{
		if( ImGui::Begin( "debug info", &ShowDebugWindow ) )
		{
			ImGui::Text( "%.3f FPS (%.3f ms)", io.Framerate, 1000 / io.Framerate );
			ImGui::Text( "ShadowMapSize = %d x %d x %d (%.3f MB)",
				FData->iShadowMapSize, FData->iShadowMapSize, FData->iUsedShadowMapCnt,
				FData->iShadowMapSize * FData->iShadowMapSize * FData->iUsedShadowMapCnt * sizeof( float ) / (1024. * 1024.) );
		}
		ImGui::End();
	}



	ImGui::Render();

	FData->ImGuiDrawData.CopyDrawData( ImGui::GetDrawData() );
}