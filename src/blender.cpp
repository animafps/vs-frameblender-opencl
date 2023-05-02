#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <memory>
#include <vector>
#include <CL/opencl.hpp>
#include <iostream>
#include "../vapoursynth/VSHelper4.h"
#include "../vapoursynth/VapourSynth4.h"

typedef struct {
        VSNode *node;
        const VSVideoInfo *vi;
        std::vector<float> weightPercents;
        cl::Context context;
        cl::Program::Sources sources;
        cl::Program program;
        cl::Device device;
        bool process[3];
} FrameBlendData;


static void
frameBlend(const FrameBlendData *d, const VSFrame * const *srcs, VSFrame *dst, int plane, const VSAPI *vsapi) {
    int stride = vsapi->getStride(dst, plane) / sizeof(uint8_t);
    int width = vsapi->getFrameWidth(dst, plane);
    int height = vsapi->getFrameHeight(dst, plane);

    const uint8_t *srcpp[128];
    const size_t numSrcs = d->weightPercents.size();

    for (size_t i = 0; i < numSrcs; i++) {
        srcpp[i] = reinterpret_cast<const uint8_t *>(vsapi->getReadPtr(srcs[i], plane));
    }
    /// Need to linearize scrpp which is the array of frames 

    uint8_t *VS_RESTRICT dstp = reinterpret_cast<uint8_t *>(vsapi->getWritePtr(dst, plane));
    const float *weight = d->weightPercents.data();
    unsigned maxVal = (1U << d->vi->format.bitsPerSample) - 1;
    cl::CommandQueue queue(d->context, d->device);
    cl::Kernel frame_blend_kernel = cl::Kernel(d->program, "frame_blend_kernel");

    cl::Buffer strideBuf(d->context, CL_MEM_READ_ONLY | CL_MEM_HOST_NO_ACCESS | CL_MEM_COPY_HOST_PTR, sizeof(stride), &stride);
    cl::Buffer widthBuf(d->context, CL_MEM_READ_ONLY | CL_MEM_HOST_NO_ACCESS | CL_MEM_COPY_HOST_PTR, sizeof(width), &width);
    cl::Buffer heightBuf(d->context, CL_MEM_READ_ONLY | CL_MEM_HOST_NO_ACCESS | CL_MEM_COPY_HOST_PTR, sizeof(height), &height);
    cl::Buffer destBuf(d->context, CL_MEM_COPY_HOST_PTR, sizeof(uint8_t) * width * height, &dst);
    cl::Buffer srcppBuf(d->context, CL_MEM_COPY_HOST_PTR, sizeof(uint8_t) * 128 * width * height, &srcpp);
    cl::Buffer weightpercentsBuf(d->context, CL_MEM_COPY_HOST_PTR, sizeof(float) * numSrcs, &weight);
    frame_blend_kernel.setArg(0, strideBuf);
    frame_blend_kernel.setArg(1, widthBuf);
    frame_blend_kernel.setArg(2,heightBuf);
    frame_blend_kernel.setArg(3, destBuf);
    frame_blend_kernel.setArg(4,srcppBuf);
    frame_blend_kernel.setArg(5, weightpercentsBuf);
    cl::Event ev1;
    queue.enqueueNDRangeKernel(frame_blend_kernel, cl::NullRange, cl::NDRange(numSrcs*width*height), cl::NullRange, NULL, &ev1);
    ev1.wait();
    queue.enqueueReadBuffer(destBuf, true, 0,sizeof(uint8_t) * width * height, dst);
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
                    frameBlend(d, frames.data(), dst, plane, vsapi);
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

    //get all platforms (drivers)
    std::vector<cl::Platform> all_platforms;
    cl::Platform::get(&all_platforms);
    if(all_platforms.size()==0){
        std::cout<<" No platforms found. Check OpenCL installation!\n";
        exit(1);
    }
    cl::Platform default_platform=all_platforms[0];
    std::cout << "Using platform: "<<default_platform.getInfo<CL_PLATFORM_NAME>()<<"\n";
    
    std::vector<cl::Device> all_devices;
    default_platform.getDevices(CL_DEVICE_TYPE_GPU, &all_devices);
    if(all_devices.size()==0){
        std::cout<<" No devices found. Check OpenCL installation!\n";
        exit(1);
    }
    cl::Device default_device=all_devices[0];
    std::cout<< "Using device: "<<default_device.getInfo<CL_DEVICE_NAME>()<<"\n";
    cl::Context context({default_device});
    cl::Program::Sources sources;

    // kernel calculates for each element C=A+B
    std::string kernel_code=
        "   void kernel frame_blend_kernel(global int* stride, global int* width, global int* height, global const int* src, global int* dest, global int* numSrcs, global int* srcpp, global const float* weightPercents){"   
        "    for (int h = 0; h < *height; ++h) { "
        "        for (int w = 0; w < *width; ++w) {"
        "            float acc = 0;"
        "            for (int i = 0; i < *numSrcs; ++i) {"
        "                int val = srcpp[w*(*width) + i];"
        "                acc += val * weightPercents[i];"
        "            }"

        "            dest[w] = acc;"
        "        }"

        "        for (int i = 0; i < *numSrcs; ++i) {"
        "            srcpp[i] += stride;"
        "        }"
        "        *dest += *stride;"
        "    }"
        "}";
    sources.push_back({kernel_code.c_str(),kernel_code.length()});

    cl::Program program(context,sources);
    if(program.build({default_device})!=CL_SUCCESS){
        std::cout<<" Error building: "<<program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(default_device)<<"\n";
        exit(1);
    }

    data.context = context;
    data.sources = sources; 
    data.program = program;
    data.device = default_device;
    
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
        "com.github.animafps.vs-frameblender-opencl", "frameblenderopencl", "Frame blender", VS_MAKE_VERSION(1, 0), VAPOURSYNTH_API_VERSION,
        0, plugin
    );
    vspapi->registerFunction(
        "FrameBlend", "clip:vnode;weights:float[];planes:int[]:opt;", "clip:vnode;", frameBlendCreate, NULL, plugin
    );
}
