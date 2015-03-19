#define VTX_ATTR_POSITION	0
#define VTX_ATTR_TEXCOORD0	1
#define FRG_OUT_COLOR   0


const mat3x3 bt709_rgb_to_ycbcr_inv = mat3x3(
    0.0011415525114155253f,0.0011415525114155251f,0.0011415525114155255f,
    -2.8740650732344318e-20f,-0.00020906726889581339f,0.0020709821428571431f,
    0.0017575892857142855f,-0.00052246012603867058f,-1.6263032587282567e-19f);

const mat3x3 bt709_rgb_to_ycbcr = mat3x3(
    186.23760000000001f,-102.65660702737659f,448.0f,
    626.51519999999994f,-345.34339297262341f,-406.92100584201171f,
    63.247199999999999f,447.99999999999994f,-41.078994157988319f);

const ivec3 binary_offset = ivec3(64, 512, 512);
const uvec3 min_binary_value = uvec3(4);
const uvec3 max_binary_value = uvec3(1019);


// TODO: rename _component from name
vec3 srgb_to_linear_component(vec3 c) {

    vec3 linear_segment = c / 12.92f;
    vec3 linear_segment_coeff = step(c, vec3(0.04045f));

    vec3 power_segment = pow(max((c + 0.055f), vec3(0.0f)) / 1.055,vec3(2.4));
    vec3 power_segment_coeff = step(vec3(0.04045f), c);

    return linear_segment * linear_segment_coeff + power_segment * power_segment_coeff;

}

vec3 linear_to_srgb_component(vec3 c) {
    
    vec3 linear_segment = c * 12.92f;
    vec3 linear_segment_coeff = step(c, vec3(0.0031308f));

    vec3 power_segment = 1.055f * pow(max(c, vec3(0.0f)), vec3(0.41666f)) - 0.055f;
    vec3 power_segment_coeff = step(vec3(0.0031308f), c);

    return linear_segment * linear_segment_coeff + power_segment * power_segment_coeff;

}

const vec3 lp_filter_coeff = vec3(0.25f, 0.5f, 0.25f);

uint apply_3x3_lp_filter(vec3 input_value) 
{

    float filtered_value = roundEven(dot(input_value, lp_filter_coeff));
    uint filtered_value_clamped = clamp(uint(filtered_value), min_binary_value.x, max_binary_value.x);
    return filtered_value_clamped;
}

uint compress_v210_components_from_word(uvec3 v210_components) {

    uint word;

    word = bitfieldInsert(word, v210_components[0], 0, 10);
    word = bitfieldInsert(word, v210_components[1], 10, 10);
    word = bitfieldInsert(word, v210_components[2], 20, 10);

    return word;
}


uvec3 uncompress_v210_components_from_word(uint word) {

    uvec3 v210_components;

    // TODO: componentize this function? Could be done, but would need to rewrite
    // V210 component usage in later stages...

    v210_components[0] = bitfieldExtract(word, 0, 10);
    v210_components[1] = bitfieldExtract(word, 10, 10);
    v210_components[2] = bitfieldExtract(word, 20, 10);

    return v210_components;
}

vec3 convert_YCbCr_to_rgb(uvec3 y_cb_cr) {

    // See Poynton, Eq. 30.6 and 30.7
    precise vec3 rgb_norm = bt709_rgb_to_ycbcr_inv * vec3((ivec3(y_cb_cr) - binary_offset));

    // Clip the foot-room (we want to keep the head-room)
    //rgb_norm = max(rgb_norm, vec3(.0f, .0f, .0f));

    // Apply EOCF

    // TODO: optimize this, by doing a component-wise calculation, which doesn't
    // require an IF statement.

#ifdef FB_GLSL_LINEAR_RGB
    return srgb_to_linear_component(rgb_norm);
#else
    return rgb_norm;
#endif

}
