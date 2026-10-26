// A throughput benchmark for gmpi_ui's macOS backends: CoreGraphics and the GPU
// renderer (Metal through bgfx). Headless: each scene is drawn into an offscreen
// target of the chosen backend, frame after frame, with no window and no vsync,
// so the numbers are what the backend can sustain rather than what the display
// allows. The scenes, the loop and the output are examples/BenchScenes.h, shared
// with WinBench (and those of WaylandBench), so a run here is like for like.
//
//     mac_bench --backend cg|gpu [--size WxH] [--frames N] [--only SCENE] [--static-colors]
//
// --static-colors  hold the bounce scene's colours still (see BenchScenes.h run()).
//
// build: see CMakeLists.txt beside this file.

#import <Cocoa/Cocoa.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "backends/CocoaGfx.h"
#include "backends/GpuGfx.h"
#include "helpers/CpuTextEngine.h"
#include "helpers/DecodeImage.h"
#include "helpers/FontProvider.h"
#include "../BenchScenes.h"

int main(int argc, char** argv)
{
    std::string backend = "cg", only;
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
        else { fprintf(stderr, "usage: mac_bench --backend cg|gpu [--size WxH] [--frames N] [--only SCENE] [--static-colors]\n"); return 2; }
    }

    @autoreleasepool
    {
        std::unique_ptr<gmpi::cocoa::Factory> cgFactory;
        std::unique_ptr<gmpi::gpugfx::Factory> gpuFactory;
        std::unique_ptr<gmpi::drawing::CpuTextEngine> textEngine;
        gmpi::drawing::Factory factory; // after the backends: it releases its pointer when destroyed
        if (backend == "cg")
        {
            cgFactory = std::make_unique<gmpi::cocoa::Factory>();
            *gmpi::drawing::AccessPtr::put(factory) = cgFactory.get();
        }
        else if (backend == "gpu")
        {
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
            *gmpi::drawing::AccessPtr::put(factory) = gpuFactory.get();
        }
        else
        {
            fprintf(stderr, "backend is cg or gpu\n");
            return 2;
        }

        return gmpi_bench::run(factory, backend.c_str(), "Helvetica", width, height, frames, only, staticColors);
    }
}
