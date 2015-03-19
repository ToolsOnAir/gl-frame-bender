#fb_include <v210_decode_constants.glsl>

layout(binding = 0) uniform usampler2D v210_input_image;

#ifdef FB_GLSL_FLIP_ORIGIN
layout(pixel_center_integer, origin_upper_left) in vec4 gl_FragCoord;
#else
layout(pixel_center_integer) in vec4 gl_FragCoord;
#endif

in block
{
    vec2 texcoord;
} frag_in;

layout(location = FRG_OUT_COLOR, index = 0) out vec4 out_color;

void main()
{

    // gl_FragCoord here gives us number of pixel for OUTPUT (uncompressed)
    // image.
    ivec2 target_coords = ivec2(gl_FragCoord.xy);

    // Derive the V210 group number from the output coordinates
    // e.g. x = 960 -> group_num = 160
    int v210_x_group_base_num = target_coords.x / 6;
    int v210_y_line_num = target_coords.y;

    // group_offset == 0
    int rgba_horiz_pixel_offset = target_coords.x % 6;

    uint[(1+chroma_filter_v210_width)*6] luma;
    uvec2[(1+chroma_filter_v210_width)*3] chroma;

    for (int i = 0; i<(chroma_filter_v210_width+1); ++i)
    {

        int v210_x_group_num = v210_x_group_base_num - chroma_filter_v210_offset_left + i;
        const uint luma_base_index = i*6;
        const uint chroma_base_index = luma_base_index/2;

        #fb_include <v210_decode_ycbcr_lookup.glsl>

    }    

    // This is the index into the accumulated array
    const uint rgba_horiz_pixel_base_coord = chroma_filter_v210_offset_left*6;

    #fb_include <v210_decode_ycbcr_to_rgb_and_store.glsl>

}
