#pragma once

#ifndef COLLISIONS_INCLUDE_COLLISIONS_H
#define COLLISIONS_INCLUDE_COLLISIONS_H

#include "GlmHeaders.h"
#include <vector>
#include <memory>
#include <optional>

#ifdef _WIN32
#define DECLSPEC_NO_VTABLE __declspec(novtable)
#else
#define DECLSPEC_NO_VTABLE
#endif

namespace collisions
{
	namespace detail
	{
		template<class T>
		class MaybeOwningRef
		{
		protected:
			std::optional<T> o;
			const T *p;
		public:
			MaybeOwningRef( T &&o2 ) : o( o2 ), p( &o.value() ) {};
			MaybeOwningRef( const T &o2 ) : p( &o2 ) {};

			const T &operator*() const { return *p; };
			const T *operator->() const { return p; };
		};

		typedef glm::vec<3, float, glm::packed_highp> Vect;
		typedef glm::mat<4, 3, float, glm::packed_highp> Mat4x3;
		typedef glm::mat<4, 4, float, glm::packed_highp> Mat4x4;

		typedef std::vector<Vect> VectorVect;
		typedef std::vector<unsigned char> ElementIdxVect;

		typedef MaybeOwningRef<VectorVect> VectorVectRes;
		typedef MaybeOwningRef<ElementIdxVect> ElementIdxVectRes;

		class VerticesCoordsData 
		{
		public:
			float *data; // format <float X[vecSize], float Y[vecSize], float Z[vecSize]>[ ceil( size / vecSize ) ];
			size_t size;

			VerticesCoordsData() { data = nullptr; size = 0; }
			VerticesCoordsData( size_t size ) { Create( size ); }
			VerticesCoordsData( const VectorVect &v ) { Create( v ); }
			VerticesCoordsData( const VerticesCoordsData & ) = delete;
			VerticesCoordsData( VerticesCoordsData &&o ) noexcept { data = o.data, size = o.size; o.data = nullptr; o.size = 0;	}
			VerticesCoordsData &operator =( const VerticesCoordsData & ) = delete;
			VerticesCoordsData &operator =( VerticesCoordsData &&o ) noexcept {
				if( this != &o ) { data = o.data, size = o.size; o.data = nullptr; o.size = 0; }
				return *this;
			}

			inline void Create( size_t size );
			void Create( const VectorVect &v );
			~VerticesCoordsData();

			inline size_t VectorCnt() const;
			inline float *X( size_t i ); 
			inline float *Y( size_t i );
			inline float *Z( size_t i );
			inline const float *X( size_t i ) const;
			inline const float *Y( size_t i ) const;
			inline const float *Z( size_t i ) const;
		};

		class DECLSPEC_NO_VTABLE IntersectTestable
		{
		public:
			virtual VectorVectRes GetVertices() const = 0;
			virtual VectorVectRes GetNormals() const = 0;
			virtual VectorVectRes GetEdges() const = 0;

			virtual VerticesCoordsData GetVerticesStream() const;
		};

		class DECLSPEC_NO_VTABLE Drawable
		{
		public:
			virtual VectorVectRes GetVertices() const = 0;
			virtual ElementIdxVectRes Triangles() const = 0;
			virtual ElementIdxVectRes Lines() const = 0;
		};

		class DECLSPEC_NO_VTABLE Box : public Drawable
		{
		public:
			virtual ElementIdxVectRes Triangles() const;
			virtual ElementIdxVectRes Lines() const;
		};
	};

	enum class IntersectResult
	{
		NoCollision,
		AInB, BInA,
		Collision
	};

	IntersectResult Intersects( const detail::IntersectTestable *, const detail::IntersectTestable * );

	class AABB :public detail::IntersectTestable, public detail::Box
	{
	public:
		detail::Vect lo, hi;

		AABB() : lo( 0 ), hi( 0 ) {};
		AABB( const detail::Vect &lo, const detail::Vect &hi ) : lo( lo ), hi( hi ) {};

		AABB &operator+=( const detail::Vect &v ) { lo = glm::min( lo, v ), hi = glm::max( hi, v ); return *this; };
		AABB operator+( const detail::Vect &v ) const { AABB a = *this; return a += v; };
		AABB &operator+=( const AABB &rhs ) { lo = glm::min( lo, rhs.lo ), hi = glm::max( hi, rhs.hi ); return *this; };
		AABB operator+( const AABB &rhs ) const { AABB a = *this; return a += rhs; };

		virtual detail::VectorVectRes GetVertices() const;
		virtual detail::VectorVectRes GetNormals() const;
		virtual detail::VectorVectRes GetEdges() const;

		bool Intersects( const AABB &rhs ) const;
	};

	class OBB : public detail::IntersectTestable, public detail::Box
	{
	public:
		detail::Mat4x3 dat;

		OBB();
		OBB( const detail::Vect &p, const detail::Vect &v1, const detail::Vect &v2, const detail::Vect &v3 );
		OBB( const AABB &o );

		AABB GetAABB() const;
		OBB &rotate( const detail::Mat4x4 &transform );

		virtual detail::VectorVectRes GetVertices() const;
		virtual detail::VectorVectRes GetNormals() const;
		virtual detail::VectorVectRes GetEdges() const;

		virtual detail::VerticesCoordsData GetVerticesStream() const;
	};

	class ConvexPolyhedron : public detail::IntersectTestable
	{
	public:
		detail::VectorVect vert, norm, edges;

		virtual detail::VectorVectRes GetVertices() const { return vert; };
		virtual detail::VectorVectRes GetNormals() const { return norm; };
		virtual detail::VectorVectRes GetEdges() const { return edges; };
	};

	class CameraCollider : public detail::Box
	{
	public:
		//glm::mat4 View;
		//float near, far, right, top;
		ConvexPolyhedron projectionHull;
		detail::Vect axis;

		virtual detail::VectorVectRes GetVertices() const;

		CameraCollider() = default;
		CameraCollider( const detail::Mat4x4 &view, float fov, float aspect, float _near, float _far );
		void Create( const detail::Mat4x4 &view, float fov, float aspect, float _near, float _far );

		struct VisibilityTestResult
		{
			std::vector<bool> visible;
			float minVis, maxVis;
		};

		VisibilityTestResult AreVisible( const std::vector<OBB> &obbs );
	};
} // collisions

#endif // ndef COLLISIONS_INCLUDE_COLLISIONS_H