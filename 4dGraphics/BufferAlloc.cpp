#include "stdafx.h"
#include "BufferAlloc.h"
#include "Debug.h"

GLBufferAllocator *g_BufAllocator = NULL;

bool GLCircularBufferGeneric_::HasToWaitForNext() const
{
	int next = (cur + 1) % partsCnt;

	if( !syncs[next] ) return false;

	GLint r, l;
	glGetSynciv( syncs[next], GL_SYNC_STATUS, 1, &l, &r );

	return r == GL_UNSIGNALED;
}

void *GLCircularBufferGeneric_::MoveToNextBuffer()
{
	Invalidate();
	++cur %= partsCnt;

	if( !syncs[cur] ) return GetData();

	GLint r, l;
	glGetSynciv( syncs[cur], GL_SYNC_STATUS, 1, &l, &r );

	if( r == GL_UNSIGNALED )
	{
		TRACE( DebugLevel::Warning, "%s had to wait\n", name.c_str() );

		GLenum res = glClientWaitSync( syncs[cur], GL_SYNC_FLUSH_COMMANDS_BIT, 100000 );
		// it's tripple buffer: It almoast never should wait here (wait 0.1s)

		if( res == GL_WAIT_FAILED || res == GL_TIMEOUT_EXPIRED )
			TRACE( DebugLevel::Error, "Couldn't wait/timeout for sync object (something horribly wrong) => pray it doesn't break\n" );
	}

	glDeleteSync( syncs[cur] );
	syncs[cur] = 0;

	return GetData();
}

void GLCircularBufferGeneric_::Flush() const
{
	if( !coherent ) glFlushMappedNamedBufferRange( Buff, GetCurrentOffset(), GetDataSize() );
}

void GLCircularBufferGeneric_::Invalidate()
{
	if( !syncs[cur] )
	{
		syncs[cur] = glFenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 );
		//SetLabelAny( GL_SYNC_FENCE, &syncs[cur], "%s - sync %d", name.c_str(), cur );
		//RenderDoc doens't show the name and says everything is wrong
	}
}

void GLCircularBufferGeneric_::clear()
{
	cur = 0;
	for( auto &i : syncs ) if( i ) glDeleteSync( i ), i = 0;
	syncs.clear();

	if( buffDat )
	{
		glUnmapNamedBuffer( Buff );
		buffDat = NULL;
	}

	Buff.clear();

	objSize = partsCnt = 0;
}

void GLCircularBufferGeneric_::Create( GLsizeiptr objSize, bool coherent, GLintptr partsCnt )
{
	clear();
	glCreateBuffers( 1, &Buff );

	static int cnt = 0;
	name = "Circular buffer " + std::to_string( cnt++ );
	Buff.SetLabel( "%s - buffer", name.c_str() );

	GLbitfield storageFlags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | (coherent ? GL_MAP_COHERENT_BIT : 0);

	glNamedBufferStorage( Buff, objSize * partsCnt, NULL, storageFlags );
	buffDat = glMapNamedBufferRange( Buff, 0, objSize * partsCnt, 
		storageFlags | (!coherent ? GL_MAP_FLUSH_EXPLICIT_BIT : 0) );

	this->objSize = objSize;
	this->coherent = coherent;
	this->partsCnt = partsCnt;
	syncs.resize( partsCnt, 0 );
}

void GLBufferAllocator::Resize( GLsizeiptr size, const char *why )
{
	Create( size );
	//#if IS_DEBUG
	//TRACE( DebugLevel::Log, "Buffer allocator resized to %d because %s\n", (int)Buff.GetDataSize(), why );
	//#endif
};

void *GLBufferAllocator::internalAlloc( GLsizeiptr siz, GLsizeiptr *allocated )
{
	//TRACE( DebugLevel::Debug, "ALLOC( %llu )\n", siz);
	siz = AlignVal( siz, alignof(max_align_t) );

	GLsizeiptr dataSize = (GLsizeiptr)Buff.GetDataSize();//data.size();
	
	if( !allocated && siz > dataSize )
		Resize( siz, "Buffer is too small for an allocation" );

	if( siz + currOffset > dataSize )
	{
		if( !allocated || (GLsizeiptr)currOffset >= dataSize )
		{
			if( Buff.HasToWaitForNext() ) Resize( dataSize, "Allocator would have to wait" );
			else Buff.MoveToNextBuffer();

			currOffset = 0;
			siz = std::min( siz, dataSize );
		}
		else siz = dataSize - currOffset;
	}

	allocatedOffset = currOffset + Buff.GetCurrentOffset();
	void *d = (char *)Buff.GetData() + currOffset;//data.data() + currOffset;//
	//void *d = glMapNamedBufferRange( Buff, currOffset, siz,
	//								 GL_MAP_WRITE_BIT | 
	//								 GL_MAP_INVALIDATE_RANGE_BIT |
	//								 GL_MAP_UNSYNCHRONIZED_BIT );

	currOffset += siz;
	allocatedSize = siz;

	if( allocated ) *allocated = siz;

	assert( (size_t)d % alignof( max_align_t ) == 0 );
	assert( allocatedOffset >= 0 );
	assert( currOffset <= Buff.GetDataSize() );

	return d;
}

void GLBufferAllocator::clear()
{
	Buff.clear();

	currOffset = allocatedOffset = 0;
	allocatedSize = 0;
}

void GLBufferAllocator::Create( GLsizeiptr maxAlloc )
{
	clear();

	GLsizeiptr realSiz = maxAlloc > 0x1000 ? (2 << glm::findMSB( maxAlloc )) : 0x1000;

	//data.resize( realSiz );
	//Buff.clear();
	//glCreateBuffers( 1, &Buff );
	//glNamedBufferStorage( Buff, realSiz, data.data(), GL_DYNAMIC_STORAGE_BIT );

	static int BuffAlocatorId = 0;
	Buff.Create( realSiz, false, 2 );
	Buff.SetName( "Buffer allocator " + std::to_string( ++BuffAlocatorId ) );

	//glCreateBuffers( 1, &Buff );
	//glNamedBufferData( Buff, BufferSize, NULL, GL_STREAM_DRAW );
}

void *GLBufferAllocator::allocAligned( GLsizeiptr siz, GLintptr alignment )
{
	if( siz + alignment > Buff.GetDataSize() ) // data.size() )
		Resize( siz + alignment, "Buffer is too small for an aligned allocation" );

	//GLsizeiptr allocated;
	//GLintptr alignPad = -( currOffset + Buff.GetCurrentOffset() ) & ( alignment - 1 );

	//currOffset += alignPad;

	void *d = internalAlloc( siz + alignment - 1, NULL );

	GLintptr offset = AlignValNonPOT( allocatedOffset, alignment ) - allocatedOffset;//(-allocatedOffset & (alignment - 1));

	allocatedOffset += offset;
	(char *&)d += offset;
	allocatedSize = siz;
	/*
	if( (GLintptr)d & ( alignment - 1 ) )
	{
		alignPad = -( currOffset + Buff.GetCurrentOffset() ) & ( alignment - 1 );
		currOffset += alignPad;
		d = internalAlloc( siz, NULL );
	}
	*/
	return d;
}
