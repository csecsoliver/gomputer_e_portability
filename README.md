# gomputer host portability library

`host.c` is compiled separately and archived by this package's `portability/nob.c`:

- Linux/macOS/MinGW: `build/native/libgomputer_host.a`
- MSVC: `build/native/gomputer_host.lib`
- Emscripten: `build/web/libgomputer_host.a`

Its only dependency is SDL3. It knows nothing about Lua, `Machine`, the palette,
or the memory/event layout. `shell.html` contains the browser/mobile UI and calls
the library's exported web input bridge. Native builds have no JS dependency.
One workstation window is supported per process.

## Build entry point

Upstream's root `nob.c` has **no dependency on this directory**, including this
package's build header. It compiles upstream's native `host_stub.c` against
installed SDL3 and Lua 5.5. The root `host.h` defines upstream's interface, not an
external-library header. Removing `portability/` leaves that build intact.

This optional build needs a C11 compiler and CMake 3.24+; the first build downloads
checksum-pinned SDL 3.2.28 and Lua 5.5.0. No installed SDL/Lua SDK is required.
CMake only builds SDL; nob compiles the application and Lua directly. Linux still
needs SDL's platform development libraries (e.g. `libx11-dev libxext-dev` for X11).

The optional package has its own nob executable. From the repository root:

```sh
mkdir -p build
cc -o build/nob-portable portability/nob.c
./build/nob-portable       # build and run native with the external host archive
./build/nob-portable build # native app + external host archive
./build/nob-portable web   # browser app + external host archive
```

On Windows use `mkdir build`, `cl /Febuild/nob-portable.exe /Fobuild/ portability/nob.c`,
then `build\nob-portable.exe build`, with CMake installed. MinGW: bootstrap with
`gcc -o build/nob-portable.exe portability/nob.c` and select the MinGW CMake generator
(`set CMAKE_GENERATOR=MinGW Makefiles`). Native output is `build/native/main`
(or `main.exe`). This optional builder places SDL beside the app with appropriate
runtime search paths; keep `main`, `lib/`, and `lua/` together on Linux/macOS, or
`main.exe`, `SDL3.dll`, and `lua/` on Windows. Desktop files are loaded from the
`lua/` directory beside the executable, independently of cwd.

`portability/nob.c` parses the target and calls `gomputer_build(web, run, stub)`.
Its private `build.h` owns dependency setup, compiler selection, archiving/linking,
and asset copying. It is neither a public runtime API nor an upstream include.
This optional executable watches the header and rebuilds itself when it changes.
Both builders write `build/native/main`; choose one implementation per build.

## Native upstream fallback (no external host library)

Carry just `host.h` and `host_stub.c` in upstream, and compile `host_stub.c` as
an application source **instead of** linking `libgomputer_host`. No changes to
the four call sites are needed. Do not compile/link both implementations.

The stub is a working native SDL implementation, not no-op placeholders: it
creates the window, waits/coalesces events, sleeps, and resolves `lua/` beside
the executable. It has no dependency on `host.c`, the archive, or the web shell.
SDL3 and Lua are still required by the application.

For this standalone build, install a C11 compiler, SDL3, and **Lua 5.5** development
headers/libraries. Make `<SDL3/SDL.h>` and `<lua.h>` available to the compiler,
and SDL3 and Lua available as `-lSDL3` and `-llua` (`SDL3.lib`/`lua.lib` on MSVC).
For SDKs outside the default search paths use `C_INCLUDE_PATH`/`LIBRARY_PATH`
with GCC/Clang, or `INCLUDE`/`LIB` with MSVC. Lua 5.4 is not sufficient.

In this checkout:

```sh
cc -o nob nob.c
./nob        # build and run using installed SDL3/Lua 5.5
./nob build  # build only; no portability files needed
# Or use this optional package just to bootstrap SDL/Lua:
./build/nob-portable build stub
```

The output is `build/native/main` (or `main.exe`). `./build/nob-portable build`
switches back to the full library. The stub intentionally does not support browser/
mobile; `./build/nob-portable web stub` is rejected rather than producing a frozen page.

Windows: use a Visual Studio 2022 Developer Command Prompt with those SDKs:

```bat
cl nob.c
nob.exe build
build\native\main.exe
```

Keep `lua/` beside the executable. Shared SDL/Lua libraries must be installed
where the OS can load them; on Windows place their DLLs beside `main.exe` or on
PATH. For Zig bootstrap `nob.zig.c` (`zig cc -o nobzig nob.zig.c`); it uses the
same installed dependencies. Windows/macOS paths are not yet tested on those hosts.

## Browser (including mobile)

Install/activate the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)
so `emcc` and `emcmake` are on PATH, then use the optional builder above:

```sh
./build/nob-portable web
python3 -m http.server 8080 --directory build/web
```

Open `http://localhost:8080` (or your computer's LAN address on a phone). Serve
`index.html`, `index.js`, `index.wasm`, and `index.data` together; opening the
HTML directly via `file://` does not work. Static HTTP hosting is sufficient—no
backend, threads, or cross-origin isolation headers are required. Source/license
files for bundled SDL and Lua are cached under `build/deps/`; include their
licenses when redistributing.

The Lua API and scripts are unchanged. Asyncify lets `Render()` and `Sleep()`
yield to the browser while retaining the existing blocking Lua loop. Long-running
Lua code must still call one of them to keep the page responsive. Browser
`ReadFile()` reads bundled files, not arbitrary host files; there is no persistent
storage or file picker yet. Ctrl+R reloads the bundled `main.lua` in memory;
after editing source files, rerun `./build/nob-portable web` and refresh the page.

On touch devices the display is a **relative touchpad**: slide to move the cursor,
tap to left-click, or hold for 400 ms then slide to drag. A cancelled gesture
releases the mouse button. Use **Keyboard** to open the system keyboard. The
scrollable bottom bar supplies sticky Ctrl/Shift/Alt, Esc, Tab, arrows, Backspace,
Enter, and Reload Lua. Tap modifiers again to release them. Text arrives through
the existing text-input event, rather than fabricated letter key presses (except
modifier shortcuts). Lua should enable text with `ToggleTextInput(true)`, just as
on desktop. Landscape orientation gives the 480×270 display more readable space.

The bar follows the visual viewport to sit above an overlaying software keyboard.
This is a web UI, not native Android/iOS packaging or a native keyboard accessory.
Real Android/iOS keyboard placement and IME behavior still need device testing;
headless browser emulation cannot validate the actual system keyboard.

## Upstream integration

Include `host.h` and link the target-specific archive **before** SDL3. Keep
`SDL3/SDL_main.h` in the translation unit containing `main(int argc, char **argv)`
for SDL's portable entry point. The four API substitutions are:

```c
// Instead of SDL_Init + SDL_CreateWindow. Check for NULL.
window = host_create_window("gomputer_e", 480, 270, scale);

// Instead of SDL_WaitEventTimeout. Also removes the upstream mouse-draining loop.
bool received = host_wait_event(&event, remaining_ms);

// Instead of SDL_Delay. No Lua coroutine or main-loop rewrite is needed.
host_sleep(milliseconds);

// Instead of constructing a cwd-relative lua/ path. Use for reads and _init.lua.
char *path = host_asset_path(filename);
// ... readFile(path, ...) or luaL_loadfile(L, path) ...
SDL_free(path);
```

Rendering, Lua bindings, key codes, and event serialization remain upstream.
Destroy the returned window and call `SDL_Quit()` as usual. `host_wait_event`
coalesces only consecutive mouse-motion events, never discarding a move before
a following key/button event. Returned text is valid until its next call.

Browser `host_wait_event`/`host_sleep` yield through Asyncify. The mobile bridge
queues standard SDL mouse/key events and converts its owned text strings into
`SDL_EVENT_TEXT_INPUT` before returning them to upstream. Text respects
`SDL_StartTextInput`/`SDL_StopTextInput` (the existing Lua toggle binding).

## Linking from another build

Native: compile `host.c` with the same SDL headers/toolchain as the application,
adding the upstream directory to the include path for `host.h`. Archive it with
`ar` or MSVC `lib`, and link that archive plus SDL3 instead of compiling the stub.
Supply `lua/` beside the executable. The optional nob build handles shared-library
placement and relative runtime search paths.

Browser: compile/archive with `emcc`/`emar`. At the **final application link** use
`-sASYNCIFY -sASYNCIFY_STACK_SIZE=131072 -sALLOW_MEMORY_GROWTH=1 -sEXIT_RUNTIME=1`,
`-sEXPORTED_RUNTIME_METHODS=['ccall']`, `--preload-file lua@/lua`, and
`--shell-file portability/shell.html`. The library does not remove the need for
these final-link settings: Asyncify instruments the whole application, including
Lua's call stack. Serve the generated HTML/JS/Wasm/data files together.

Beyond these calls, upstream still needs ordinary C portability corrections:
remove unused POSIX-only headers, use portable integer format specifiers and
`SDL_strdup`/`SDL_free`, and terminate a label with `;` before a declaration in
C11. No Emscripten conditionals or mobile logic are needed upstream.
