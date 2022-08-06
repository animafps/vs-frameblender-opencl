@echo off
clang++ .\src\blender.cpp -O3 -march=native -shared -std=c++17 -I.\vapoursynth -o blender.dll
