clang src/blender.cpp  --shared -I./vapoursynth -I/usr/include -fPIC  -lOpenCL -lstdc++ -o ./blender.so

if [ $? -eq 0 ]; then
    echo "Build success"
else
    echo "Build failed"
    exit 1
fi

sudo mv ./blender.so /usr/lib/vapoursynth/blenderopencl.so
