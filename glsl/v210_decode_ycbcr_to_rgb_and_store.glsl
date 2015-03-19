// Inputs required:
//      luma == uint[] array of luma values to be converted
//      chroma == uvec2[] array of chroma values
//      chroma_filter_v210_offset_left == as defined in constants
//      rgba_horiz_pixel_base_coord
//      rgba_output_image (only >= glsl_420)
//      rgba_pixel_base_coords (only >= glsl_420)
//      rgba_horiz_pixel_offset (only == glsl_330)

// First output the values where no interpolation is necessary
// (V210 is cosited)

#ifdef FB_GLSL_MODE_330
if ((rgba_horiz_pixel_offset % 2) == 0) {
#else
for (uint rgba_horiz_pixel_offset = 0; rgba_horiz_pixel_offset < 6; rgba_horiz_pixel_offset += 2) 
{
#endif

    const uint pixel_idx = rgba_horiz_pixel_base_coord + rgba_horiz_pixel_offset;

    uint luma_value = luma[pixel_idx];
    uvec2 chroma_value = chroma[pixel_idx/2];

    const uvec3 y_cb_cr = uvec3(luma_value, chroma_value.x, chroma_value.y);

    vec3 rgb_linear = convert_YCbCr_to_rgb(y_cb_cr);

#ifdef FB_GLSL_MODE_330

    out_color = vec4(rgb_linear, 1.0);

} else {

#else

    imageStore(
        rgba_output_image, 
        ivec2(rgba_pixel_base_coords + ivec2(rgba_horiz_pixel_offset, 0)), 
        vec4(rgb_linear, 1.0f));

    
}

// Now perform interpolation and write out
for (uint rgba_horiz_pixel_offset = 1; rgba_horiz_pixel_offset < 6; rgba_horiz_pixel_offset += 2)
{

#endif
   
    const uint pixel_idx = rgba_horiz_pixel_base_coord + rgba_horiz_pixel_offset;

    uint luma_value = luma[pixel_idx];

    // Need to reconstruct chroma values from neighbouring values.

    vec2 chroma_value = {.0f, .0f};

    for (int j = 0; j<chroma_filter_pixel_width; ++j) {

        // Note that the pixel rgba_horiz_pixel_offset has to be shifted by one, because
        // the chrom_value at int(pixel_idx) / 2) is already the one
        // left to the spatial position to be interpolated.
        int chroma_idx = (int(pixel_idx) / 2) - chroma_filter_pixel_offset + 1 + j;
        chroma_value += chroma[chroma_idx] * chroma_filter_pixel_weights[j];

    }

    const uvec3 y_cb_cr = uvec3(luma_value, int(chroma_value.x), int(chroma_value.y));

    vec3 rgb_linear = convert_YCbCr_to_rgb(y_cb_cr);

#ifdef FB_GLSL_MODE_330

    out_color = vec4(rgb_linear, 1.0);    

#else

    imageStore(
        rgba_output_image, 
        ivec2(rgba_pixel_base_coords + ivec2(rgba_horiz_pixel_offset, 0)), 
        vec4(rgb_linear, 1.0f));

#endif

}