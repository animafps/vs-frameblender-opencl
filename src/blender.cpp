#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <memory>
#include <vector>
#include "../vapoursynth/VSHelper4.h"
#include "../vapoursynth/VapourSynth4.h"


typedef struct {
        VSNode *node;
        const VSVideoInfo *vi;
        std::vector<float> weightPercents;
        bool process[3];
} FrameBlendData;


template<typename T>
static void
frameBlend(const FrameBlendData *d, const VSFrame * const *srcs, VSFrame *dst, int plane, const VSAPI *vsapi) {
    int stride = vsapi->getStride(dst, plane) / sizeof(T);
    int width = vsapi->getFrameWidth(dst, plane);
    int height = vsapi->getFrameHeight(dst, plane);

    const T *srcpp[128];
    const size_t numSrcs = d->weightPercents.size();

    for (size_t i = 0; i < numSrcs; i++) {
        srcpp[i] = reinterpret_cast<const T *>(vsapi->getReadPtr(srcs[i], plane));
    }

    T *VS_RESTRICT dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));

    unsigned maxVal = (1U << d->vi->format.bitsPerSample) - 1;

    for (int h = 0; h < height; ++h) {
        for (int w = 0; w < width; ++w) {
            float acc = 0;

            for (size_t i = 0; i < numSrcs; ++i) {
                T val = srcpp[i][w];
                acc += val * d->weightPercents[i];
            }

            int actualAcc = std::clamp(int(acc), 0, int(maxVal));
            dstp[w] = static_cast<T>(actualAcc);
        }

        for (size_t i = 0; i < numSrcs; ++i) {
            srcpp[i] += stride;
        }
        dstp += stride;
    }
}

static const VSFrame *VS_CC frameBlendGetFrame(
    int n, int activationReason, void *instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core,
    const VSAPI *vsapi
) {

    FrameBlendData *d = static_cast<FrameBlendData *>(instanceData);

    const int half = int(d->weightPercents.size() / 2);

    if (activationReason == arInitial) {
        bool clamp = (n > INT_MAX - 1 - half);
        int lastframe = clamp ? INT_MAX - 1 : n + half;

        // request all the frames we'll need
        for (int i = std::max(0, n - half); i <= lastframe; i++) {
            vsapi->requestFrameFilter(i, d->node, frameCtx);
        }
    } else if (activationReason == arAllFramesReady) {
        // get this frame's frames to be blended
        std::vector<const VSFrame *> frames(d->weightPercents.size());

        int fn = n - half;
        for (size_t i = 0; i < d->weightPercents.size(); i++) {
            frames[i] = vsapi->getFrameFilter(std::max(0, fn), d->node, frameCtx);
            if (fn < INT_MAX - 1) fn++;
        }

        const VSFrame *center = frames[frames.size() / 2];
        const VSVideoFormat *fi = vsapi->getVideoFrameFormat(center);

        const int pl[] = { 0, 1, 2 };
        const VSFrame *fr[] = {
            d->process[0] ? nullptr : center,
            d->process[1] ? nullptr : center,
            d->process[2] ? nullptr : center
        };

        VSFrame *dst;
        dst = vsapi->newVideoFrame2(
            fi, vsapi->getFrameWidth(center, 0), vsapi->getFrameHeight(center, 0), fr, pl, center, core
        );

        for (int plane = 0; plane < fi->numPlanes; plane++) {
            if (d->process[plane]) {
                if (fi->bytesPerSample == 1) {
                    frameBlend<uint8_t>(d, frames.data(), dst, plane, vsapi);
                } else if (fi->bytesPerSample == 2) {
                    frameBlend<uint16_t>(d, frames.data(), dst, plane, vsapi);
                } else {
                    return nullptr;
                }
            }
        }

        for (auto iter : frames)
            vsapi->freeFrame(iter);

        return dst;
    }

    return nullptr;
}

static void VS_CC frameBlendFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    FrameBlendData *d = static_cast<FrameBlendData *>(instanceData);
    vsapi->freeNode(d->node);
    free(d);
}

static void VS_CC frameBlendCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {

    FrameBlendData data;
    FrameBlendData *out_data;

    int err;

    // get clip and clip video info
    data.node = vsapi->mapGetNode(in, "clip", 0, &err);
    if (err) {
        vsapi->mapSetError(out, "FrameBlend: clip is not a valid VideoNode");
        vsapi->freeNode(data.node);
        return;
    }

    int numWeights = vsapi->mapNumElements(in, "weights");
    if ((numWeights % 2) != 1) {
        vsapi->mapSetError(out, "Number of weights must be odd");
        vsapi->freeNode(data.node);
        return;
    }

    data.vi = vsapi->getVideoInfo(data.node);

    // get weights
    float totalWeights = 0.f;
    for (int i = 0; i < numWeights; i++)
        totalWeights += vsapi->mapGetFloat(in, "weights", i, 0);

    // scale weights
    for (int i = 0; i < numWeights; i++) {
        data.weightPercents.push_back(vsapi->mapGetFloat(in, "weights", i, 0) / totalWeights);
    }

    int nPlanes = vsapi->mapNumElements(in, "planes");

    for (int i = 0; i < 3; i++)
        data.process[i] = (nPlanes <= 0); // default to all planes if no planes specified

    if (nPlanes <= 0) nPlanes = 3;

    for (int i = 0; i < nPlanes; i++) {

        int plane = vsapi->mapGetInt(in, "planes", i, &err);

        if (plane < 0 || plane >= 3) {
            vsapi->mapSetError(out, "FrameBlend: plane index out of range");
            vsapi->freeNode(data.node);
            return;
        }

        data.process[plane] = true;
    }

    out_data = new FrameBlendData { data };

    VSFilterDependency deps[] = {
        {data.node, rpGeneral}
    };

    vsapi->createVideoFilter(
        out, "FrameBlend", data.vi, frameBlendGetFrame, frameBlendFree, fmParallelRequests, deps, 1, out_data, core
    );
}

VS_EXTERNAL_API(void) VapourSynthPluginInit2(VSPlugin *plugin, const VSPLUGINAPI *vspapi) {
    vspapi->configPlugin(
        "com.vapoursynth.frameblender", "frameblender", "Frame blender", VS_MAKE_VERSION(1, 0), VAPOURSYNTH_API_VERSION,
        0, plugin
    );
    vspapi->registerFunction(
        "FrameBlend", "clip:vnode;weights:float[];planes:int[]:opt;", "clip:vnode;", frameBlendCreate, NULL, plugin
    );
}
