#!/bin/bash
clang++ ./vs-frameblender/main.cpp -shared -std=c++17 -I./vs-frameblender/vapoursynth -o foo.so