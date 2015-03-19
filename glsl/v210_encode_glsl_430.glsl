#fb_include <v210_encode_constants.glsl>

const int tile_v210_width = TILE_V210_WIDTH;

const int chroma_filter_v210_offset_left = (chroma_filter_pixel_offset_left + 5) / 6;
const int chroma_filter_v210_offset_right = (chroma_filter_pixel_offset_right + 5) / 6;

const int neighborhood_v210_size = tile_v210_width + chroma_filter_v210_offset_left + chroma_filter_v210_offset_right;
const int neighborhood_pixel_size = neighborhood_v210_size*6;

// Declare the input and output images.

layout(binding = 0) uniform sampler2D rgba_input_image;

#ifdef FB_VENDOR_AMD
layout(binding = 1) uniform writeonly uimage2D v210_output_image;
#else
layout(binding = 1) uniform restrict writeonly uimage2D v210_output_image;
#endif

layout(local_size_x=TILE_V210_WIDTH) in;

shared uvec3 y_cb_cr_444[neighborhood_pixel_size];

void main() {

    const ivec2 tile_xy = ivec2(gl_WorkGroupID);
    const uint thread_x = gl_LocalInvocationID.x;
    const ivec2 v210_coords = tile_xy*ivec2(tile_v210_width, 1) + ivec2(thread_x, 0);
    int v210_y_line_num = v210_coords.y;
    const uint x = thread_x;

    int v210_x_group_base_num = v210_coords.x;

    // Which domain is our computational problem in?
    // -> V210 4-byte domain, one unit == 4 x 1 byte word, or in RGBA values
    // it's a group of 6 pixels.

#ifdef FB_GLSL_FLIP_ORIGIN
    const uvec2 rgba_pixel_base_coords = uvec2(v210_coords.x * 6, rgba_input_image_bounds.y - v210_coords.y);    
#else 
    const uvec2 rgba_pixel_base_coords = uvec2(v210_coords.x * 6, v210_coords.y);
#endif

#ifdef FB_GLSL_DEAL_WITH_EXCESS_THREADS
    // We might have had to spawn more threads than which are actually
    // contributing to image
    if (rgba_pixel_base_coords.x < rgba_input_image_size.x) {
#endif
        
        // Phase 1: Read-in RGBA and convert to YCbCr with sRGB

        for (int i = 0; i < neighborhood_v210_size; i += tile_v210_width) {

            if ( (x + i) < neighborhood_v210_size) {
            
                for (int j = 0; j < 6; ++j) {

                    const int y_cb_cr_dst_index = int(x+i)*6 + j;
    
                    const int rgba_input_image_pixel_coord_x = clamp(
                        (v210_coords.x - chroma_filter_v210_offset_left + i)*6 + j,
                        0,
                        rgba_input_image_bounds.x);

                    const ivec2 rgba_input_image_pixel_coords = ivec2(
                        rgba_input_image_pixel_coord_x,
                        rgba_pixel_base_coords.y
                        );  

                    #fb_include <v210_encode_lookup_and_rgba_to_ycbcr.glsl>

                }

            }

        }

        memoryBarrierShared(); 
        barrier();

        // Phase 2: Filter chroma values, pack to V210 and store
    
        // Coords into the shared array   
        uint rgba_horiz_pixel_base_coord = (x + chroma_filter_v210_offset_left)*6;

        #fb_include <v210_encode_subsample_ycbcr_pack_and_store.glsl>

#ifdef FB_GLSL_DEAL_WITH_EXCESS_THREADS
    }
#endif


}

