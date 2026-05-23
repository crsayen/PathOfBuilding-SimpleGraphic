set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES arm64)

# ANGLE must be dynamic so GLFW can dlopen("libEGL.dylib") at runtime.
# With static linkage, ANGLE symbols are hidden inside libSimpleGraphic.dylib
# and GLFW's EGL loader fails with "EGL: Library not found".
if(PORT STREQUAL "angle")
    set(VCPKG_LIBRARY_LINKAGE dynamic)
endif()
