#include "Collisions.h"
using namespace collisions;
using namespace detail;

#include <numeric>
//#include <execution>
//#include <ranges>
//#include "taskflow/taskflow.hpp"

#include <immintrin.h>

template< size_t allocSize, size_t align, size_t cnt >
class FastAlignedAllocator
{
	static_assert(allocSize % align == 0, "allocSize must be multiple of align");
	static_assert( ( align & (align-1) ) == 0, "align must be power of 2");
	static_assert(allocSize > 0 && align > 0 && cnt > 0, "allocSize, align and cnt must be > 0");

protected:
	using idxType = 
		std::conditional_t< cnt <= (size_t)std::numeric_limits<uint8_t >::max() - 1, uint8_t,
		std::conditional_t< cnt <= (size_t)std::numeric_limits<uint16_t>::max() - 1, uint16_t,
		std::conditional_t< cnt <= (size_t)std::numeric_limits<uint32_t>::max() - 1, uint32_t,
		                                                                             uint64_t > > >;
public:
	constexpr FastAlignedAllocator()
	{
		std::iota( std::rbegin( freeDat ), std::rend( freeDat ), static_cast<idxType>(0) );
		freeEnd = cnt;
		std::fill( std::begin( data ), std::end( data ), (char)0 );
	};

	void *Alloc()
	{
		if( freeEnd != 0 ) return &data[allocSize * freeDat[--freeEnd]];
		else return _mm_malloc( allocSize, align );
	}

	void Free( void *p )
	{
		if( (char*)p >= &data[0] && (char *)p < &data[allocSize * cnt] ) freeDat[freeEnd++] = (idxType)( ((char *)p - &data[0]) / allocSize );
		else _mm_free( p );
	}
	
protected:
	alignas(align) char data[ allocSize * cnt ];
	idxType freeDat[cnt];
	idxType freeEnd;
};

VerticesCoordsData collisions::detail::IntersectTestable::GetVerticesStream() const { return *GetVertices(); }

static ElementIdxVect
Box_Triangles{
	// front face
	0, 1, 2,
	1, 3, 2,
	// top face
	2, 3, 6,
	3, 7, 6,
	// back face
	6, 7, 4,
	7, 5, 4,
	// down face
	4, 5, 0,
	5, 1, 0,
	// right face
	3, 1, 7,
	1, 5, 7,
	// left face
	4, 0, 2,
	0, 6, 2
},
Box_Lines{
	// front face
	0,1,
	1,3,
	3,2,
	2,0,
	// back face
	4,5,
	5,7,
	7,6,
	6,4,
	// front to back lines
	0,4,
	1,5,
	3,7,
	2,6
};

ElementIdxVectRes collisions::detail::Box::Triangles() const { return Box_Triangles; }
ElementIdxVectRes collisions::detail::Box::Lines() const { return Box_Lines; }

VectorVectRes AABB::GetVertices() const
{
	VectorVect q( 8 );
	for( int i = 0; i < 8; i++ )
		q[i] = Vect(
			(i & 1 ? hi : lo).x,
			(i & 2 ? hi : lo).y,
			(i & 4 ? hi : lo).z
		);

	return q;
}

static VectorVect AABB_NormalsEdges{ Vect( 0, 0, 1 ), Vect( 0, 1, 0 ), Vect( 1, 0, 0 ) };

VectorVectRes AABB::GetNormals() const { return AABB_NormalsEdges; }
VectorVectRes AABB::GetEdges() const { return AABB_NormalsEdges; }

bool AABB::Intersects( const AABB &rhs ) const
{
	return glm::all(
		glm::lessThan( lo, rhs.hi ) &&
		glm::lessThan( rhs.lo, hi )
	);
}

OBB::OBB() : dat( 0 ) {}
OBB::OBB( const Vect &p, const Vect &v1, const Vect &v2, const Vect &v3 ) : dat( v1, v2, v3, p ) {}
OBB::OBB( const AABB &o ) : dat( Vect( o.hi.x - o.lo.x, 0, 0 ), Vect( 0, o.hi.y - o.lo.y, 0 ), Vect( 0, 0, o.hi.z - o.lo.z ), o.lo ) {}

AABB OBB::GetAABB() const
{
	AABB res{ Vect( INFINITY ), Vect( -INFINITY ) };

	for( int i = 0; i < 8; i++ )
		res += dat * glm::vec4( i & 1 ? 1 : 0, i & 2 ? 1 : 0, i & 4 ? 1 : 0, 1 );

	return res;
}

VerticesCoordsData OBB::GetVerticesStream() const
{
#if defined(__AVX__)
	VerticesCoordsData res( 8 );

	__m256 Xmask = _mm256_castsi256_ps( _mm256_set_epi32( -1, 0, -1, 0, -1, 0, -1, 0 ) );
	__m256 Ymask = _mm256_castsi256_ps( _mm256_set_epi32( -1, -1, 0, 0, -1, -1, 0, 0 ) );
	__m256 Zmask = _mm256_castsi256_ps( _mm256_set_epi32( -1, -1, -1, -1, 0, 0, 0, 0 ) );

	__m256 x0 = _mm256_broadcast_ss( &dat[0].x );
	__m256 y0 = _mm256_broadcast_ss( &dat[0].y );
	__m256 z0 = _mm256_broadcast_ss( &dat[0].z );

	__m256 x1 = _mm256_broadcast_ss( &dat[1].x );
	__m256 y1 = _mm256_broadcast_ss( &dat[1].y );
	__m256 z1 = _mm256_broadcast_ss( &dat[1].z );

	__m256 x2 = _mm256_broadcast_ss( &dat[2].x );
	__m256 y2 = _mm256_broadcast_ss( &dat[2].y );
	__m256 z2 = _mm256_broadcast_ss( &dat[2].z );

	__m256 x3 = _mm256_broadcast_ss( &dat[3].x );
	__m256 y3 = _mm256_broadcast_ss( &dat[3].y );
	__m256 z3 = _mm256_broadcast_ss( &dat[3].z );

	x0 = _mm256_and_ps( x0, Xmask );
	y0 = _mm256_and_ps( y0, Xmask );
	z0 = _mm256_and_ps( z0, Xmask );

	x1 = _mm256_and_ps( x1, Ymask );
	y1 = _mm256_and_ps( y1, Ymask );
	z1 = _mm256_and_ps( z1, Ymask );

		x0 = _mm256_add_ps( x0, x1 );
		y0 = _mm256_add_ps( y0, y1 );
		z0 = _mm256_add_ps( z0, z1 );

	x2 = _mm256_and_ps( x2, Zmask );
	y2 = _mm256_and_ps( y2, Zmask );
	z2 = _mm256_and_ps( z2, Zmask );

		x2 = _mm256_add_ps( x2, x3 );
		y2 = _mm256_add_ps( y2, y3 );
		z2 = _mm256_add_ps( z2, z3 );

	x0 = _mm256_add_ps( x0, x2 );
	y0 = _mm256_add_ps( y0, y2 );
	z0 = _mm256_add_ps( z0, z2 );
	
	_mm256_store_ps( res.data, x0 );
	_mm256_store_ps( res.data + 8, y0 );
	_mm256_store_ps( res.data + 16, z0 );


#elif defined(__SSE__)
	VerticesCoordsData res( 8 );

	__m128 Xmask = _mm_castsi128_ps( _mm_set_epi32( -1, 0, -1, 0 ) );
	__m128 Ymask = _mm_castsi128_ps( _mm_set_epi32( -1, -1, 0, 0 ) );

	__m128 x0 = _mm_load1_ps( &dat[0].x );
	__m128 y0 = _mm_load1_ps( &dat[0].y );
	__m128 z0 = _mm_load1_ps( &dat[0].z );

	__m128 x1 = _mm_load1_ps( &dat[1].x );
	__m128 y1 = _mm_load1_ps( &dat[1].y );
	__m128 z1 = _mm_load1_ps( &dat[1].z );

	__m128 x2 = _mm_load1_ps( &dat[2].x );
	__m128 y2 = _mm_load1_ps( &dat[2].y );
	__m128 z2 = _mm_load1_ps( &dat[2].z );

	__m128 x3 = _mm_load1_ps( &dat[3].x );
	__m128 y3 = _mm_load1_ps( &dat[3].y );
	__m128 z3 = _mm_load1_ps( &dat[3].z );

	x0 = _mm_and_ps( x0, Xmask );
	y0 = _mm_and_ps( y0, Xmask );
	z0 = _mm_and_ps( z0, Xmask );

	x1 = _mm_and_ps( x1, Ymask );
	y1 = _mm_and_ps( y1, Ymask );
	z1 = _mm_and_ps( z1, Ymask );

	x0 = _mm_add_ps( x0, x1 );
	y0 = _mm_add_ps( y0, y1 );
	z0 = _mm_add_ps( z0, z1 );

	x0 = _mm_add_ps( x0, x3 );
	y0 = _mm_add_ps( y0, y3 );
	z0 = _mm_add_ps( z0, z3 );

	_mm_store_ps( res.data, x0 );
	_mm_store_ps( res.data + 4, y0 );
	_mm_store_ps( res.data + 8, z0 );

	x0 = _mm_add_ps( x0, x2 );
	y0 = _mm_add_ps( y0, y2 );
	z0 = _mm_add_ps( z0, z2 );

	_mm_store_ps( res.data + 12, x0 );
	_mm_store_ps( res.data + 16, y0 );
	_mm_store_ps( res.data + 20, z0 );
#else
	VerticesCoordsData res( *GetVertices() );
#endif

	return res;
}

VectorVectRes OBB::GetVertices() const
{
	VectorVect q( 8 );;

	for( int i = 0; i < 8; i++ )
		q[i] = dat * glm::vec4( i & 1 ? 1 : 0, i & 2 ? 1 : 0, i & 4 ? 1 : 0, 1 );

	return q;
}

VectorVectRes OBB::GetNormals() const
{
	return VectorVect{
		glm::cross( dat[0], dat[1] ),
		glm::cross( dat[1], dat[2] ),
		glm::cross( dat[2], dat[0] ), 
	};
}

VectorVectRes OBB::GetEdges() const
{
	return VectorVect{ dat[0], dat[1], dat[2] };
}

OBB &OBB::rotate( const Mat4x4 &transform )
{
	assert( transform[0].w == 0 && transform[1].w == 0 && transform[2].w == 0 && transform[3].w == 1 );

	/*
	Vect p2 = transform * glm::vec4( p, 1 );
	for( int i = 0; i < 3; i++ ) v[i] = Vect( transform * glm::vec4( p + v[i], 1 ) ) - p2;
	p = p2;
	*/

	dat = transform * Mat4x4(dat);

	return *this;
}

struct MinMax
{
	float mi, ma;
	MinMax() : mi( INFINITY ), ma( -INFINITY ) {}
	MinMax( float f ) : mi( f ), ma( f ) {}
	MinMax( float mi, float ma ) : mi( mi ), ma( ma ) {}

	static MinMax merge( const MinMax &a, const MinMax &b ) { return MinMax{ glm::min( a.mi, b.mi ), glm::max( a.ma, b.ma ) }; }
};

/*
static IntersectResult SATAxisColides( const Vect &axis, const VectorVect &v1, const VectorVect &v2 )
{
	if( v1.size() == 0 || v2.size() == 0 ) return IntersectResult::NoCollision;

	auto Transform = [axis]( const Vect &v ) { return MinMax{ glm::dot( v, axis ) }; };

	MinMax A = std::transform_reduce( std::execution::unseq, v1.begin(), v1.end(), MinMax{}, MinMax::merge, Transform );
	MinMax B = std::transform_reduce( std::execution::unseq, v2.begin(), v2.end(), MinMax{}, MinMax::merge, Transform );

	if( A.ma < B.mi || B.ma < A.mi ) return IntersectResult::NoCollision;
	if( A.ma < B.ma && A.mi > B.mi ) return IntersectResult::AInB;
	if( B.ma < A.ma && B.mi > A.mi ) return IntersectResult::BInA;
	return IntersectResult::Collision;
}
*/

constexpr size_t VecLen =
#if defined(__AVX__)
8;
#elif defined(__SSE__)
4;
#else
1;
#endif

constexpr size_t VecSize = VecLen * sizeof( float );

constexpr size_t FastAllocatorMaxSize = 8;
static FastAlignedAllocator< FastAllocatorMaxSize * 3 * sizeof( float ), VecSize, 32 > allocator;

void collisions::detail::VerticesCoordsData::Create( size_t _size )
{ 
	size = ((_size + VecLen - 1) / VecLen) * VecLen;
	//size_t siz = this->size * VecSize;

	if( size == 0 ) data = NULL;
	else if( size <= FastAllocatorMaxSize ) data = (float *)allocator.Alloc();
	else data = (float *)_mm_malloc( size * 3 * sizeof(float), VecSize );
	/*
	if( size == 8 ) x = (float *)allocator.Alloc();
	else x = (float*)_mm_malloc( siz * 3, VecSize );
	y = (float *)((char *)x + siz);//(float *)_mm_malloc( siz, VecSize );
	z = (float *)((char *)y + siz);//(float *)_mm_malloc( siz, VecSize );
	*/
}

collisions::detail::VerticesCoordsData::~VerticesCoordsData()
{ 
	if( size <= FastAllocatorMaxSize ) allocator.Free( data );
	else _mm_free( data );

	data = nullptr;
	size = 0;
}

inline size_t collisions::detail::VerticesCoordsData::VectorCnt() const { return size / VecLen; }
inline float *collisions::detail::VerticesCoordsData::X( size_t i ) { return data + VecLen * ( 3*i + 0 ); }
inline float *collisions::detail::VerticesCoordsData::Y( size_t i ) { return data + VecLen * ( 3*i + 1 ); }
inline float *collisions::detail::VerticesCoordsData::Z( size_t i ) { return data + VecLen * ( 3*i + 2 ); }
inline const float *collisions::detail::VerticesCoordsData::X( size_t i ) const { return data + VecLen * (3 * i + 0); }
inline const float *collisions::detail::VerticesCoordsData::Y( size_t i ) const { return data + VecLen * (3 * i + 1); }
inline const float *collisions::detail::VerticesCoordsData::Z( size_t i ) const { return data + VecLen * (3 * i + 2); }

void collisions::detail::VerticesCoordsData::Create( const VectorVect &v )
{
	Create( v.size() );
	int i = 0, j = 0;
	// move data and for empty cells copy any other correct point

#if defined(__AVX2__)
	static_assert(VecLen >= 8);

	constexpr int sizeVect = sizeof( Vect );
	__m256i idx = _mm256_set_epi32(
		7 * sizeVect, 6 * sizeVect, 5 * sizeVect, 4 * sizeVect,
		3 * sizeVect, 2 * sizeVect, 1 * sizeVect, 0 * sizeVect );

		
	for( ; i < v.size() - VecLen + 1; i += VecLen, j++ )
	{
		__m256 _x = _mm256_i32gather_ps( ( (const float *)&v[i] ) + 0, idx, 1 );
		__m256 _y = _mm256_i32gather_ps( ( (const float *)&v[i] ) + 1, idx, 1 );
		__m256 _z = _mm256_i32gather_ps( ( (const float *)&v[i] ) + 2, idx, 1 );
		_mm256_store_ps( X(j), _x );
		_mm256_store_ps( Y(j), _y );
		_mm256_store_ps( Z(j), _z );
	}

	if( i < v.size() )
	{
		__m256i cmpv = _mm256_set1_epi32( (int)( v.size() - i - 1 ) * sizeVect );
		idx = _mm256_min_epi32( idx, cmpv );

		__m256 _x = _mm256_i32gather_ps( ((const float *)&v[i]) + 0, idx, 1 );
		__m256 _y = _mm256_i32gather_ps( ((const float *)&v[i]) + 1, idx, 1 );
		__m256 _z = _mm256_i32gather_ps( ((const float *)&v[i]) + 2, idx, 1 );
		_mm256_store_ps( X(j), _x );
		_mm256_store_ps( Y(j), _y );
		_mm256_store_ps( Z(j), _z );
	}
#else
	for( ; i < (int)v.size() - (int)VecLen + 1; j++ )
	{
		float *x = X( j ), *y = Y( j ), *z = Z( j );
		for( size_t l = 0; l < VecLen; l++, i++ )
		{
			x[l] = v[i].x;
			y[l] = v[i].y;
			z[l] = v[i].z;
		}
	}

	if( i < (int)v.size() )
	{
		float *x = X( j ), *y = Y( j ), *z = Z( j );
		size_t l = 0;
		for(; i < (int)v.size(); l++, i++ )
		{
			x[l] = v[i].x;
			y[l] = v[i].y;
			z[l] = v[i].z;
		}

		for(; l < VecLen; l++ )
		{
			x[l] = v[0].x;
			y[l] = v[0].y;
			z[l] = v[0].z;
		}
	}
#endif
}


#if defined(__AVX__)
#define DEFINE_HORISONTAL( op ) \
static float h ## op( __m128 v ) { \
__m128 shuf = _mm_movehdup_ps(v); \
__m128 sums = _mm_ ## op ## _ps( v, shuf ); \
shuf = _mm_movehl_ps( shuf, sums ); \
sums = _mm_ ## op ## _ss( sums, shuf ); \
return _mm_cvtss_f32( sums ); \
} \
static float h ## op( __m256 v ) { \
__m128 vlow = _mm256_castps256_ps128( v ); \
__m128 vhigh = _mm256_extractf128_ps( v, 1 ); \
vlow = _mm_ ## op ## _ps( vlow, vhigh ); \
return h ## op( vlow ); \
}
#elif defined(__SSE__)
#define DEFINE_HORISONTAL( op ) \
static float h ## op( __m128 v ) {                            \
__m128 shuf = _mm_shuffle_ps( v, v, _MM_SHUFFLE( 2, 3, 0, 1 ) );	\
__m128 sums = _mm_ ## op ## _ps( v, shuf );   \
shuf = _mm_movehl_ps( shuf, sums );    \
sums = _mm_ ## op ## _ss( sums, shuf );		\
return    _mm_cvtss_f32( sums );		\
}
#else
#define DEFINE_HORISONTAL( op ) //
#endif

DEFINE_HORISONTAL( max )
DEFINE_HORISONTAL( min )
#undef DEFINE_HORISONTAL

struct SATAxisCollidesOne8Axis
{
#if defined(__AVX__)
	__m256 X, Y, Z;
	SATAxisCollidesOne8Axis( const glm::vec3 &axis )
	{
		X = _mm256_broadcast_ss( &axis.x );
		Y = _mm256_broadcast_ss( &axis.y );
		Z = _mm256_broadcast_ss( &axis.z );
	}
#elif defined(__SSE__)
	__m128 X, Y, Z;
	SATAxisCollidesOne8Axis( const glm::vec3 &axis )
	{
		X = _mm_load1_ps( &axis.x );
		Y = _mm_load1_ps( &axis.y );
		Z = _mm_load1_ps( &axis.z );
	}
#else
	const glm::vec3 &axis;
	SATAxisCollidesOne8Axis( const glm::vec3 &axis ) : axis(axis) {}
#endif
};

static MinMax SATAxisCollidesOne8( const SATAxisCollidesOne8Axis &axis, const VerticesCoordsData &v )
{
	assert( v.size == 8 );
#if defined(__AVX__)
	__m256 x = _mm256_load_ps( v.X( 0 ) );
	__m256 y = _mm256_load_ps( v.Y( 0 ) );
	__m256 z = _mm256_load_ps( v.Z( 0 ) );

	x = _mm256_mul_ps( x, axis.X );
	y = _mm256_mul_ps( y, axis.Y );
	z = _mm256_mul_ps( z, axis.Z );

	x = _mm256_add_ps( x, y );
	x = _mm256_add_ps( x, z );

	__m128 hi = _mm256_extractf128_ps( x, 1 );
	__m128 lo = _mm256_castps256_ps128( x );

	__m128 mi = _mm_min_ps( hi, lo );
	__m128 ma = _mm_max_ps( hi, lo );

	return { hmin( mi ), hmax( ma ) };
#elif defined(__SSE__)
	__m128 hi, lo;

	{
		__m128 x = _mm_load_ps( v.X( 0 ) );
		__m128 y = _mm_load_ps( v.Y( 0 ) );
		__m128 z = _mm_load_ps( v.Z( 0 ) );

		x = _mm_mul_ps( x, axis.X );
		y = _mm_mul_ps( y, axis.Y );
		z = _mm_mul_ps( z, axis.Z );

		x = _mm_add_ps( x, y );
		lo = _mm_add_ps( x, z );
	}

	{
		__m128 x = _mm_load_ps( v.X( 1 ) );
		__m128 y = _mm_load_ps( v.Y( 1 ) );
		__m128 z = _mm_load_ps( v.Z( 1 ) );

		x = _mm_mul_ps( x, axis.X );
		y = _mm_mul_ps( y, axis.Y );
		z = _mm_mul_ps( z, axis.Z );

		x = _mm_add_ps( x, y );
		hi = _mm_add_ps( x, z );
	}

	__m128 mi = _mm_min_ps( hi, lo );
	__m128 ma = _mm_max_ps( hi, lo );

	return { hmin( mi ), hmax( ma ) };
#else
	MinMax r;
	for( size_t i = 0; i < v.size; i++ )
	{
		Vect q = Vect( *v.X( i ), *v.Y( i ), *v.Z( i ) );
		r = MinMax::merge( r, glm::dot( q, axis.axis ) );
	}
	return r;
#endif
}

static MinMax SATAxisCollidesOne8AnyLen( const SATAxisCollidesOne8Axis &axis, const VerticesCoordsData &v )
{
#if defined(__AVX__)
	__m256 mi = _mm256_set1_ps( INFINITY ), ma = _mm256_set1_ps( -INFINITY );
	for( size_t i = 0; i < v.VectorCnt(); i++ )
	{
		__m256 x = _mm256_load_ps( v.X( i ) );
		__m256 y = _mm256_load_ps( v.Y( i ) );
		__m256 z = _mm256_load_ps( v.Z( i ) );

		x = _mm256_mul_ps( x, axis.X );
		y = _mm256_mul_ps( y, axis.Y );
		z = _mm256_mul_ps( z, axis.Z );

		x = _mm256_add_ps( x, y );
		x = _mm256_add_ps( x, z );

		mi = _mm256_min_ps( mi, x );
		ma = _mm256_max_ps( ma, x );
	}

	return { hmin( mi ), hmax( ma ) };
#elif defined(__SSE__)
	__m128 mi = _mm_set1_ps( INFINITY ), ma = _mm_set1_ps( -INFINITY );
	for( size_t i = 0; i < v.VectorCnt(); i++ )
	{
		__m128 x = _mm_load_ps( v.X( i ) );
		__m128 y = _mm_load_ps( v.Y( i ) );
		__m128 z = _mm_load_ps( v.Z( i ) );

		x = _mm_mul_ps( x, axis.X );
		y = _mm_mul_ps( y, axis.Y );
		z = _mm_mul_ps( z, axis.Z );

		x = _mm_add_ps( x, y );
		x = _mm_add_ps( x, z );

		mi = _mm_min_ps( mi, x );
		ma = _mm_max_ps( ma, x );
	}

	return { hmin( mi ), hmax( ma ) };
#else
	MinMax r;
	for( size_t i = 0; i < v.size; i++ )
	{
		Vect q = Vect( *v.X( i ), *v.Y( i ), *v.Z( i ) );
		r = MinMax::merge( r, glm::dot( q, axis.axis ) );
	}
	return r;
#endif
}
/*
static MinMax SATAxisCollidesOne( const Vect &axis, const VerticesCoordsData &v )
{
	size_t i = 0;

	MinMax res;
#if 1
#if defined(__AVX__)
	__m256
		axisX256 = _mm256_broadcast_ss( &axis.x ),
		axisY256 = _mm256_broadcast_ss( &axis.y ),
		axisZ256 = _mm256_broadcast_ss( &axis.z );

	__m256 mi256, ma256;
	mi256 = _mm256_set1_ps( INFINITY );
	ma256 = _mm256_set1_ps( -INFINITY );

	for(; i < v.size - 7; i += 8 )
	{
		__m256 x = _mm256_load_ps( v.x + i );
		__m256 y = _mm256_load_ps( v.y + i );
		__m256 z = _mm256_load_ps( v.z + i );

		x = _mm256_mul_ps( x, axisX256 );
		y = _mm256_mul_ps( y, axisY256 );
		z = _mm256_mul_ps( z, axisZ256 );

		x = _mm256_add_ps( x, y );
		x = _mm256_add_ps( x, z );

		mi256 = _mm256_min_ps( mi256, x );
		ma256 = _mm256_max_ps( ma256, x );
	}
#endif

#if defined(__AVX__) || defined(__SSE__)	
#if defined(__AVX__)
	const __m128
		axisX128 = _mm256_castps256_ps128( axisX256 ),
		axisY128 = _mm256_castps256_ps128( axisY256 ),
		axisZ128 = _mm256_castps256_ps128( axisZ256 );

	__m128 mi128 = _mm_min_ps( _mm256_extractf128_ps( mi256, 1 ), _mm256_castps256_ps128( mi256 ) );
	__m128 ma128 = _mm_max_ps( _mm256_extractf128_ps( ma256, 1 ), _mm256_castps256_ps128( ma256 ) );
#else
	const __m128
		axisX128 = _mm_set1_ps( axis.x ),
		axisY128 = _mm_set1_ps( axis.y ),
		axisZ128 = _mm_set1_ps( axis.z );
	__m128 mi128 = _mm_set1_ps( INFINITY ), ma128 = _mm_set1_ps( -INFINITY );
#endif	
	
	for( ; i < v.size - 3; i += 4 )
	{
		__m128 x = _mm_load_ps( v.x + i );
		__m128 y = _mm_load_ps( v.y + i );
		__m128 z = _mm_load_ps( v.z + i );

		__m128 dx = _mm_mul_ps( x, axisX128 );
		__m128 dy = _mm_mul_ps( y, axisY128 );
		__m128 dz = _mm_mul_ps( z, axisZ128 );

		__m128 dot = _mm_add_ps( _mm_add_ps( dx, dy ), dz );

		mi128 = _mm_min_ps( mi128, dot );
		ma128 = _mm_max_ps( ma128, dot );
	}
	
	res.mi = hmin( mi128 );
	res.ma = hmax( ma128 );
#else
	MinMax r1, r2, r3, r4;

	for( ; i < v.size - 3; i += 4 )
	{
		Vect vec1( v.x[i + 0], v.y[i + 0], v.z[i + 0] );
		Vect vec2( v.x[i + 1], v.y[i + 1], v.z[i + 1] );
		Vect vec3( v.x[i + 2], v.y[i + 2], v.z[i + 2] );
		Vect vec4( v.x[i + 3], v.y[i + 3], v.z[i + 3] );

		r1 = MinMax::merge( r1, glm::dot( vec1, axis ) );
		r2 = MinMax::merge( r2, glm::dot( vec2, axis ) );
		r3 = MinMax::merge( r3, glm::dot( vec3, axis ) );
		r4 = MinMax::merge( r4, glm::dot( vec4, axis ) );
	}

	res = MinMax::merge( MinMax::merge( r1, r2 ), MinMax::merge( r3, r4 ) );
#endif
#endif
	for( ; i < v.size; i++ )
	{
		Vect vec( v.x[i], v.y[i], v.z[i] );
		res = MinMax::merge( res, glm::dot( vec, axis ) );
	}
	
	return res;
}
*/
static IntersectResult SATAxisCollides( const Vect &axis,  const VerticesCoordsData &v1, const VerticesCoordsData &v2 )
{
	SATAxisCollidesOne8Axis ax( axis );

	MinMax A = SATAxisCollidesOne8AnyLen( ax, v1 );
	MinMax B = SATAxisCollidesOne8AnyLen( ax, v2 );

	if( A.ma < B.mi || B.ma < A.mi ) return IntersectResult::NoCollision; // A< > B< > OR B< > A< >
	if( B.mi < A.mi && A.ma < B.ma ) return IntersectResult::AInB; // B< A< > >
	if( A.mi < B.mi && B.ma < A.ma ) return IntersectResult::BInA; // A< B< > >
	return IntersectResult::Collision; // A< B< \A> \B> OR B< A< \B> \A>
}

IntersectResult collisions::Intersects( const IntersectTestable *a, const IntersectTestable *b )
{
	//VerticesCoordsData va( *a->GetVertices() ), vb( *b->GetVertices() );
	VerticesCoordsData va( a->GetVerticesStream() ), vb( b->GetVerticesStream() );

	if( va.size <= 0 || vb.size <= 0 ) return IntersectResult::NoCollision;

	bool allIn = true;
	for( const Vect &n : *a->GetNormals() )
	{
		IntersectResult res = SATAxisCollides( n, va, vb );
		if( res == IntersectResult::NoCollision ) return IntersectResult::NoCollision;
		else allIn &= res == IntersectResult::BInA;
	}

	if( allIn ) return IntersectResult::BInA;

	allIn = true;
	for( const Vect &n : *b->GetNormals() )
	{
		//n = glm::normalize( n );
		IntersectResult res = SATAxisCollides( n, va, vb );
		if( res == IntersectResult::NoCollision ) return IntersectResult::NoCollision;
		else allIn &= res == IntersectResult::AInB;
	}

	if( allIn ) return IntersectResult::AInB;

	VectorVectRes ea = a->GetEdges(), eb = b->GetEdges();

	//In 3D, using face normals alone will fail to separate some edge-on-edge non-colliding cases. 
	//Additional axes, consisting of the cross-products of pairs of edges, one taken from each object, are required.
	for( const Vect &e1 : *ea )
	{
		for( const Vect &e2 : *eb )
		{
			Vect axis = glm::cross( e1, e2 );
			if( SATAxisCollides( axis, va, vb ) == IntersectResult::NoCollision )
				return IntersectResult::NoCollision;
		}
	}

	return IntersectResult::Collision;
}

void CameraCollider::Create( const Mat4x4 &view, float fov, float aspect, float near, float far )
{
	const float tanHalfFovy = tan( fov / 2.f );
	float right = aspect * tanHalfFovy;
	float top = tanHalfFovy;

	Mat4x4 VInv = glm::inverse( view );

	axis = glm::mat3( VInv ) * Vect( 0, 0, -1 );

	projectionHull.vert = VectorVect( 8 );

	//const Vect zero( 0 );
	const Vect p( 0, 0, -1 );
	const Vect r( right, 0, 0 );
	const Vect t( 0, top, 0 );
	for( int i = 0; i < 8; i++ )
		projectionHull.vert[i] = VInv * glm::vec4(
			(
				p +
				(i & 1 ? r : -r) +
				(i & 2 ? t : -t)
				) * (i & 4 ? near : far),
			1
		);

	const VectorVect &v = projectionHull.vert;

	projectionHull.edges = {
		// front/back edges
		v[0] - v[1],
		v[1] - v[2],
		// front to back edges
		v[0] - v[4],
		v[1] - v[5],
		v[2] - v[6],
		v[3] - v[7]
	};

	const VectorVect &e = projectionHull.edges;

	projectionHull.norm = {
		// front/back normal
		glm::cross( e[0], e[1] ),
		// front to back normals
		glm::cross( e[2], e[0] ),
		glm::cross( e[3], e[0] ),
		glm::cross( e[4], e[0] ),
		glm::cross( e[5], e[0] )
	};
}

VectorVectRes CameraCollider::GetVertices() const { return projectionHull.GetVertices(); }

CameraCollider::CameraCollider( const Mat4x4 &view, float fov, float aspect, float near, float far ) { Create( view, fov, aspect, near, far ); }

CameraCollider::VisibilityTestResult CameraCollider::AreVisible( const std::vector<OBB> &obbs )
{
	unsigned int siz = (unsigned int)obbs.size();
	std::vector<bool> vis( siz );
	std::vector<int> range( siz );
	std::iota( range.begin(), range.end(), 0 );

	MinMax MM = std::transform_reduce( range.begin(), range.end(), MinMax{}, MinMax::merge, 
	[this, &obbs, &vis]( int idx ) -> MinMax
	{
		const OBB &o = obbs[idx];
	
		vis[idx] = Intersects( (IntersectTestable *)&projectionHull, (IntersectTestable *)&o ) != IntersectResult::NoCollision;
		return SATAxisCollidesOne8( SATAxisCollidesOne8Axis(axis), o.GetVerticesStream() );
	} );

	CameraCollider::VisibilityTestResult res{
		.visible = std::move(vis),
		.minVis = MM.mi,
		.maxVis = MM.ma
	};

	return res;
}
