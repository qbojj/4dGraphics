#version 450
#include "include/GLSLInit.glsl"
out vec4 vFragColor;
in vec2 vPos;

uniform sampler2D tNormals, tColors, tDepth;
layout(pixel_center_integer) in vec4 gl_FragCoord;

/*
const float Lx[9] ={
	1, 0,-1,
	2, 0,-2,
	1, 0,-1,
};

const float Ly[9] = {
	 1, 2, 1,
	 0, 0, 0,
	-1,-2,-1,
};
*/

const float EdgeKernel[9] = {
	-1, -1, -1,
	-1,  8, -1,
	-1, -1, -1
};

const ivec2 offsets[9] = {
	{-1,-1}, {-1, 0}, {-1, 1},  
	{ 0,-1}, { 0, 0}, { 0, 1},
	{ 1,-1}, { 1, 0}, { 1, 1},
};

vec4[9] GetValues( sampler2D tex, vec2 pos, mat2 dMat )
{
	vec4 v[9];

	for( int i = 0; i < 9; i++ )
		v[i] = texture( tex, pos + dMat * offsets[i] );

	return v;
}

vec3 rgb2hsv(vec3 c)
{
	vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
	vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
	vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

	float d = q.x - min(q.w, q.y);
	float e = 1.0e-10;
	return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c)
{
	vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
	vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
	return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

uniform float fNear = 0.00001;//, fFar = 10000.;

float linearize_depth(float d)
{
	return fNear / d;//fNear * fFar / (fFar + d * (fNear - fFar));
}

vec4 kernelMult( vec4 data[9], float kernel[9] )
{ vec4 sum = vec4(0); for( int i = 0; i < 9; i++ ) sum += data[i] * kernel[i]; return sum; }
vec3 kernelMult( vec3 data[9], float kernel[9] )
{ vec3 sum = vec3(0); for( int i = 0; i < 9; i++ ) sum += data[i] * kernel[i]; return sum; }
vec2 kernelMult( vec2 data[9], float kernel[9] )
{ vec2 sum = vec2(0); for( int i = 0; i < 9; i++ ) sum += data[i] * kernel[i]; return sum; }
float kernelMult( float data[9], float kernel[9] )
{ float sum = 0;      for( int i = 0; i < 9; i++ ) sum += data[i] * kernel[i]; return sum; }

void main()
{
	//ivec2 pos = ivec2( round(gl_FragCoord.xy) );

	mat2 dMat = mat2( dFdx( vPos ), dFdy( vPos ) );

	vec4 Norm[9] = GetValues( tNormals, vPos, dMat );
	vec4 Depth[9] = GetValues( tDepth, vPos, dMat );
  
	float DepthEdge;
	{
		float LinearDepth[9];
		for( int i = 0; i < 9; i++ ) LinearDepth[i] = linearize_depth( Depth[i].x );

		DepthEdge = kernelMult( LinearDepth, EdgeKernel );
	}
	float NormalEdge;
	{
		vec4 val = kernelMult( Norm, EdgeKernel );

		NormalEdge = length( val.xyz );
	}
  
	bool isDepthEdge = DepthEdge > 3;
	bool isNormalEdge = NormalEdge > 1;

	bool isEdge = isDepthEdge || isNormalEdge;
	//vFragColor = vec4(isDepthEdge,isNormalEdge,0,1);
	vFragColor = texture( tColors, vPos ) * int(!isEdge);
}