void kernel frame_blend_kernel(global int* stride, global int* width, global int* height, global int* dest, global int* numSrcs, global int* srcpp, global const float* weightPercents){        
    for (int h = 0; h < *height; ++h) {
        for (int w = 0; w < *width; ++w) {
            float acc = 0;

            for (int i = 0; i < *numSrcs; ++i) {
                int val = srcpp[w*(*width) + i]; //linearized frame
                acc += val * weightPercents[i];
            }

            dest[w] = acc;
        }

        for (int i = 0; i < *numSrcs; ++i) {
            srcpp[i] += *stride; // might not work
        }
        *dest += *stride;
    }
}