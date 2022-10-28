#version 450

layout( set=0, binding=0 ) uniform perFrame_t {
    mat4 MVP;
};

struct vertex {
    float x;
    float y;
    float z;
    float u;
    float v;
};

layout( set=0, binding=1 ) buffer readonly restrict verteces_t{
    vertex verteces[];
};

layout( set=0, binding=2 ) buffer readonly restrict indences_t{
    uint indences[];
};

layout( location=0 ) out VS_OUT {
    vec2 uv;
};

out gl_PerVertex {
    vec4 gl_Position;
};

const float triangleConst = .5f;
const vertex triangle[3] = {
    { -triangleConst, -triangleConst, .5, 0, 0 },
    {  triangleConst, -triangleConst, .5, 0, 1 },
    {             .0,  triangleConst, .5, 1,.5 },
};

void main()
{
    uint idx = indences[gl_VertexIndex];
    vertex v = verteces[idx];
    vec3 V = vec3( v.x, v.y, v.z );
    gl_Position = MVP * vec4( V + vec3( 10 * gl_InstanceIndex,0,0), 1 );//vec4( V / 50.f + vec3( 0, 0, .8 ), 1 ); 
    uv = vec2( v.u, v.v );
} 