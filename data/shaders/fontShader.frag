#include "/include/GLSLInit.glsl"
out vec4 FragColor;

uniform sampler2D tTex;
uniform vec4 vTextColor;

in vec2 TexCoords;

void main()
{    
    FragColor = vTextColor * texture(tTex, TexCoords);
}