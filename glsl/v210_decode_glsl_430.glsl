#fb_include <v210_decode_constants.glsl>

const int tile_v210_width = TILE_V210_WIDTH;

// i == in V210 domain, unit = 4 byte-tuple
const int neighborhood_v210_size = (tile_v210_width + chroma_filter_v210_width);

// unit = pixels
const int neighborhood_pixel_size = neighborhood_v210_size * 6;

// Declare the input and output images.
// TODO: use 10_10_10_2 as conversion for lookup?
layout(binding = 0) uniform usampler2D v210_input_image;

#ifdef FB_VENDOR_AMD
layout(binding = 1) uniform writeonly image2D rgba_output_image;
#else
layout(binding = 1) uniform restrict writeonly image2D rgba_output_image;
#endif

// TODO: what could be an advantage of adding Y as dimension?
// None, you shared data would be unnecessarily high.
layout(local_size_x=TILE_V210_WIDTH) in;

// Note that we don't need to store the reconstructed Y values
// TODO-DEF: in the future, we might actually want this in order to perform
// de-interlacing?
shared uint luma[neighborhood_pixel_size];
shared uvec2 chroma[neighborhood_pixel_size/2];

void main() {

    const ivec2 tile_xy = ivec2(gl_WorkGroupID);
    const uint thread_x = gl_LocalInvocationID.x;
    const ivec2 v210_coords = tile_xy*ivec2(tile_v210_width, 1) + ivec2(thread_x, 0);
    const int v210_y_line_num = v210_coords.y;
    const uint x = thread_x;
    
    // Which domain is our computational problem in?
    // -> V210 4-byte domain, one unit == 4 x 1 byte word

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
        // Phase 1: Read-in & decompose luma and chroma from uint's

        for (int i=0; i < neighborhood_v210_size; i += tile_v210_width) {

            if ( (x + i) < neighborhood_v210_size) {           

                const int v210_x_group_num = int(v210_coords.x) + i - chroma_filter_v210_offset_left;

                const uint luma_base_index = (x+i)*6;
                const uint chroma_base_index = luma_base_index/2;

                #fb_include <v210_decode_ycbcr_lookup.glsl>

            }

        }

        memoryBarrierShared(); 
        barrier();

        // Phase 2: Reconstruct chroma values (assuming 422 subsampling)
        // Phase 3: convert to RGBA
        // Phase 4: write out to image
    
        // Coords into the shared array   
        uint rgba_horiz_pixel_base_coord = (x+chroma_filter_v210_offset_left)*6;

        #fb_include <v210_decode_ycbcr_to_rgb_and_store.glsl>        

#ifdef FB_GLSL_DEAL_WITH_EXCESS_THREADS
    }
#endif


}

