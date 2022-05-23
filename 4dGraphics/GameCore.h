#pragma once

class GameEngine;

class ShutdownException {};

class InputHandler
{
protected:
	virtual bool OnCreate( GLFWwindow * ) { return true; };
	virtual void OnDestroy( GLFWwindow * ) {};
	virtual void OnPreTick() {};
	virtual void OnPostTick() {};
public:
	virtual ~InputHandler() {};

	friend GameEngine;
};

class GameHandler
{
protected:
	virtual void *NewFData() = 0;
	virtual void DeleteFData( void * ) = 0;

	virtual bool OnCreate() { return true; };
	virtual void OnDestroy() {};
	virtual void OnTick( void* FData, InputHandler *IData ) = 0;
public:
	virtual ~GameHandler() {};

	friend GameEngine;
};

class RenderHandler
{
protected:
	virtual bool OnCreate() { return true; };
	virtual void OnDestroy() {};
	virtual void OnDraw( const void *FData ) = 0;
public:
	virtual ~RenderHandler() {};

	friend GameEngine;
};

class GameEngine
{
protected:
	bool m_bInitialized = false;

	InputHandler *m_pInputHandler = NULL;
	GameHandler *m_pGameHandler = NULL;
	RenderHandler *m_pRenderHandler = NULL;
public:
	bool Init( InputHandler *, GameHandler *, RenderHandler * );
	bool Start();

	~GameEngine();

protected:
	void InputLoop( void *wData );
	void GameLoop( void *wData );
	void RenderLoop( void *wData );
	void EngineLoop( void *wData );
};