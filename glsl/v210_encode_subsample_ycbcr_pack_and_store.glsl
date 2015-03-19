// Input requirements:
//      rgba_horiz_pixel_base_coord
//      y_cb_cr_444
//      chroma_filter_pixel_weights
//      chroma_filter_pixel_width
//      chroma_filter_pixel_offset
//      v210_output_image
//      v210_x_group_base_num
//      v210_y_line_num
//      rgba_input_image_bounds

// | Cb_0, Y'_0, Cr_0 | Y'_1, Cb_2, Y'_2 | Cr_2, Y'_3, Cb_4 | Y'_4, Cr_4, Y'_5 |

// For the group of 6 pixels, we will write out exactly 4 V210 words
uvec3 v210_words[4];// = {uvec3(512), uvec3(512), uvec3(512), uvec3(512) };

// Filter for 'even' sample numbers (422 subsampling)
    
// Write out the CbCr values and perform downsampling before
int cb_idx = 0; // first write index for Cb
int cr_idx = 2; // first write index for Cr

for (uint rgba_horiz_pixel_offset = 0; 
     rgba_horiz_pixel_offset < 6; 
     rgba_horiz_pixel_offset += 2) 
{

    vec2 chroma_value = {.0f, .0f};

    const uint pixel_idx = rgba_horiz_pixel_base_coord + rgba_horiz_pixel_offset;

#if (FB_GLSL_CHROMA_FILTER_NONE == 1)

    chroma_value = y_cb_cr_444[pixel_idx].yz;

#else

    for (int j = 0; j<chroma_filter_pixel_width; ++j)
    {
        uint idx = pixel_idx - chroma_filter_pixel_offset + j;
        chroma_value += y_cb_cr_444[idx].yz * chroma_filter_pixel_weights[j];
    }

#endif

    // Cb
    const int cb_word_idx = cb_idx / 3;
    const int cb_word_offset = cb_idx % 3;

    // Cr
    const int cr_word_idx = cr_idx / 3;
    const int cr_word_offset = cr_idx % 3;

    v210_words[cb_word_idx][cb_word_offset] = int(chroma_value[0]);
    cb_idx += 4;

    v210_words[cr_word_idx][cr_word_offset] = int(chroma_value[1]);
    cr_idx += 4;

}

// Write out the Y' in original resolution, and to the imageStore in
// between
int y_idx = 1;
for (uint rgba_horiz_pixel_offset = 0; 
     rgba_horiz_pixel_offset < 6; 
     ++rgba_horiz_pixel_offset) 
{

    const uint pixel_idx = rgba_horiz_pixel_base_coord + rgba_horiz_pixel_offset;

    int y_word_idx = y_idx / 3;
    int y_word_offset = y_idx % 3;

    v210_words[y_word_idx][y_word_offset] = y_cb_cr_444[pixel_idx][0];    
    y_idx += 2;

}

#ifndef FB_GLSL_MODE_330
for (int v210_x_group_offset = 0; v210_x_group_offset<4; ++v210_x_group_offset) {
#endif

#ifdef FB_GLSL_V210_EXTRACT_BITFIELDS_IN_SHADER

    // R is UI 32 bit, pack it there
    uvec4 val = uvec4(compress_v210_components_from_word(v210_words[v210_x_group_offset]), 0, 0, 0);

#else

    uvec4 val = uvec4(v210_words[v210_x_group_offset], 0);

#endif

#ifdef FB_GLSL_MODE_330        
    
#ifdef FB_GLSL_V210_EXTRACT_BITFIELDS_IN_SHADER
    out_v210_word = val.x;
#else
    out_v210_word = val;
#endif

#else
    imageStore(
        v210_output_image, 
        ivec2(v210_x_group_base_num*4 + v210_x_group_offset, v210_y_line_num), 
        val);

}
#endif
