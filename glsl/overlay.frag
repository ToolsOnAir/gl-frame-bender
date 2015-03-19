#version 420 core

#define VTX_ATTR_POSITION	0
#define VTX_ATTR_TEXCOORD0	1
#define FRG_OUT_COLOR   0
#define BLEND_UNIFORMS  2

layout(binding = 0) uniform sampler2D overlay_sampler;

layout(binding = BLEND_UNIFORMS) uniform blend
{
    float opacity;
} Blend;

in block
{
    vec2 texcoord;
} frag_in;

layout(location = FRG_OUT_COLOR, index = 0) out vec4 out_color;

void main()
{
    vec4 tex_col = texture(overlay_sampler, frag_in.texcoord);
    // TODO: replace with linearized function
    tex_col.xyz = pow(tex_col.xyz, vec3(2.2));
    tex_col.a = tex_col.a * Blend.opacity;
    out_color = tex_col;
}
