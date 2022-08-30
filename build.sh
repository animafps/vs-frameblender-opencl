clang++ ./src/blender.cpp -O3 -march=native -shared -std=c++17 -I./vapoursynth -o ./blender.so

if [ $? -eq 0 ]; then
    echo "Build success"
else
    echo "Build failed"
    exit 1
fi

sudo mv ./blender.so /usr/lib/vapoursynth/blender.so
