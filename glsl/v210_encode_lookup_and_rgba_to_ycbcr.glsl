// Inputs required:
//      rgba_input_image
//      rgba_input_image_pixel_coords
//      bt709_rgb_to_ycbcr
//      binary_offset
//      min_binary_value
//      max_binary_value
//      y_cb_cr_444
//      y_cb_cr_dst_index
    
vec4 rgb_linear = texelFetch(
    rgba_input_image, 
    rgba_input_image_pixel_coords, 
    0);

#ifdef FB_GLSL_LINEAR_RGB

precise vec3 rgb_perc = linear_to_srgb_component(rgb_linear.rgb);

#else

vec3 rgb_perc = rgb_linear.rgb;

#endif
        
precise vec3 y_cb_cr_sym = bt709_rgb_to_ycbcr * vec3(rgb_perc);
uvec3 y_cb_cr = uvec3(ivec3(binary_offset + ivec3(roundEven(y_cb_cr_sym))));

// Get rid of the forbidden values
y_cb_cr = clamp(y_cb_cr, min_binary_value, max_binary_value);

y_cb_cr_444[y_cb_cr_dst_index] = y_cb_cr;
