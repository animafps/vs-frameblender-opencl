void kernel frame_blend_kernel(global const float *weights, global const void *srcs_, unsigned num_srcs, global void *dst_, unsigned depth, unsigned w, unsigned h, long stride){        
    ptrdiff_t offset = 0;
	int maxval = (1L << depth) - 1;
    unsigned** srcs = srcs_;

	for (unsigned i = 0; i < h; ++i) {
		int *dst = (dst_ + offset);

		for (unsigned j = 0; j < w; ++j) {
			int accum = 0;

			for (unsigned k = 0; k < num_srcs; ++k) {
				const unsigned *src = srcs[k] + offset;
				int val = src[j];
				accum += val * weights[k];
			}

			accum = min(max(accum, 0), maxval);
			dst[j] = accum;
		}

		offset += stride;
	}
}