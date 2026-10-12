#ifndef GOMPUTER_BUILD_H
#define GOMPUTER_BUILD_H

// Include after nob.h. All dependency/toolchain details stay out of the entry point.
// CMake is only used to fetch pinned sources and build/install SDL, not the app.
static const char dependencies[] =
    "cmake_minimum_required(VERSION 3.24)\n"
    "project(gomputer_dependencies C)\n"
    "include(FetchContent)\n"
    "set(SDL_TESTS OFF CACHE BOOL \"\" FORCE)\n"
    "FetchContent_Declare(SDL3\n"
    " URL https://github.com/libsdl-org/SDL/releases/download/release-3.2.28/SDL3-3.2.28.tar.gz\n"
    " URL_HASH SHA256=1330671214d146f8aeb1ed399fc3e081873cdb38b5189d1f8bb6ab15bbc04211\n"
    " SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/SDL)\n"
    "FetchContent_MakeAvailable(SDL3)\n"
    "FetchContent_Declare(lua\n"
    " URL https://www.lua.org/ftp/lua-5.5.0.tar.gz\n"
    " URL_HASH SHA256=57ccc32bbbd005cab75bcc52444052535af691789dba2b9016d5c50640d68b3d\n"
    " SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lua)\n"
    "FetchContent_MakeAvailable(lua)\n";

static int gomputer_build(bool web, bool run, bool stub)
{
    const char *output = web ? "build/web" : "build/native";
    if (!nob_mkdir_if_not_exists("build") || !nob_mkdir_if_not_exists("build/deps") ||
        !nob_mkdir_if_not_exists(output)) return 1;
    if (!nob_write_entire_file("build/deps/CMakeLists.txt", dependencies, strlen(dependencies))) return 1;

    Nob_Cmd cmd = {0};
    if (web) nob_cmd_append(&cmd, "/usr/lib/emscripten/emcmake");
    nob_cmd_append(&cmd, "cmake", "-S", "build/deps", "-B",
        web ? "build/deps/web" : "build/deps/native", "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_INSTALL_LIBDIR=lib", "-DSDL_INSTALL=ON",
        web ? "-DSDL_SHARED=OFF" : "-DSDL_SHARED=ON",
        web ? "-DSDL_STATIC=ON" : "-DSDL_STATIC=OFF",
        nob_temp_sprintf("-DCMAKE_INSTALL_PREFIX=%s/%s", nob_get_current_dir_temp(), output));
    if (!nob_cmd_run(&cmd)) return 1;
    nob_cmd_append(&cmd, "cmake", "--build", web ? "build/deps/web" : "build/deps/native",
        "--config", "Release", "--parallel");
    if (!nob_cmd_run(&cmd)) return 1;
    nob_cmd_append(&cmd, "cmake", "--install", web ? "build/deps/web" : "build/deps/native",
        "--config", "Release");
    if (!nob_cmd_run(&cmd)) return 1;

    const char *exe = web ? "build/web/index.html" :
#ifdef _WIN32
        "build/native/main.exe";
#else
        "build/native/main";
#endif
    if (web) nob_cmd_append(&cmd, "/usr/lib/emscripten/emcc", "-O3", "-std=c23");
    else {
#ifdef GOMPUTER_ZIG
        nob_cmd_append(&cmd, "zig", "cc", "-O3", "-std=c23");
#else
#if defined(_WIN32) && defined(__GNUC__)
        nob_cmd_append(&cmd, "gcc");
#else
        nob_cc(&cmd);
#endif
#ifdef _MSC_VER
        nob_cmd_append(&cmd, "/O2", "/std:c23", "/D_CRT_SECURE_NO_WARNINGS");
#else
        nob_cmd_append(&cmd, "-O3", "-std=c23");
#endif
#endif
    }

    const char *host_library = nob_temp_sprintf("%s/libgomputer_host.a", output);
    if (!stub) {
        // A real archive: the runtime links it without including its implementation.
        const char *host_object = nob_temp_sprintf("%s/host.o", output);
        Nob_Cmd host_cmd = {0};
        nob_da_append_many(&host_cmd, cmd.items, cmd.count);
#if defined(_MSC_VER) && !defined(GOMPUTER_ZIG)
        if (!web) {
            host_object = "build/native/host.obj";
            host_library = "build/native/gomputer_host.lib";
            nob_cmd_append(&host_cmd, "/I.", "/Ibuild/native/include", "/c", "portability/host.c",
                nob_temp_sprintf("/Fo:%s", host_object));
        } else
#endif
        nob_cmd_append(&host_cmd, "-I.", nob_temp_sprintf("-I%s/include", output), "-c", "portability/host.c",
            "-o", host_object);
        if (!nob_cmd_run(&host_cmd)) return 1;
#if defined(_MSC_VER) && !defined(GOMPUTER_ZIG)
        if (!web) nob_cmd_append(&host_cmd, "lib.exe", "/nologo", nob_temp_sprintf("/OUT:%s", host_library), host_object);
        else
#endif
        {
#ifdef GOMPUTER_ZIG
            if (!web) nob_cmd_append(&host_cmd, "zig", "ar");
            else
#endif
            nob_cmd_append(&host_cmd, web ? "/usr/lib/emscripten/emar" : "ar");
            nob_cmd_append(&host_cmd, "rcs", host_library, host_object);
        }
        if (!nob_cmd_run(&host_cmd)) return 1;
    }

#if defined(_MSC_VER) && !defined(GOMPUTER_ZIG)
    if (!web) nob_cmd_append(&cmd, "/Ibuild/deps/lua/src", "/Ibuild/native/include",
        "/Fobuild/native/", nob_temp_sprintf("/Fe:%s", exe));
    else
#endif
    nob_cmd_append(&cmd, "-Ibuild/deps/lua/src", nob_temp_sprintf("-I%s/include", output), "-o", exe);
    nob_cmd_append(&cmd, "main.c");
    Nob_File_Paths sources = {0};
    if (!nob_read_entire_dir("build/deps/lua/src", &sources)) return 1;
    for (size_t i = 0; i < sources.count; ++i) {
        const char *name = sources.items[i];
        if (nob_sv_end_with(nob_sv_from_cstr(name), ".c") &&
            strcmp(name, "lua.c") != 0 && strcmp(name, "luac.c") != 0)
            nob_cmd_append(&cmd, nob_temp_sprintf("build/deps/lua/src/%s", name));
    }
    nob_cmd_append(&cmd, stub ? "host_stub.c" : host_library);
    if (web) {
        nob_cmd_append(&cmd, "build/web/lib/libSDL3.a", "-lm", "-sASYNCIFY",
            "-sASYNCIFY_STACK_SIZE=131072", "-sALLOW_MEMORY_GROWTH=1", "-sEXIT_RUNTIME=1",
            "-sEXPORTED_RUNTIME_METHODS=['ccall']", "--preload-file", "lua@/lua",
            "--shell-file", "portability/shell.html", "-sMINIFY_HTML=0");
    } else {
#if defined(_MSC_VER) && !defined(GOMPUTER_ZIG)
        nob_cmd_append(&cmd, "build/native/lib/SDL3.lib");
#else
        nob_cmd_append(&cmd, "-Lbuild/native/lib", "-lSDL3");
#ifndef _WIN32
        nob_cmd_append(&cmd, "-lm",
#ifdef __APPLE__
            "-Wl,-rpath,@loader_path/lib");
#else
            "-Wl,-rpath,$ORIGIN/lib");
#endif
#endif
#endif
    }
    if (!nob_cmd_run(&cmd)) return 1;
    if (!web) {
        if (!nob_copy_directory_recursively("lua", "build/native/lua")) return 1;
#ifdef _WIN32
        if (!nob_copy_file("build/native/bin/SDL3.dll", "build/native/SDL3.dll")) return 1;
#endif
    }
    if (run) {
        nob_cmd_append(&cmd, exe);
        if (!nob_cmd_run(&cmd)) return 1;
    }
    nob_log(NOB_INFO, "Built %s", exe);
    return 0;
}

#endif
