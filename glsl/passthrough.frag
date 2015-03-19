#version 420 core

#define VTX_ATTR_POSITION	0
#define VTX_ATTR_TEXCOORD0	1
#define FRG_OUT_COLOR   0

layout(binding = 0) uniform sampler2D video_sampler;

in block
{
    vec2 texcoord;
} frag_in;

layout(location = FRG_OUT_COLOR, index = 0) precise out vec4 out_color;

void main()
{
    ivec2 tex_size = textureSize(video_sampler, 0);
    ivec2 tex_coord = ivec2(tex_size * frag_in.texcoord);
    out_color = texelFetch(video_sampler, tex_coord, 0);
}
