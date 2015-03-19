#fb_include <v210_decode_constants.glsl>

const int tile_v210_width = TILE_V210_WIDTH;

layout(binding = 0) uniform usampler2D v210_input_image;

#ifdef FB_VENDOR_AMD
layout(binding = 1) uniform writeonly image2D rgba_output_image;
#else
layout(binding = 1) uniform restrict writeonly image2D rgba_output_image;
#endif

// TODO: what could be an advantage of adding Y as dimension?
// None, you shared data would be unnecessarily high.
layout(local_size_x=TILE_V210_WIDTH) in;

void main() {

    const ivec2 tile_xy = ivec2(gl_WorkGroupID);
    const uint thread_x = gl_LocalInvocationID.x;
    const ivec2 v210_coords = tile_xy*ivec2(tile_v210_width, 1) + ivec2(thread_x, 0);
    const int v210_y_line_num = v210_coords.y;
    const uint x = thread_x;
    const int v210_x_group_base_num = v210_coords.x;
    
#ifdef FB_GLSL_FLIP_ORIGIN
    const uvec2 rgba_pixel_base_coords = uvec2(v210_coords.x * 6, v210_input_image_bounds.y - v210_coords.y);    
#else 
    const uvec2 rgba_pixel_base_coords = uvec2(v210_coords.x * 6, v210_coords.y);
#endif

#ifdef FB_GLSL_DEAL_WITH_EXCESS_THREADS
    // We might have had to spawn more threads than which are actually
    // contributing to image
    if (rgba_pixel_base_coords.x < ((v210_image_size.x/4)*6)) {
#endif

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

        // This is the index into the accumulated array
        const uint rgba_horiz_pixel_base_coord = chroma_filter_v210_offset_left*6;

        #fb_include <v210_decode_ycbcr_to_rgb_and_store.glsl>
#ifdef FB_GLSL_DEAL_WITH_EXCESS_THREADS
    }
#endif

}

