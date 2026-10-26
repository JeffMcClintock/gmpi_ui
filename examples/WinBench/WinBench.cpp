// A throughput benchmark for gmpi_ui's Windows backends: Direct2D and the GPU
// renderer (Direct3D 12 through bgfx). Headless: each scene is drawn into an
// offscreen target of the chosen backend, frame after frame, with no window and
// no vsync, so the numbers are what the backend can sustain rather than what the
// display allows.
//
// The scenes, the loop and the output are examples/BenchScenes.h, shared with
// MacBench (and those of examples/WaylandBench), so a run here is like for like.
//
//     win_bench --backend d2d|gpu [--size WxH] [--frames N] [--only SCENE] [--static-colors]
//
// --static-colors  hold the bounce scene's colours still (see BenchScenes.h run()).
//
// build: see CMakeLists.txt beside this file.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "backends/DirectXGfx.h"
#if GMPI_UI_GPU_BACKEND
#include "backends/GpuGfx.h"
#include "helpers/CpuTextEngine.h"
#include "helpers/DecodeImage.h"
#include "helpers/FontProvider.h"
#endif
#include "../BenchScenes.h"

using namespace gmpi;
using namespace gmpi::drawing;

int main(int argc, char** argv)
{
    std::string backend = "d2d", only;
    bool staticColors = false;
    int width = 1000, height = 700, frames = 240;
    for (int i = 1; i < argc; ++i)
    {
        const bool more = i + 1 < argc;
        if (!strcmp(argv[i], "--backend") && more)      backend = argv[++i];
        else if (!strcmp(argv[i], "--frames") && more)  frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--only") && more)    only = argv[++i];
        else if (!strcmp(argv[i], "--static-colors"))   staticColors = true;
        else if (!strcmp(argv[i], "--size") && more)    sscanf(argv[++i], "%dx%d", &width, &height);
        else { fprintf(stderr, "usage: win_bench --backend d2d|gpu [--size WxH] [--frames N] [--only SCENE] [--static-colors]\n"); return 2; }
    }

    ::CoInitialize(nullptr);

    std::unique_ptr<gmpi::directx::Factory> d2dFactory;
#if GMPI_UI_GPU_BACKEND
    std::unique_ptr<gmpi::gpugfx::Factory> gpuFactory;
    std::unique_ptr<gmpi::drawing::CpuTextEngine> textEngine;
#endif
    gmpi::drawing::Factory factory; // after the backends: it releases its pointer when destroyed
    if (backend == "d2d")
    {
        d2dFactory = std::make_unique<gmpi::directx::Factory>();
        *AccessPtr::put(factory) = d2dFactory.get();
    }
    else if (backend == "gpu")
    {
#if GMPI_UI_GPU_BACKEND
        if (!gmpi::gpugfx::initialize())
        {
            fprintf(stderr, "the GPU backend will not start\n");
            return 1;
        }
        gpuFactory = std::make_unique<gmpi::gpugfx::Factory>();
        textEngine = std::make_unique<gmpi::drawing::CpuTextEngine>(gmpi::drawing::findFont);
        textEngine->imageDecoder = gmpi::drawing::decodeImageMemory;
        gpuFactory->cpu.imageDecoder = gmpi::drawing::decodeImageFile;
        gpuFactory->cpu.textEngine = textEngine.get();
        *AccessPtr::put(factory) = gpuFactory.get();
#else
        fprintf(stderr, "built without the GPU backend\n");
        return 1;
#endif
    }
    else
    {
        fprintf(stderr, "backend is d2d or gpu\n");
        return 2;
    }

    return gmpi_bench::run(factory, backend.c_str(), "Segoe UI", width, height, frames, only, staticColors);
}
