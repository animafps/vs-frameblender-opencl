@echo off
cd /d "%~dp0"
g++ .\vs-frameblender\main.cpp -shared -std=c++17 -I.\vs-frameblender\vapoursynth -o foo.dll