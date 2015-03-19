#if (FB_GLSL_CHROMA_FILTER_NONE == 1)

const int chroma_filter_pixel_width = 1; // must be an odd number!
const int chroma_filter_pixel_offset = chroma_filter_pixel_width / 2;
const float[chroma_filter_pixel_width] chroma_filter_pixel_weights = {1.0f};

#elif (FB_GLSL_CHROMA_FILTER_BASIC == 1)

const int chroma_filter_pixel_width = 3; // must be an odd number!
const int chroma_filter_pixel_offset = chroma_filter_pixel_width / 2;
const float[chroma_filter_pixel_width] chroma_filter_pixel_weights = {0.25f, 0.5f, 0.25f};

#elif (FB_GLSL_CHROMA_FILTER_HIGH == 1)

const int chroma_filter_pixel_width = 13; // must be an odd number!
const int chroma_filter_pixel_offset = chroma_filter_pixel_width / 2;
const float[chroma_filter_pixel_width] chroma_filter_pixel_weights = {-1.0f/256.0f, 3.0f/256.0f, -6.0f/256.0f, 12.0f/256.0f, -24.0f/256.0f, 80.0f/256.0f, 128.0f/256.0f, 80.0f/256.0f, -24.0f/256.0f, 12.0f/256.0f, -6.0f/256.0f, 3.0f/256.0f, -1.0f/256.0f};

#endif

// Because 422 drops every other sample, we can shorten the offset on the right-side 
// a bit (because the last CbCr is dropped anyway...)
#if (FB_GLSL_CHROMA_FILTER_NONE == 1)

const int chroma_filter_pixel_offset_left = 0;
const int chroma_filter_pixel_offset_right = 0;

#else

const int chroma_filter_pixel_offset_left = chroma_filter_pixel_offset;
const int chroma_filter_pixel_offset_right = chroma_filter_pixel_offset-1;

#endif

const ivec2 rgba_input_image_size = ivec2(FB_INPUT_IMAGE_WIDTH, FB_INPUT_IMAGE_HEIGHT);
const ivec2 rgba_input_image_bounds = ivec2(FB_INPUT_IMAGE_WIDTH - 1, FB_INPUT_IMAGE_HEIGHT - 1);

