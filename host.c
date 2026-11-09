#include "host.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

static SDL_Window *host_window;
static int host_width, host_height, host_scale;
static int pointer_x, pointer_y;
static Uint32 text_event;
static char *last_text;

// These entry points are called by shell.html, not by the Lua runtime.
EMSCRIPTEN_KEEPALIVE void web_pointer(int dx, int dy, int button, int down) {
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_EVENT_MOUSE_MOTION;
    pointer_x = SDL_clamp(pointer_x + dx, 0, host_width - 1);
    pointer_y = SDL_clamp(pointer_y + dy, 0, host_height - 1);
    event.motion.which = SDL_TOUCH_MOUSEID;
    event.motion.x = pointer_x * host_scale;
    event.motion.y = pointer_y * host_scale;
    SDL_PushEvent(&event);
    if (button) {
        SDL_zero(event);
        event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        event.button.button = button;
        event.button.clicks = 1;
        SDL_PushEvent(&event);
    }
}

EMSCRIPTEN_KEEPALIVE void web_key(int key, int mod, int down) {
    SDL_Event event;
    SDL_zero(event);
    event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.key = key;
    event.key.mod = mod;
    event.key.down = down;
    SDL_PushEvent(&event);
}

EMSCRIPTEN_KEEPALIVE void web_text(const char *text) {
    if (!SDL_TextInputActive(host_window)) return;
    SDL_Event event;
    SDL_zero(event);
    // JS's string expires after this call, so own a copy until event consumption.
    event.type = text_event;
    event.user.data1 = SDL_strdup(text);
    if (!SDL_PushEvent(&event)) SDL_free(event.user.data1);
}
#endif

SDL_Window *host_init_window(const char *title, int width, int height, int scale) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        SDL_Log("SDL init: %s", SDL_GetError());
        return NULL;
    }
    SDL_Window *window = SDL_CreateWindow(title, width * scale, height * scale, 0);
    if (!window) {
        SDL_Log("SDL window: %s", SDL_GetError());
        SDL_Quit();
        return NULL;
    }
#ifdef __EMSCRIPTEN__
    host_window = window;
    host_width = width;
    host_height = height;
    host_scale = scale;
    pointer_x = pointer_y = 0;
    text_event = SDL_RegisterEvents(1);
#endif
    return window;
}

bool host_wait_event(SDL_Event *event, int timeout_ms) {
#ifdef __EMSCRIPTEN__
    SDL_free(last_text);
    last_text = NULL;
    Uint64 start = SDL_GetTicks();
    // Present even when events are already queued, preserving the blocking Lua loop.
    emscripten_sleep(16);
    bool success = SDL_PollEvent(event);
    while (!success && SDL_GetTicks() - start < (Uint64)timeout_ms) {
        emscripten_sleep(8);
        success = SDL_PollEvent(event);
    }
    if (success && event->type == text_event) {
        last_text = event->user.data1;
        SDL_zero(*event);
        event->type = SDL_EVENT_TEXT_INPUT;
        event->text.text = last_text;
    }
#else
    bool success = SDL_WaitEventTimeout(event, timeout_ms);
#endif
    if (success && event->type == SDL_EVENT_MOUSE_MOTION) {
        SDL_Event next;
        while (SDL_PeepEvents(&next, 1, SDL_PEEKEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST) > 0 &&
               next.type == SDL_EVENT_MOUSE_MOTION) {
            SDL_PollEvent(event);
        }
#ifdef __EMSCRIPTEN__
        if (event->motion.which != SDL_TOUCH_MOUSEID) {
            pointer_x = event->motion.x / host_scale;
            pointer_y = event->motion.y / host_scale;
        }
#endif
    }
    return success;
}

void host_sleep(uint32_t ms) {
#ifdef __EMSCRIPTEN__
    emscripten_sleep(ms);
#else
    SDL_Delay(ms);
#endif
}

char *host_asset_path(const char *filename) {
    char *path = NULL;
#ifdef __EMSCRIPTEN__
    SDL_asprintf(&path, "/lua/%s", filename);
#else
    SDL_asprintf(&path, "%slua/%s", SDL_GetBasePath(), filename);
#endif
    return path;
}

char *host_db_path(void) {
    char *path = NULL;
#ifdef __EMSCRIPTEN__
    SDL_asprintf(&path, "/gomputer_vfs.db");
#else
    SDL_asprintf(&path, "%sgomputer_vfs.db", SDL_GetBasePath());
#endif
    return path;
}
