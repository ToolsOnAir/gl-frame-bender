#if (FB_GLSL_CHROMA_FILTER_NONE == 1)

// Replicating filter

const int chroma_filter_pixel_width = 2; // must be an even number!
const int chroma_filter_pixel_offset = chroma_filter_pixel_width / 2;
const float[chroma_filter_pixel_width] chroma_filter_pixel_weights = {1.0f, .0f};

#elif (FB_GLSL_CHROMA_FILTER_BASIC == 1)

// Simple linear interpolation

const int chroma_filter_pixel_width = 2; // must be an even number!
const int chroma_filter_pixel_offset = chroma_filter_pixel_width / 2;
const float[chroma_filter_pixel_width] chroma_filter_pixel_weights = {0.5f, 0.5f};

#elif (FB_GLSL_CHROMA_FILTER_HIGH == 1)

// 12-tap filter from Poynton

const int chroma_filter_pixel_width = 12; // must be an even number!
const int chroma_filter_pixel_offset = chroma_filter_pixel_width / 2;
const float[chroma_filter_pixel_width] chroma_filter_pixel_weights = {-2.0/256.0, 6.0/256.0, -12.0/256.0, 24.0/256.0, -48.0/256.0, 160.0/256.0, 160.0/256.0, -48.0/256.0, 24.0/256.0, -12.0/256.0, 6.0/256.0, -2.0/256.0};

#endif

// This is basically rounding up to a multiple of 6
// Note that we need to multiple the chroma_filter_pixel_offset by two, because 
// the chroma_filter for the chroma values always "skips" one pixel value, e.g.
// in order to access 3 chroma values, you'll need to pass 6 luma values
// Note that the offsets are NOT symmetrical, because in the first V210 we 
// already have one full CbCr pair.

const int chroma_filter_v210_offset_left = (chroma_filter_pixel_offset - 1 + 2) / 3;
const int chroma_filter_v210_offset_right = (chroma_filter_pixel_offset + 2) / 3;
const int chroma_filter_v210_width = chroma_filter_v210_offset_left + chroma_filter_v210_offset_right;

const ivec2 v210_image_size = ivec2(FB_INPUT_IMAGE_WIDTH, FB_INPUT_IMAGE_HEIGHT);
const ivec2 v210_input_image_bounds = ivec2(FB_INPUT_IMAGE_WIDTH - 1, FB_INPUT_IMAGE_HEIGHT - 1);
const ivec2 v210_input_image_group_bounds = ivec2(FB_INPUT_IMAGE_WIDTH - 4, FB_INPUT_IMAGE_HEIGHT - 1);
