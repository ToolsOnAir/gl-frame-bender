#fb_include <v210_encode_constants.glsl>

layout(binding = 0) uniform sampler2D rgba_input_image;

#ifdef FB_GLSL_FLIP_ORIGIN
layout(pixel_center_integer, origin_upper_left) in vec4 gl_FragCoord;
#else
layout(pixel_center_integer) in vec4 gl_FragCoord;
#endif

in block
{
    vec2 texcoord;
} frag_in;

#ifdef FB_GLSL_V210_EXTRACT_BITFIELDS_IN_SHADER
layout(location = FRG_OUT_COLOR, index = 0) out uint out_v210_word;
#else
layout(location = FRG_OUT_COLOR, index = 0) out uvec4 out_v210_word;
#endif

void main()
{

    ivec2 target_coords = ivec2(gl_FragCoord.xy);

    const int v210_x_group_base_num = target_coords.x / 4;
    const int v210_y_line_num = target_coords.y;
    
    // e.g. 3
    int v210_x_group_offset = target_coords.x % 4;

    const int neighbourhood_pixel_size = 6 + chroma_filter_pixel_offset_left + chroma_filter_pixel_offset_right;

    uvec3 y_cb_cr_444[neighbourhood_pixel_size];

    for (int i = 0; i<neighbourhood_pixel_size; ++i)
    {
    
        int rgba_input_image_pixel_coord_x = clamp(
            v210_x_group_base_num * 6  - chroma_filter_pixel_offset + i,
            0,
            rgba_input_image_bounds.x);

        ivec2 rgba_input_image_pixel_coords = ivec2(
            rgba_input_image_pixel_coord_x,
            v210_y_line_num
            );

        int y_cb_cr_dst_index = i;

        #fb_include <v210_encode_lookup_and_rgba_to_ycbcr.glsl>

    }

    const uint rgba_horiz_pixel_base_coord = chroma_filter_pixel_offset;

    #fb_include <v210_encode_subsample_ycbcr_pack_and_store.glsl>

}
