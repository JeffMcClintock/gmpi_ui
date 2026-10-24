# Applied to the fetched bgfx source (FetchContent PATCH_COMMAND). Idempotent.
#
# Vulkan present mode without vsync: bgfx's table treats MAILBOX as a vsync
# mode, so "no vsync" accepts only IMMEDIATE, which Mesa's Wayland WSI does
# not offer, and the swap chain falls back to FIFO. Mesa's FIFO throttles on
# frame callbacks: the commit of frame N is performed inside the NEXT WSI
# call, so a window that presents once after being idle shows that frame one
# present late. MAILBOX commits immediately, so it is the no-vsync mode here.
set(file "${BGFX_SRC}/bgfx/src/renderer_vk.cpp")
file(READ "${file}" contents)
string(REPLACE
  "{ VK_PRESENT_MODE_MAILBOX_KHR,      true,  \"VK_PRESENT_MODE_MAILBOX_KHR\"      },"
  "{ VK_PRESENT_MODE_MAILBOX_KHR,      false, \"VK_PRESENT_MODE_MAILBOX_KHR\"      }, // gmpi: a no-vsync mode (see cmake/patch_bgfx.cmake)"
  patched "${contents}")
if (NOT patched STREQUAL contents)
  file(WRITE "${file}" "${patched}")
  message(STATUS "gmpi: patched bgfx present-mode table")
endif ()

# No OpenGL on Linux: gmpi_gpu.cmake compiles bgfx's GL renderer out there,
# but bgfx.cmake still demanded OpenGL's development files and linked its
# libraries (which nothing called; bgfx dlopens libGL itself when GL is used).
set(file "${BGFX_SRC}/cmake/bgfx/bgfx.cmake")
file(READ "${file}" contents)
string(REPLACE "\tfind_package(OpenGL REQUIRED)\n" "" patched "${contents}")
string(REPLACE "\${X11_LIBRARIES} \${OPENGL_LIBRARIES}" "\${X11_LIBRARIES}" patched "${patched}")
if (NOT patched STREQUAL contents)
  file(WRITE "${file}" "${patched}")
  message(STATUS "gmpi: patched bgfx.cmake (no OpenGL on Linux)")
endif ()
