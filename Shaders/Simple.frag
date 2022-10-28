#version 450

layout( set=0, binding=3 ) uniform sampler2D tex;

layout( location=0 ) in VS_OUT {
    vec2 uv;
};

layout( location=0 ) out vec4 col;

void main() {
    col = vec4( texture( tex, uv ).rgb, 1 );
}