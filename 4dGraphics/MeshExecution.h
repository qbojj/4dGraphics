#pragma once

#include "GLId.h"
#include "GLUniformBlockDefinitions.h"
#include "Collisions.h"
#include "CommonUtility.h"
#include "ShaderUniforms.h"

class GLProgram;

struct GLDrawElementsIndirectData
{
	glm::u32 count;
	glm::u32 instanceCount;
	glm::u32 firstIndex;
	glm::i32 baseVertex = 0;
	glm::u32 baseInstance = 0;
};

class Model
{
public:
	void Draw( GLProgram *fragment ) const;
	void DrawMasked( GLProgram *fragment, const std::vector<bool> &mask, size_t startIdx ) const;
	bool Load( const char *file );
	bool Load( const char *file, Assimp::Importer &importer, unsigned int additionalFlags = 0 );
	void clear();

	void MakeNonResident() const;
	void MakeResident() const;

	size_t GetOBBcnt() const { return DrawCallInfo.size(); };
	void GetOBBS( collisions::OBB *res ) const;
protected:
	void DrawInternal( GLProgram *fragment, const GLDrawElementsIndirectData *dat, GLsizei size ) const;
	GLDrawElementsIndirectData GetDrawElementsData( size_t DrawCallInfoIdx ) const;

	GLVAOId VAO;
	GLBufId RenderDataBuffer; 

	struct BufferSubresource {
		GLintptr offset;
		GLsizeiptr size;

		GLintptr end() const { return offset + size; }
	} VBO, EBO, Materials, Matrices;

	GLenum EBOType;

	struct NodeData_t
	{
		int parent;
		std::vector< glm::uint > Children;

		std::vector< glm::uint > MeshIds;
		glm::uint MatrixIdx;

		std::string name;
	};

	struct MeshData_t
	{
		glm::uint start, end, material, baseVertex;
		collisions::AABB aabb;
	};

	std::vector< MeshData_t > MeshData;
	std::vector< NodeData_t > NodeData;
	std::vector< glm::mat4 > matrices;

	std::vector<GLTexId> textures;
	std::vector<GLuint64> textureHandles; mutable bool texturesResident;

	struct DrawCallInfo_t
	{
		unsigned int NodeIdx, MeshIdx;
	};

	std::vector<DrawCallInfo_t> DrawCallInfo;

	glm::uint TraverseNodes( aiNode *node, int parent, glm::mat4 currMat );// , GLModelMatrices *Matrices, int curMatIdx = 0 );//, std::unordered_map<glm::mat4, int> &MatsIdxs );
};
/*
class FontRenderer
{
protected:
	using ivec2=glm::ivec2;
	using vec4=glm::vec4;

	struct glyphInfo
	{
		glm::ivec2 pos;
		glm::ivec2 size;
		glm::ivec2 bearing;
		glm::ivec2 advance;
	};

	GLTexId tex;
	GLVAOId VAO;
	GLBufId EBO;

	GLuint VBOBound;

	inline static constexpr int numOfGlyphsInBatch = 1024;
	struct quad { vec4 a, b, c, d; };

	//robin_hood::unordered_map< unsigned short, unsigned short > CharToGlyph;
	std::vector< unsigned int > CharToGlyph;
	std::vector< glyphInfo > GlyphInfo;
	glm::ivec2 texSize;
	int charHeight;

public:
	void RenderText( GLProgram *fontFrag, const char *str, glm::vec2 pos, glm::vec4 col = { 1,1,1,1 }, float scale = 1 );
	bool Load( FT_Library ftLib, const char *fontPath, int CharHeight = 48 );

	~FontRenderer() { clear(); }
	void clear();
};
*/
class StreamArrayDrawer
{
public:
	GLVAOId VAO;

	struct VertexAttributeDescr
	{
		GLuint attribindex;
		GLint size;
		GLenum type;
		GLboolean normalized;
		GLuint relativeOffset;

		GLuint arrayID = 0;
	};

	struct BindingDescr
	{
		GLsizei stride;
		GLuint divisor = 0;
	};

	void clear() { VAO.clear(); arrayDescriptors.clear(); };
	void Create( const std::vector<VertexAttributeDescr> &attributes, const std::vector<BindingDescr> &arrays );

	void DrawStreamArray( const void *vertex_data[], GLsizei size, GLenum mode = GL_TRIANGLES );
	void DrawStreamElements( const void *vertex_data[], GLsizei dataSize, const void *indices, 
		GLsizei count, GLenum mode = GL_TRIANGLES, GLenum indices_type = GL_UNSIGNED_INT );

protected:
	std::vector<BindingDescr> arrayDescriptors;
	GLsizeiptr indecesOffset;
	bool prepareData( const void *vertex_data[], GLsizei size, const void *index = NULL, GLsizei indexSize = 0 );
};