#version 420 core

#define VTX_ATTR_POSITION	0
#define VTX_ATTR_TEXCOORD0	1
#define FRG_OUT_COLOR   0
#define TRANSFORM0      1

layout(binding = TRANSFORM0) uniform transform
{
    mat4 MVP;
} Transform;

layout(location = VTX_ATTR_POSITION) in vec2 in_position;
layout(location = VTX_ATTR_TEXCOORD0) in vec2 in_texcoord;

out vec4 out_position;

out gl_PerVertex
{
    vec4 gl_Position;
};

out block
{
    vec2 texcoord;
} vtx_out;

void main()
{	
    // TODO: add transform for animation
    vtx_out.texcoord = in_texcoord;
    gl_Position = Transform.MVP * vec4(in_position, 0.0, 1.0);
}