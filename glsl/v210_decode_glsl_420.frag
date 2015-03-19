#fb_include <v210_decode_constants.glsl>

layout(binding = 0) uniform usampler2D v210_input_image;

#ifdef FB_VENDOR_AMD
layout(binding = 1) uniform writeonly image2D rgba_output_image;
#else
layout(binding = 1) uniform restrict writeonly image2D rgba_output_image;
#endif

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

    // Based on gl_FragCoord, get the group number
    ivec2 target_coords = ivec2(gl_FragCoord.xy);

    // Number of the compressed group
    // e.g. x = 960 -> group_num = 160
    int v210_x_group_base_num = target_coords.x;
    int v210_y_line_num = target_coords.y;

    // TODO: check if it were faster NOT using the luma and using if's
    
    // Note that the +1 here accomodates the special case of the assymetric
    // filter (think it through with a filter_v210_width of 1 what happens
    // when the pixel_width == 2.
    uint[(1+chroma_filter_v210_width)*6] luma;
    uvec2[(1+chroma_filter_v210_width)*3] chroma;

    for (int i = 0; i<(chroma_filter_v210_width+1); ++i)
    {
       
        int v210_x_group_num = v210_x_group_base_num - chroma_filter_v210_offset_left + i;
        const uint luma_base_index = i*6;
        const uint chroma_base_index = luma_base_index/2;

        #fb_include <v210_decode_ycbcr_lookup.glsl>

    }

    // This is the layout of the array 'words', 
    // with the indices for horizontal sample location
    // | Cb_0, Y'_0, Cr_0 | Y'_1, Cb_2, Y'_2 | Cr_2, Y'_3, Cb_4 | Y'_4, Cr_4, Y'_5 |

#ifdef FB_GLSL_FLIP_ORIGIN

    target_coords.y = v210_input_image_bounds.y - target_coords.y;

#endif

    // This is the index into the accumulated array
    const uint rgba_horiz_pixel_base_coord = chroma_filter_v210_offset_left*6;

    const uvec2 rgba_pixel_base_coords = uvec2(v210_x_group_base_num*6, target_coords.y);

    #fb_include <v210_decode_ycbcr_to_rgb_and_store.glsl>

}
