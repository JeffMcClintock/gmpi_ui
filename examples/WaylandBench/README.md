# wayland_bench

A frame-rate benchmark for gmpi_ui's Wayland backends: the CPU (software)
renderer and the GPU renderer in `backends/gpu`. The default scene bounces
filled paths round the window, as JUCE's GraphicsDemo does, so a dropped frame
is visible as well as measured. Raise the load until the frame rate falls,
then run the same scene on the other backend.

## Build

Needs a Wayland session, `libwayland-dev wayland-protocols libxkbcommon-dev
libdecor-0-dev libdbus-1-dev libfontconfig1-dev libharfbuzz-dev libfreetype-dev
libpng-dev`. The GPU backend renders through Vulkan and needs no OpenGL
development files: `gmpi_gpu.cmake` compiles bgfx's GL renderer out on Linux.
The GMPI SDK is expected beside gmpi_ui (`../GMPI`);
override with `-DGMPI_SDK=...`.

```sh
cmake -S examples/WaylandBench -B build-bench -G Ninja
cmake --build build-bench
```

`-DWAYLAND_BENCH_GPU=OFF` builds the CPU backend only, without fetching bgfx.

## Run

```sh
build-bench/wayland_bench --gpu
build-bench/wayland_bench --cpu
```

| Option | |
|---|---|
| `--cpu`, `--gpu` | The backend. Chosen at launch: a frame picks its renderer once. Default GPU. `GMPI_UI_GPU=0/1` in the environment overrides both. |
| `--scene N` | 1 bounce, 2 lines, 3 rects, 4 ellipses, 5 text, 6 gradients |
| `--load N` | How many things the scene draws (default 16) |
| `--detail N` | Curves per bouncing path (default 8) |
| `--fill solid\|linear\|radial`, `--no-outline` | The bounce scene's fill and outline at launch |
| `--static-colors` | Hold the bounce colours still. By default they drift, so every gradient is new each frame (atlas churn); this measures reused gradients instead. |
| `--size WxH` | Window size (default 1000x700) |
| `--report` | Print one line of stats a second on stdout |
| `--frames N` | Quit after N presented frames |
| `--paused` | Start with the animation frozen: every frame draws the same picture |
| `--dump FILE` | Redraw frame N (`--frames N`, default 3) into an offscreen target of the same backend and write it: int32 width, height, pixel format, then the rows. With `--paused`, two runs compare pixel for pixel. |

Keys: `1`-`6` scene, `+`/`-` double/halve the load, `]`/`[` double/halve path
detail, `f` cycle the bounce fill (solid, linear, radial gradient), `o` outline,
space freezes the animation (frames keep coming, so a static scene is
measured), `q` quit.

## Reading the numbers

- **fps / frame** - time between presents. The compositor's frame callbacks
  pace presents, so this tops out at the display refresh rate; below it, it is
  the backend's real throughput.
- **render** - the client's drawing calls. On the GPU path these only record
  work.
- **present** - what follows render: the encode into the shm buffer on the CPU
  path, the submit and swap-chain present on the GPU path
  (`WaylandFrameBase::lastPresentTiming()`).

The overlay names the backend that actually presented, so a GPU run that fell
back to the CPU shows as CPU.
