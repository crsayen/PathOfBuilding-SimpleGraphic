// macOS launcher for SimpleGraphic / Path of Building
// Loads libSimpleGraphic.dylib from the same directory and calls RunLuaFileAsWin.

#include <dlfcn.h>
#include <libgen.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*RunLuaFileAsWin_t)(int argc, char** argv);

int main(int argc, char** argv)
{
    // Resolve our own executable path via the macOS dyld API
    char exePath[PATH_MAX];
    uint32_t size = (uint32_t)sizeof(exePath);
    if (_NSGetExecutablePath(exePath, &size) != 0) {
        fprintf(stderr, "_NSGetExecutablePath: buffer too small\n");
        return 1;
    }
    // Resolve symlinks so dirname() gives the real directory
    char exeResolved[PATH_MAX];
    if (!realpath(exePath, exeResolved))
        strncpy(exeResolved, exePath, sizeof(exeResolved));

    char exeDirBuf[PATH_MAX];
    strncpy(exeDirBuf, exeResolved, sizeof(exeDirBuf));
    // dirname() may return a pointer to static storage that a later dirname() call will overwrite.
    // Copy the result immediately so subsequent dirname() calls don't corrupt it.
    char dir[PATH_MAX];
    strncpy(dir, dirname(exeDirBuf), sizeof(dir));

    // Make the runtime directory visible to dlopen BEFORE loading libSimpleGraphic.dylib,
    // so dyld finds libEGL.dylib (ANGLE) as a transitive dependency. GLFW's later
    // dlopen("libEGL.dylib") also benefits because the library will already be loaded.
    if (!getenv("DYLD_LIBRARY_PATH"))
        setenv("DYLD_LIBRARY_PATH", dir, 1);

    // Load libSimpleGraphic.dylib from the same directory as the launcher.
    // RTLD_GLOBAL is required so LuaJIT symbols are visible to dlopen'd Lua C modules.
    char libPath[PATH_MAX];
    snprintf(libPath, sizeof(libPath), "%s/libSimpleGraphic.dylib", dir);

    void* lib = dlopen(libPath, RTLD_LAZY | RTLD_GLOBAL);
    if (!lib) {
        fprintf(stderr, "Failed to load %s: %s\n", libPath, dlerror());
        return 1;
    }

    RunLuaFileAsWin_t runLua = (RunLuaFileAsWin_t)dlsym(lib, "RunLuaFileAsWin");
    if (!runLua) {
        fprintf(stderr, "dlsym RunLuaFileAsWin: %s\n", dlerror());
        return 1;
    }

    // Resolve the script path.  When invoked with no arguments (e.g. double-click from a
    // .app bundle) fall back to src/Launch.lua alongside the launcher.
    char scriptAbs[PATH_MAX];
    if (argc >= 2) {
        if (realpath(argv[1], scriptAbs) == NULL) {
            perror(argv[1]);
            return 1;
        }
        argv[1] = scriptAbs;
    } else {
        snprintf(scriptAbs, sizeof(scriptAbs), "%s/src/Launch.lua", dir);
    }

    // Derive the PoB root from the script:
    //   script = <pob_root>/src/Launch.lua  →  scriptDir = <pob_root>/src  →  pobRoot = <pob_root>
    char scriptDirBuf[PATH_MAX];
    strncpy(scriptDirBuf, scriptAbs, sizeof(scriptDirBuf));
    char* scriptDir = dirname(scriptDirBuf);

    char pobRoot[PATH_MAX];
    snprintf(pobRoot, sizeof(pobRoot), "%s/..", scriptDir);
    char pobRootAbs[PATH_MAX];
    if (!realpath(pobRoot, pobRootAbs))
        strncpy(pobRootAbs, pobRoot, sizeof(pobRootAbs));

    // Point the engine at the launcher's directory for runtime data (fonts, SimpleGraphic.cfg).
    if (!getenv("SG_BASE_PATH"))
        setenv("SG_BASE_PATH", dir, 1);

    // Ensure LUA_PATH includes PoB's bundled pure-Lua modules.
    // Also guards against pob-wide-crt.patch's strdup(NULL) crash when LUA_PATH is unset.
    if (!getenv("LUA_PATH")) {
        char luaPath[PATH_MAX * 4];
        snprintf(luaPath, sizeof(luaPath),
            "%s/runtime/lua/?.lua;%s/runtime/lua/?/init.lua;;",
            pobRootAbs, pobRootAbs);
        setenv("LUA_PATH", luaPath, 1);
    }

    // Ensure LUA_CPATH includes the launcher directory so Lua finds the C modules.
    // macOS Lua looks for both .so and .dylib suffixes.
    if (!getenv("LUA_CPATH")) {
        char luaCPath[PATH_MAX * 4];
        snprintf(luaCPath, sizeof(luaCPath), "%s/?.so;%s/?.dylib;;", dir, dir);
        setenv("LUA_CPATH", luaCPath, 1);
    }

    if (argc >= 2) {
        return runLua(argc - 1, argv + 1);
    } else {
        char* defaultArgv[] = { scriptAbs };
        return runLua(1, defaultArgv);
    }
}
