/*
 * config_pc.c - user settings: config.ini (keys, gamepad, hotkeys, options).
 *
 * Looked up in this order: $PHG_CONFIG, ./config.ini, then next to the
 * executable. If none exists, a commented default file is written next to the
 * executable so that players can discover and edit it.
 *
 * Format (case-insensitive names, several bindings separated by commas):
 *   [keyboard]   A = X          key names as SDL writes them ("Return",
 *                               "Right Shift", "Keypad 8", "F5"...)
 *   [gamepad]    A = b          SDL controller buttons (a, b, x, y, back,
 *                               start, leftshoulder, rightshoulder, dpup...)
 *                               plus lefttrigger / righttrigger
 *   [hotkeys]    FastForward = Tab ...
 *   [options]    fast_forward_speed = 4   (0 = as fast as possible)
 *                rom_dir = D:\roms        (where to look for the player's ROM)
 *   [display]    fullscreen, layout (stacked / side_by_side), window_scale
 */
#include "platform_pc.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

static PcConfig s_cfg;
static char     s_path[1024];

static const char *const s_button_names[PC_BUTTON_COUNT] = {
    "A", "B", "Select", "Start", "Right", "Left", "Up", "Down", "R", "L", "X", "Y",
};

static const char *const s_hotkey_names[PC_HOTKEY_COUNT] = {
    "Quit", "SwapScreens", "Lid", "Fullscreen", "FastForward", "FastForwardToggle", "Mute", "Screenshot", "Filter",
};

static const char *const s_default_ini =
    "; Pokemon HeartGold / SoulSilver - PC port settings\n"
    "; Several keys can be bound to one button: separate them with commas.\n"
    "; Key names: as SDL names them (A..Z, 0..9, Up, Down, Left, Right, Return,\n"
    "; Space, Tab, Backspace, Left Shift, Right Shift, Left Ctrl, F1..F12,\n"
    "; Keypad 0..9...). Delete this file to get the defaults back.\n"
    "\n"
    "[keyboard]\n"
    "A = X\n"
    "B = Z\n"
    "X = S\n"
    "Y = A\n"
    "L = Q\n"
    "R = W\n"
    "Start = Return\n"
    "Select = Right Shift, Backspace\n"
    "Up = Up\n"
    "Down = Down\n"
    "Left = Left\n"
    "Right = Right\n"
    "\n"
    "; Gamepad: SDL button names (a b x y back start leftshoulder rightshoulder\n"
    "; dpup dpdown dpleft dpright leftstick rightstick lefttrigger righttrigger).\n"
    "; Default: Nintendo positional layout (east button = A, south = B).\n"
    "[gamepad]\n"
    "A = b\n"
    "B = a\n"
    "X = y\n"
    "Y = x\n"
    "L = leftshoulder, lefttrigger\n"
    "R = rightshoulder, righttrigger\n"
    "Start = start\n"
    "Select = back\n"
    "Up = dpup\n"
    "Down = dpdown\n"
    "Left = dpleft\n"
    "Right = dpright\n"
    "\n"
    "[hotkeys]\n"
    "Quit = Escape\n"
    "SwapScreens = F9\n"
    "Lid = F10\n"
    "Fullscreen = F11\n"
    "; held down\n"
    "FastForward = Tab\n"
    "; press once to turn on, again to turn off\n"
    "FastForwardToggle = F8\n"
    "Mute = M\n"
    "Screenshot = F12\n"
    "; cycles the picture filter\n"
    "Filter = F7\n"
    "\n"
    "[options]\n"
    "; speed while fast-forwarding: 2..10 times, 0 = as fast as possible\n"
    "fast_forward_speed = 4\n"
    "; gamepad analog stick acts as the D-pad (1 = yes)\n"
    "analog_stick_dpad = 1\n"
    "; language: auto (the language of your ROM), fr, en. English comes from\n"
    "; the game's source (pc_data/en), the rest from your ROM.\n"
    "language = auto\n"
    "; folder holding your ROMs (.nds), searched when no ROM sits next to the\n"
    "; game and its data is not extracted yet. The right ROM is picked from its\n"
    "; header (game code), whatever the file is called. Empty: not used.\n"
    "; Example: rom_dir = D:\\Games\\roms\n"
    "rom_dir =\n"
    "\n"
    "[display]\n"
    "; start in fullscreen (1 = yes); F11 switches at any time\n"
    "fullscreen = 0\n"
    "; screens (F9 switches at any time):\n"
    ";   large_top    big top screen, touch screen at its right (16:9, fills a PC monitor)\n"
    ";   stacked      top above bottom, like the DS\n"
    ";   side_by_side both screens the same size, next to each other\n"
    "layout = large_top\n"
    "; 1 = stretch the picture to the whole window (no black bars, but distorted\n"
    ";     unless the window has the layout's shape)\n"
    "stretch = 0\n"
    "; picture filter: sharp (crisp at any size), nearest (blocky), smooth,\n"
    ";                 scanlines (CRT look), lcd (DS screen grid)\n"
    "filter = sharp\n"
    "; window size at start, in multiples of the DS resolution\n"
    "window_scale = 2\n";

/* ---------------------------------------------------------------- parsing */

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = '\0';
    return s;
}

static int find_name(const char *const *names, int count, const char *name) {
    for (int i = 0; i < count; i++) {
        if (SDL_strcasecmp(names[i], name) == 0) return i;
    }
    return -1;
}

static void bind_keys(int32_t *slots, const char *value, const char *what) {
    memset(slots, 0, sizeof(int32_t) * PC_MAX_BINDINGS);
    char buf[256];
    SDL_strlcpy(buf, value, sizeof buf);
    int n = 0;
    for (char *tok = strtok(buf, ","); tok != NULL && n < PC_MAX_BINDINGS; tok = strtok(NULL, ",")) {
        char *name = trim(tok);
        if (*name == '\0') continue;
        SDL_Keycode k = SDL_GetKeyFromName(name);
        if (k == SDLK_UNKNOWN) {
            pc_warn("config: unknown key '%s' for %s", name, what);
            continue;
        }
        slots[n++] = k;
    }
}

static void bind_pad(int *slots, const char *value, const char *what) {
    for (int i = 0; i < PC_MAX_BINDINGS; i++) slots[i] = -1;
    char buf[256];
    SDL_strlcpy(buf, value, sizeof buf);
    int n = 0;
    for (char *tok = strtok(buf, ","); tok != NULL && n < PC_MAX_BINDINGS; tok = strtok(NULL, ",")) {
        char *name = trim(tok);
        if (*name == '\0') continue;
        if (SDL_strcasecmp(name, "lefttrigger") == 0) { slots[n++] = PC_PAD_TRIGGER_LEFT; continue; }
        if (SDL_strcasecmp(name, "righttrigger") == 0) { slots[n++] = PC_PAD_TRIGGER_RIGHT; continue; }
        SDL_GameControllerButton b = SDL_GameControllerGetButtonFromString(name);
        if (b == SDL_CONTROLLER_BUTTON_INVALID) {
            pc_warn("config: unknown gamepad button '%s' for %s", name, what);
            continue;
        }
        slots[n++] = (int)b;
    }
}

static void parse(const char *text) {
    char section[32] = "";
    const char *p = text;
    int lineno = 0;
    while (*p != '\0') {
        char line[512];
        size_t len = strcspn(p, "\r\n");
        if (len >= sizeof line) len = sizeof line - 1;
        memcpy(line, p, len);
        line[len] = '\0';
        p += strcspn(p, "\r\n");
        while (*p == '\r' || *p == '\n') p++;
        lineno++;
        char *l = trim(line);
        if (*l == '\0' || *l == ';' || *l == '#') continue;
        if (*l == '[') {
            char *e = strchr(l, ']');
            if (e != NULL) *e = '\0';
            SDL_strlcpy(section, trim(l + 1), sizeof section);
            continue;
        }
        char *eq = strchr(l, '=');
        if (eq == NULL) {
            pc_warn("config: line %d ignored (no '=')", lineno);
            continue;
        }
        *eq = '\0';
        char *key = trim(l), *value = trim(eq + 1);
        if (SDL_strcasecmp(section, "keyboard") == 0 || SDL_strcasecmp(section, "gamepad") == 0) {
            int b = find_name(s_button_names, PC_BUTTON_COUNT, key);
            if (b < 0) { pc_warn("config: unknown button '%s'", key); continue; }
            if (section[0] == 'k' || section[0] == 'K') bind_keys(s_cfg.keys[b], value, key);
            else bind_pad(s_cfg.pad[b], value, key);
        } else if (SDL_strcasecmp(section, "hotkeys") == 0) {
            int h = find_name(s_hotkey_names, PC_HOTKEY_COUNT, key);
            if (h < 0) { pc_warn("config: unknown hotkey '%s'", key); continue; }
            bind_keys(s_cfg.hotkeys[h], value, key);
        } else if (SDL_strcasecmp(section, "options") == 0) {
            if (SDL_strcasecmp(key, "fast_forward_speed") == 0) {
                s_cfg.fastForwardSpeed = atoi(value);
                if (s_cfg.fastForwardSpeed < 0) s_cfg.fastForwardSpeed = 0;
                if (s_cfg.fastForwardSpeed == 1) s_cfg.fastForwardSpeed = 2;
            } else if (SDL_strcasecmp(key, "analog_stick_dpad") == 0) {
                s_cfg.analogDpad = atoi(value) != 0;
            } else if (SDL_strcasecmp(key, "language") == 0) {
                SDL_strlcpy(s_cfg.language, value, sizeof s_cfg.language);
            } else if (SDL_strcasecmp(key, "rom_dir") == 0) {
                SDL_strlcpy(s_cfg.romDir, value, sizeof s_cfg.romDir);
            } else {
                pc_warn("config: unknown option '%s'", key);
            }
        } else if (SDL_strcasecmp(section, "display") == 0) {
            if (SDL_strcasecmp(key, "fullscreen") == 0) {
                s_cfg.fullscreen = atoi(value) != 0;
            } else if (SDL_strcasecmp(key, "layout") == 0) {
                if (SDL_strcasecmp(value, "stacked") == 0) s_cfg.layout = PC_LAYOUT_STACKED;
                else if (SDL_strcasecmp(value, "side_by_side") == 0) s_cfg.layout = PC_LAYOUT_SIDE_BY_SIDE;
                else if (SDL_strcasecmp(value, "large_top") == 0) s_cfg.layout = PC_LAYOUT_LARGE_TOP;
                else pc_warn("config: layout must be large_top, stacked or side_by_side, not '%s'", value);
            } else if (SDL_strcasecmp(key, "filter") == 0) {
                s_cfg.filter = -1;
                for (int f = 0; f < PC_FILTER_COUNT; f++) {
                    if (SDL_strcasecmp(value, pc_gfx_filter_name(f)) == 0) s_cfg.filter = f;
                }
                if (s_cfg.filter < 0) {
                    pc_warn("config: unknown filter '%s'", value);
                    s_cfg.filter = PC_FILTER_SHARP;
                }
            } else if (SDL_strcasecmp(key, "stretch") == 0) {
                s_cfg.stretch = atoi(value) != 0;
            } else if (SDL_strcasecmp(key, "window_scale") == 0) {
                s_cfg.windowScale = atoi(value);
                if (s_cfg.windowScale < 1) s_cfg.windowScale = 1;
                if (s_cfg.windowScale > 8) s_cfg.windowScale = 8;
            } else {
                pc_warn("config: unknown display option '%s'", key);
            }
        }
    }
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    if (buf != NULL) {
        n = (long)fread(buf, 1, (size_t)n, f);
        buf[n] = '\0';
    }
    fclose(f);
    return buf;
}

void pc_config_load(void) {
    static bool loaded;
    if (loaded) {
        return;   /* read once: before the window opens, then by input init */
    }
    loaded = true;
    memset(&s_cfg, 0, sizeof s_cfg);
    SDL_strlcpy(s_cfg.language, "auto", sizeof s_cfg.language);
    parse(s_default_ini);   /* defaults first, then the user's file on top */

    const char *candidates[3] = { getenv("PHG_CONFIG"), "config.ini", NULL };
    char exe_path[1024] = "";
    char *base = SDL_GetBasePath();
    if (base != NULL) {
        SDL_snprintf(exe_path, sizeof exe_path, "%sconfig.ini", base);
        SDL_free(base);
        candidates[2] = exe_path;
    }
    for (int i = 0; i < 3; i++) {
        if (candidates[i] == NULL || candidates[i][0] == '\0') continue;
        char *text = read_file(candidates[i]);
        if (text != NULL) {
            parse(text);
            free(text);
            SDL_strlcpy(s_path, candidates[i], sizeof s_path);
            pc_log("config: %s", s_path);
            return;
        }
    }
    /* none: write the defaults next to the executable */
    if (exe_path[0] != '\0') {
        FILE *f = fopen(exe_path, "wb");
        if (f != NULL) {
            fputs(s_default_ini, f);
            fclose(f);
            SDL_strlcpy(s_path, exe_path, sizeof s_path);
            pc_log("config: wrote defaults to %s", s_path);
        }
    }
}

const PcConfig *pc_config(void) {
    return &s_cfg;
}
