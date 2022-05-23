#include "/include/GLSLInit.glsl"
#include "/include/Lights.glsl" //! #include "./include/Lights.glsl"

layout(triangles) in;
layout(triangle_strip, max_vertices=32) out; // 128*3/32

in gl_PerVertex
{
	vec4 gl_Position;
} gl_in[];

out gl_PerVertex
{
	vec4 gl_Position;
};

out GS_OUT
{
    vec3 vTexAlpha; // <vec2 tex, float alpha>
    flat uint vMaterialIdx; //  material
} gs_out;

in VS_OUT
{
	vec3 vTexAlpha; // <vec2 tex, float alpha>
    flat uint vMaterialIdx; // material
} gs_in[];

void main()
{
	for( int i = 0; i < MAX_SHADOW_MAPS; i++ )
	{
		if( ( baShadowMapActive[ i >> 7 ][ (i>>5) & 3] & ( 1 << (i&31) ) ) != 0 ) //PACKED_BITS_GET( baShadowMapActive, i ) )
		{
			int idx = i;// + j;
			gl_Layer = idx;
			for( int q = 0; q < gl_in.length(); q++ )
			{
				gl_Position = amShadowMatrices[idx] * gl_in[q].gl_Position;

				gs_out.vTexAlpha = gs_in[q].vTexAlpha;
				gs_out.vMaterialIdx = gs_in[q].vMaterialIdx;

				EmitVertex();
			}
			EndPrimitive();
		}
	}
}