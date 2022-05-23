#include "/include/GLSLInit.glsl"
out vec2 vPos;

out gl_PerVertex
{
    vec4 gl_Position;
    float gl_PointSize;
    float gl_ClipDistance[];
};

const vec2 vOutputPos[4] = 
{
    { -1, -1 },
    { 1 , -1 },
    { 1 ,  1 },
    { -1,  1 }
};

const vec2 vInputPos[4] =
{
    { 0, 0 },
    { 1, 0 },
    { 1, 1 },
    { 0, 1 }
};

void main()
{
	gl_Position = vec4(vOutputPos[gl_VertexID],0,1);
	vPos = vInputPos[gl_VertexID];
}
