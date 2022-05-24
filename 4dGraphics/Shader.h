#pragma once
#include "GLId.h"
#include "Debug.h"
#include "ShaderUniforms.h"
#include "CommonUtility.h"

#define SHADER_USE_BINARY 0//IS_NOT_DEBUG

class GLShader;
class GLProgram;
class GLPipeline;

class GLShader
{
public:
	bool Load( const char *codePath, GLenum shaderType, bool lazy=false );
	void clear() { data = nullptr; };
	bool Reload();
	bool NeedsReload() const;
	std::time_t GetLastWriteTime() const;

protected:
	bool LoadInternal( const char *codePath, GLenum shaderType, bool lazy );

	enum class ShaderCompileStatus { lazy, error, success };

	struct FileInfo_t
	{
		std::string path;
		std::time_t wtime;
	};

	struct GLShaderData_t
	{
		GLShaderId ID;

		std::string ShaderFilePath;

		GLenum type;
		ShaderCompileStatus compileStatus;

		std::vector<FileInfo_t> Dependencies;
	};
	std::shared_ptr<GLShaderData_t> data;

public:
	static bool AddIncudeDir( const char *dir );

	static constexpr GLbitfield ShaderTypeToShaderTypeBit( GLenum type );
protected:
	static std::vector<std::filesystem::path> includeDirs;

	static std::filesystem::path ChoseShaderPath(const std::filesystem::path&, const std::filesystem::path&, bool*);
	static bool ResolveIncludes( 
		std::vector<std::string> &sourceList, 
		const char *filePath,
		std::vector<FileInfo_t> &deps
	);

	static bool lessThan( const GLShader &a, const GLShader &b );

	friend GLProgram;
};

class GLProgram
{
public:
	GLProgram() {};
	~GLProgram() { clear(); }

	GLProgramId ID;

	void use() { DEBUG_ONLY(assert(!separable); ) glUseProgram(ID); }
	void clear() { 
#if SHADER_USE_BINARY
		if( shaders.size() != 0 ) SaveBinary( GetBinPath() );
#endif
		ID.clear(); shaders.clear(); shadersAttached = 0;
		separable = false; ShadersReadMaxTime = 0;
	}
	//inline void use() { glUseProgram( ID ); };

	template< typename T >
	inline void set( GLint loc, const T &v ) const { uniformDetail::SetUniformDSA( ID, loc, v ); }
	template< typename T >
	inline void set( const char *name, const T &v ) const 
	{
		uniformDetail::SetUniformDSA( ID, GetUniformLocation( name ), v );
	}

	inline void setBool( GLint loc, bool value ) const { set( loc, value ); };
	inline void setInt( GLint loc, int value ) const { set( loc, value ); };
	inline void setFloat( GLint loc, float value ) const { set( loc, value ); };
	inline void setVec2( GLint loc, const glm::vec2 &value ) const { set( loc, value ); };
	inline void setVec3( GLint loc, const glm::vec3 &value ) const { set( loc, value ); };
	inline void setVec4( GLint loc, const glm::vec4 &value ) const { set( loc, value ); };
	inline void setMat3( GLint loc, const glm::mat3 &value ) const { set( loc, value ); };
	inline void setMat4( GLint loc, const glm::mat4 &value ) const { set( loc, value ); };

	inline void setBool( const char *name, bool value ) const { set( name, value ); };
	inline void setInt( const char *name, int value ) const { set( name, value ); };
	inline void setFloat( const char *name, float value ) const { set( name, value ); };
	inline void setVec2( const char *name, const glm::vec2 &value ) const { set( name, value ); };
	inline void setVec3( const char *name, const glm::vec3 &value ) const { set( name, value ); };
	inline void setVec4( const char *name, const glm::vec4 &value ) const { set( name, value ); };
	inline void setMat3( const char *name, const glm::mat3 &value ) const { set( name, value ); };
	inline void setMat4( const char *name, const glm::mat4 &value ) const { set( name, value ); };

	inline GLint GetUniformLocation( const char *name ) const
	{ 
		return glGetUniformLocation( ID, name );
		//auto r = Uniforms.find( name );
		//if( r == Uniforms.cend() ) return -1;
		//return r->second.location; //glGetUniformLocation( ID, name );
	}

	inline GLint GetAttribLocation( const char *name ) const { return glGetAttribLocation( ID, name ); }

	inline void setUniformBindingIndex( const char *name, GLuint index ) { glUniformBlockBinding( ID, glGetUniformBlockIndex( ID, name ), index ); }

	//bool Load( const Path &vertexPath, const Path &fragmentPath, bool bAllowBinary = !IS_DEBUG );
	bool Create( std::vector<GLShader> shaders, bool separable = false );
	bool Reload();

protected:
	bool CreateInternal();

	//bool LoadFromCode( const char *vertex, const char *fragment );
	bool LoadFromBinary( const char *binaryPath );
	bool SaveBinary( const char *binaryPath ) const;
	//bool InitUniforms();

	std::string GetBinPath() const;

	struct UniformInfo
	{
		GLint location;
		GLenum type;
	};

	std::vector<GLShader> shaders;
	//std::unordered_map<std::string, UniformInfo> Uniforms;
	GLbitfield shadersAttached;
	bool separable;

	std::time_t ShadersReadMaxTime;
	//std::filesystem::file_time_type ShadersReadMaxTime;

	friend GLPipeline;
};

class GLPipeline
{
public:
	GLPipelineId ID;

	void clear() { ID.clear(); }
	bool Create()
	{ 
		glCreateProgramPipelines( 1, &ID ); 
		if( !ID ) TRACE( DebugLevel::Error, "Error: glCreateProgramPipelines returned invalid handle\n" );
		return ID != 0;
	}

	inline void use() { glBindProgramPipeline( ID ); };

	inline void bind( const GLProgram &prog ) { DEBUG_ONLY( assert( prog.separable ); ) _bind( prog.shadersAttached, prog.ID ); }
	inline void bindPartial( const GLProgram &prog, GLbitfield stages ) { DEBUG_ONLY( assert( prog.separable ); ) _bind( prog.shadersAttached & stages, prog.ID ); }
	inline void unbind( GLbitfield stages ) { _bind( stages, 0 ); }

protected:
	inline void _bind( GLbitfield stages, GLuint prog )
	{
		if( stages != 0 ) glUseProgramStages( ID, stages, prog );
	}
};