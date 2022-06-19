#pragma once

#include "CommonUtility.h"
#include "GLId.h"
#include <string>

class GLBufferAllocator;
extern GLBufferAllocator *g_BufAllocator;

class GLCircularBufferGeneric_
{
public:
	void clear();
	void Create( GLsizeiptr objSize, bool coherent = true, GLintptr partsCnt = 3 );

	void *GetData() { return (char *)buffDat + GetCurrentOffset(); }
	GLuint ID() const { return Buff; };
	bool HasToWaitForNext() const;
	void *MoveToNextBuffer();
	inline void Flush() const;
	inline void Invalidate();

	GLintptr GetCurrentOffset() const { return cur * objSize; };
	GLsizeiptr GetDataSize() const { return objSize; };

	~GLCircularBufferGeneric_() { clear(); }

	void SetName( const std::string &nm ) { name = nm; Buff.SetLabel( "%s - buffer", nm.data() ); }

private:
	GLsizeiptr objSize;
	GLintptr partsCnt;

	bool coherent;
	GLBufId Buff;
	void *buffDat = NULL;
	GLint cur;
	std::vector<GLsync> syncs;
	std::string name;
};

class GLBufferAllocator
{
protected:
	void Resize( GLsizeiptr size, const char *why );
	void *internalAlloc( GLsizeiptr siz, GLsizeiptr *allocated );

public:
	void clear();
	void Create( GLsizeiptr maxAlloc = 1 << 20u );

	inline void *allocPartial( GLsizeiptr siz, GLsizeiptr &allocated ) { return internalAlloc( siz, &allocated ); }
	inline void *alloc( GLsizeiptr siz ) { return internalAlloc( siz, NULL ); }

	// alignment must be power of 2
	void *allocAligned( GLsizeiptr siz, GLintptr alignment );

	inline void Flush( GLsizeiptr si ) const { assert( si <= allocatedSize ); glFlushMappedNamedBufferRange( Buff.ID(), allocatedOffset, si ); } // glNamedBufferSubData( Buff, allocatedOffset, si, data.data() ); }
	inline void Flush() const { Flush( allocatedSize ); }

	inline GLintptr GetCurrentOffset() const { return allocatedOffset; }
	inline GLuint ID() const { return Buff.ID(); }//.ID(); }

private:
	//GLCircularBufferGeneric_ Buff;

	GLintptr currOffset = 0;
	GLintptr allocatedOffset = 0;
	GLsizeiptr allocatedSize = 0;

	GLCircularBufferGeneric_ Buff;
	//std::vector<char> data;
};

/*
class GLCircularUniformBufferGeneric_ : public GLCircularBufferGeneric_
{
public:
	inline void Create( GLsizeiptr typeSize, bool Coherent = true, GLintptr PartsCnt = 3 )
	{
		GLsizeiptr alignedSize = AlignVal( typeSize, GLParameters.UniformAlignment );
		GLCircularBufferGeneric_::Create( alignedSize, Coherent, PartsCnt );
	}
};

template< class GLCircularBufferType,  typename T >
class GLCircularBufferTyped_ : public GLCircularBufferType
{
	using PBuffT = GLCircularBufferType *;
public:
	static_assert( std::is_trivially_copyable<T>, "Buffer must be trivially" );

	inline void Create( bool Coherent = true, GLintptr PartsCnt = 3 )
	{
		GLCircularBufferType::Create( sizeof( T ), Coherent, PartsCnt );
		GLCircularBufferType::SetName( std::string("circular buffer (") + typeid( T ).name() + ")" );
	}

	inline T *GetData() const { return (T *)( (PBuffT)this )->GetData(); }
	inline T *MoveToNextBuffer() { return (T *)( (PBuffT)this )->MoveToNextBuffer(); };
};

template< typename T > using GLCircularUniformBuffer = GLCircularBufferTyped_< GLCircularUniformBufferGeneric_, T >;
template< typename T > using GLCircularBuffer = GLCircularBufferTyped_< GLCircularBufferGeneric_, T >;

template< typename T >
class GLUniformArrayData
{
public:
	~GLUniformArrayData() { clear(); }
	GLUniformArrayData() {}
	GLUniformArrayData( GLintptr cnt ) { Create( cnt ); }

	void Create( GLintptr cnt )
	{
		clear();
		objSize = sizeof( T ) + ( -(GLintptr)sizeof( T ) & ( GLParameters.UniformAlignment - 1 ) );
		objCnt = cnt;
		data = malloc( GetSize() );
	}

	inline GLsizeiptr GetSize() const { return objSize * objCnt; }
	inline T &operator[]( int i ) { return *(T*)( (char *)data + objSize * i); }
	inline void clear() { free(data); objSize = objCnt = 0; data = 0; }
protected:
	void *data;
	GLsizeiptr objSize;
	GLintptr objCnt;
};
*/
