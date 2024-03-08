#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require

layout(buffer_reference, scalar, buffer_reference_align=4) buffer VertexData {
    vec3 position;
    vec3 normal;
    vec2 texcoord;
    vec4 color;
};

layout(buffer_reference, scalar) buffer Uniforms {
    mat4 view, projection;
    mat4 VP;

    float time;
};

layout(push_constant) uniform PushConstants {
    mat4 model;
    VertexData vertex;
    Uniforms uniforms;
};

struct VertexOutput {
    vec4 position;
    vec3 normal;
    vec2 texcoord;
    vec4 color;
};

