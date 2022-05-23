#pragma once

#include <string>
#include <memory>
#include "GlmHeaders.h"

template< typename T > using GArr = std::vector< T >;
template< typename T > using GPtr = std::shared_ptr< T >;
template< typename T > using GArrP = GArr< GPtr< T > >;

class GObject
{
public:
	std::string m_sName;
};

class GSceneNode : GObject
{
public:
	glm::mat4 m_mLocalTransform;

	GSceneNode *m_pParent;
	GArr<GSceneNode> m_aChildren;

	GArrP<GObject> m_aContents; // models / cameras / etc
};

class GBehaviour : GObject
{
public:
	virtual ~GBehaviour() {};

	virtual void Create() {};
	virtual void UpdateTick() {};
	virtual void UpdateDraw() {};
	virtual void Destroy() {};
};