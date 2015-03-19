// Inputs required for this snippet:
//      v210_x_group_num == v210 horizontal unit (in per 4-byte-units)
//      v210_y_line_num == v210 vertical line number
//      v210_input_image == usampler2D with the V210 image texture.
//      v210_input_image_group_bounds == the bounds for clamping (use decode constants for that...)
//      luma == uint[] destination array for luma values
//      luma_base_index == first index into the luma array
//      chroma == uvec2[] destination array for chroma values
//      chroma_base_index == first index into the chroma dst array

const int v210_img_x = v210_x_group_num*4;

const ivec2 read_at_base = clamp(
    ivec2(v210_img_x, v210_y_line_num), 
    ivec2(0, 0), 
    v210_input_image_group_bounds);        
                
const bool clamp_right = v210_img_x > v210_input_image_group_bounds.x;
const bool clamp_left = v210_img_x < 0;

// TODO: could we fuse this loop somehow with the writing to the YCbCr
// values?

uvec3 v210_unpacked_words[4];
for (int v210_word_idx = 0; v210_word_idx < 4; ++v210_word_idx)
{

    const ivec2 read_at = read_at_base + ivec2(v210_word_idx, 0);

#ifdef FB_GLSL_V210_EXTRACT_BITFIELDS_IN_SHADER

    const uint v210_packed = texelFetch(v210_input_image, read_at, 0).r;
    v210_unpacked_words[v210_word_idx] = uncompress_v210_components_from_word(v210_packed);

#else

    const uvec3 v210_uncompressed = texelFetch(v210_input_image, read_at, 0).rgb;
    v210_unpacked_words[v210_word_idx] = v210_uncompressed;

#endif

}

// This is the layout of the array 'words', 
// with the indices for horizontal sample location
// | Cb_0, Y'_0, Cr_0 | Y'_1, Cb_2, Y'_2 | Cr_2, Y'_3, Cb_4 | Y'_4, Cr_4, Y'_5 |

luma[luma_base_index]     = v210_unpacked_words[0][1]; // Y'_0
luma[luma_base_index + 1] = v210_unpacked_words[1][0]; // Y'_1
luma[luma_base_index + 2] = v210_unpacked_words[1][2]; // Y'_2
luma[luma_base_index + 3] = v210_unpacked_words[2][1]; // Y'_3
luma[luma_base_index + 4] = v210_unpacked_words[3][0]; // Y'_4
luma[luma_base_index + 5] = v210_unpacked_words[3][2]; // Y'_5

if (clamp_left) {

    chroma[chroma_base_index][0]     = v210_unpacked_words[0][0]; // Cb_0
    chroma[chroma_base_index][1]     = v210_unpacked_words[0][2]; // Cr_0
    chroma[chroma_base_index + 1][0] = v210_unpacked_words[0][0]; // Cb_0
    chroma[chroma_base_index + 1][1] = v210_unpacked_words[0][2]; // Cr_0
    chroma[chroma_base_index + 2][0] = v210_unpacked_words[0][0]; // Cb_0
    chroma[chroma_base_index + 2][1] = v210_unpacked_words[0][2]; // Cr_0

} else if (clamp_right) {

    chroma[chroma_base_index][0]     = v210_unpacked_words[2][2]; // Cb_4
    chroma[chroma_base_index][1]     = v210_unpacked_words[3][1]; // Cr_4
    chroma[chroma_base_index + 1][0] = v210_unpacked_words[2][2]; // Cb_4
    chroma[chroma_base_index + 1][1] = v210_unpacked_words[3][1]; // Cr_4
    chroma[chroma_base_index + 2][0] = v210_unpacked_words[2][2]; // Cb_4
    chroma[chroma_base_index + 2][1] = v210_unpacked_words[3][1]; // Cr_4                    

} else {

    chroma[chroma_base_index][0]     = v210_unpacked_words[0][0]; // Cb_0
    chroma[chroma_base_index][1]     = v210_unpacked_words[0][2]; // Cr_0
    chroma[chroma_base_index + 1][0] = v210_unpacked_words[1][1]; // Cb_2
    chroma[chroma_base_index + 1][1] = v210_unpacked_words[2][0]; // Cr_2
    chroma[chroma_base_index + 2][0] = v210_unpacked_words[2][2]; // Cb_4
    chroma[chroma_base_index + 2][1] = v210_unpacked_words[3][1]; // Cr_4

}    