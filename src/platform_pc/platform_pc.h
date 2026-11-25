/*
 * PC hardware abstraction layer (HAL) for the pokeheartgold native port.
 *
 * This header is the *host-side* interface. It deliberately does not include
 * any NitroSDK header: the Nitro-compatible entry points (OS_*, FS_*, PAD_*,
 * CARD_*, GX_*...) will be thin adapters built on top of these functions
 * (milestone 2, see PORTING_ROADMAP.md).
 */
#ifndef PHG_PLATFORM_PC_H
#define PHG_PLATFORM_PC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ log */
void pc_log(const char *fmt, ...);
void pc_warn(const char *fmt, ...);
void pc_fatal(const char *fmt, ...);
/* Prints exception + backtrace (stable addresses: fixed image base). */
void pc_crash_install(void);
/* Runs fn(arg) on a stack below 4 GiB (Linux/macOS; direct call on Windows). */
void pc_run_on_low_stack(void (*fn)(void *), void *arg);
/* Call once per frame; a 5 s stall prints the game thread backtrace. */
void pc_watchdog_kick(void);

/* ------------------------------------------------------------- os_pc.c */
/* DS LCD refresh rate: 33.513982 MHz / (6 * 355 * 263) = 59.8261 Hz. */
#define PC_DS_FRAME_RATE 59.8261
#define PC_DS_CPU_HZ     33513982u   /* ARM9 bus clock: OS tick base   */

bool     pc_os_init(void);
void     pc_os_shutdown(void);
uint64_t pc_os_ticks(void);           /* OS_GetTick() units (CPU_HZ/64)  */
uint64_t pc_os_ms(void);
/* Blocks until the next emulated VBlank; returns the number of VBlanks
 * elapsed since the previous call (>1 when the host fell behind). */
uint32_t pc_os_wait_vblank(void);
/* Emulation speed: 1 = normal (59.83 Hz), N = N times faster, 0 = unlimited. */
void     pc_os_set_speed(int speed);
int      pc_os_speed(void);
uint64_t pc_os_vblank_count(void);
void    *pc_os_alloc(size_t size, size_t align);
void     pc_os_free(void *ptr);

/* ------------------------------------------------------------- fs_pc.c */
#ifndef PHG_REPO_DIR
#define PHG_REPO_DIR "."          /* set by CMake to the repository root */
#endif
#define PHG_DEFAULT_LANG "fr"

/* The two versions are two separate games on PC, as on DS: each executable
 * only accepts its own ROM and keeps its own data, mods and save, so both can
 * live in one folder. (CMake: PHG_GAME_VERSION=HEARTGOLD or SOULSILVER.) */
#ifdef SOULSILVER
#define PC_GAME_TITLE    "Pokemon SoulSilver"
#define PC_GAME_TITLE_FR "Pok\xc3\xa9mon Argent SoulSilver"
#define PC_GAME_TITLE_EN "Pok\xc3\xa9mon SoulSilver"
#define PC_GAME_CODE3    "IPG"
#define PC_DATA_DIR      "pc_data_ss"
#define PC_MODS_DIR      "mods_ss"
#define PC_SAVE_DEFAULT  "save_ss.bin"
#else
#define PC_GAME_TITLE    "Pokemon HeartGold"
#define PC_GAME_TITLE_FR "Pok\xc3\xa9mon Or HeartGold"
#define PC_GAME_TITLE_EN "Pok\xc3\xa9mon HeartGold"
#define PC_GAME_CODE3    "IPK"
#define PC_DATA_DIR      "pc_data"
#define PC_MODS_DIR      "mods"
#define PC_SAVE_DEFAULT  "save.bin"
#endif
typedef struct PcFile {
    void    *handle;     /* FILE*                                        */
    uint32_t start;      /* byte range inside the host file (NARC member) */
    uint32_t length;
    uint32_t pos;        /* relative to start                             */
} PcFile;

typedef enum { PC_SEEK_SET = 0, PC_SEEK_CUR = 1, PC_SEEK_END = 2 } PcSeek;

/* Layered search path: pc_fs_init() sets the highest-priority root, each
 * pc_fs_add_root() appends a lower-priority one (mods -> language -> base). */
bool     pc_fs_init(const char *root_dir);
bool     pc_fs_add_root(const char *root_dir);
void     pc_fs_shutdown(void);
const char *pc_fs_root(void);
int      pc_fs_root_count(void);
const char *pc_fs_root_at(int i);
/* Standard layers under repo_dir, highest priority first, skipping missing
 * directories: mods/<lang>, mods, pc_data/<lang>, files. */
bool     pc_fs_setup_layers(const char *repo_dir, const char *lang);
bool     pc_fs_exists(const char *nitro_path);
bool     pc_fs_open(PcFile *f, const char *nitro_path);
void     pc_fs_close(PcFile *f);
int32_t  pc_fs_read(PcFile *f, void *dst, int32_t len);
bool     pc_fs_seek(PcFile *f, int32_t offset, PcSeek whence);
/* Loads a whole file; caller frees with pc_os_free(). */
void    *pc_fs_load(const char *nitro_path, uint32_t *out_size);

/* NARC (Nitro ARChive) access, same layout as filesystem.c expects. */
typedef struct PcNarc {
    PcFile   file;
    uint32_t count;
    uint32_t fat_offset;   /* absolute offset of the first BTAF entry */
    uint32_t img_offset;   /* absolute offset of GMIF payload         */
} PcNarc;

bool     pc_narc_open(PcNarc *n, const char *nitro_path);
void     pc_narc_close(PcNarc *n);
bool     pc_narc_member(PcNarc *n, uint32_t index, uint32_t *offset, uint32_t *size);
void    *pc_narc_load_member(PcNarc *n, uint32_t index, uint32_t *out_size);

typedef struct PcFsReport {
    uint32_t total;
    uint32_t present;
    uint32_t missing;
    char     first_missing[512];
} PcFsReport;
/* Checks every path listed (one per line) in manifest_path. */
bool     pc_fs_check_manifest(const char *manifest_path, PcFsReport *out, bool verbose);

/* --------------------------------------------------------- install_pc.c */
/* Folder holding pc_data/, mods/, the ROM and the save (exe dir for a
 * release, the source tree for a developer build). */
const char *pc_install_base_dir(void);
/* Makes sure pc_data/<lang> exists (extracts a ROM found in the base dir or
 * in rom_dir on first run). NULL lang: the language found or extracted. Returns the
 * language to use, or NULL (a message box told the player what to do). */
const char *pc_install_prepare(const char *requested_lang);
/* Extra folder searched for the ROM (--rom-dir); overrides config rom_dir. */
void        pc_install_set_rom_dir(const char *dir);
/* Relative save names are resolved next to the executable. */
void        pc_install_save_path(const char *name, char *out, size_t n);

/* ---------------------------------------------------------- input_pc.c */
/* Bit layout identical to NitroSDK PAD_BUTTON_* / PAD_KEY_* */
enum {
    PC_PAD_A = 0x0001, PC_PAD_B = 0x0002, PC_PAD_SELECT = 0x0004, PC_PAD_START = 0x0008,
    PC_PAD_RIGHT = 0x0010, PC_PAD_LEFT = 0x0020, PC_PAD_UP = 0x0040, PC_PAD_DOWN = 0x0080,
    PC_PAD_R = 0x0100, PC_PAD_L = 0x0200, PC_PAD_X = 0x0400, PC_PAD_Y = 0x0800,
    PC_PAD_DEBUG = 0x2000,
};

typedef struct PcTouch {
    bool    touching;
    uint16_t x, y;       /* 0..255, 0..191 in bottom-screen pixels */
} PcTouch;

bool     pc_input_init(void);
void     pc_input_shutdown(void);
/* Processes pending SDL events. Returns false when the user asked to quit. */
bool     pc_input_poll(void);
uint16_t pc_input_buttons(void);      /* active-high PAD mask           */
PcTouch  pc_input_touch(void);
void     pc_input_set_test_touch(bool touching, uint16_t x, uint16_t y);   /* test hooks */
bool     pc_input_lid_closed(void);   /* PAD_DetectFold equivalent      */

/* ---------------------------------------------------------- config_pc.c */
/* config.ini: key / gamepad bindings, hotkeys, options. Buttons are indexed
 * by their bit number in the PC_PAD_* mask (0 = A ... 11 = Y). */
#define PC_BUTTON_COUNT      12
#define PC_MAX_BINDINGS      4
#define PC_PAD_TRIGGER_LEFT  100   /* gamepad "buttons" beyond SDL's list */
#define PC_PAD_TRIGGER_RIGHT 101
enum PcHotkey {
    PC_HOTKEY_QUIT, PC_HOTKEY_SWAP_SCREENS, PC_HOTKEY_LID, PC_HOTKEY_FULLSCREEN,
    PC_HOTKEY_FAST_FORWARD, PC_HOTKEY_FAST_FORWARD_TOGGLE, PC_HOTKEY_MUTE, PC_HOTKEY_SCREENSHOT,
    PC_HOTKEY_FILTER,
    PC_HOTKEY_COUNT
};
typedef struct PcConfig {
    int32_t keys[PC_BUTTON_COUNT][PC_MAX_BINDINGS];    /* SDL keycodes, 0 = none */
    int     pad[PC_BUTTON_COUNT][PC_MAX_BINDINGS];     /* SDL buttons, -1 = none */
    int32_t hotkeys[PC_HOTKEY_COUNT][PC_MAX_BINDINGS];
    int     fastForwardSpeed;                          /* 0 = unlimited */
    bool    analogDpad;
    bool    fullscreen;                                /* [display] */
    int     layout;                                    /* PcLayout */
    bool    stretch;
    int     filter;                                    /* PC_FILTER_* */
    int     windowScale;
    char    language[8];                               /* [options] auto, fr, en... */
    char    romDir[512];                               /* [options] rom_dir, "" = none */
} PcConfig;
void            pc_config_load(void);
const PcConfig *pc_config(void);

/* ----------------------------------------------------------- save_pc.c */
/* HGSS uses a 4 Mbit (512 KiB) SPI flash backup chip. */
#define PC_SAVE_SIZE   (512u * 1024u)
#define PC_SAVE_SECTOR 0x1000u

bool     pc_save_init(const char *path);
void     pc_save_shutdown(void);
bool     pc_save_read(uint32_t offset, void *dst, uint32_t len);
bool     pc_save_write(uint32_t offset, const void *src, uint32_t len);
bool     pc_save_erase(uint32_t offset, uint32_t len);  /* fills 0xFF */
bool     pc_save_flush(void);

/* ------------------------------------------------------------ gfx_pc.c */
#define PC_SCREEN_W 256
#define PC_SCREEN_H 192

typedef enum { PC_SCREEN_TOP = 0, PC_SCREEN_BOTTOM = 1 } PcScreen;
typedef enum { PC_LAYOUT_STACKED = 0, PC_LAYOUT_SIDE_BY_SIDE = 1, PC_LAYOUT_LARGE_TOP = 2, PC_LAYOUT_COUNT } PcLayout;

bool     pc_gfx_init(const char *title, int scale);
void     pc_gfx_shutdown(void);
/* 256x192 RGBA8888 framebuffer for one screen; written by the 2D/3D
 * renderers, uploaded by pc_gfx_present(). */
uint32_t *pc_gfx_framebuffer(PcScreen screen);
/* Saves an RGBA8 image (pixels as R | G<<8 | B<<16 | A<<24) as PNG. */
bool     pc_write_png(const char *path, const uint32_t *rgba, int width, int height);
void     pc_gfx_present(void);
void     pc_gfx_set_layout(PcLayout layout);
PcLayout pc_gfx_layout(void);
void     pc_gfx_set_stretch(bool stretch);   /* fill the window (distorts) */
enum { PC_FILTER_NEAREST, PC_FILTER_SHARP, PC_FILTER_SMOOTH, PC_FILTER_SCANLINES, PC_FILTER_LCD, PC_FILTER_COUNT };
void        pc_gfx_set_filter(int filter);
int         pc_gfx_filter(void);
const char *pc_gfx_filter_name(int filter);
void     pc_gfx_fit_window(int scale);       /* windowed: match the layout's shape */
bool     pc_gfx_stretch(void);
void     pc_gfx_toggle_fullscreen(void);
void     pc_gfx_set_title(const char *title);
/* Window pixel -> bottom-screen pixel. Returns false outside the screen. */
bool     pc_gfx_window_to_touch(int wx, int wy, uint16_t *tx, uint16_t *ty);
void     pc_gfx_on_resize(void);

static inline uint32_t pc_rgb555_to_rgba(uint16_t c) {
    uint32_t r = (c >> 0) & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
    r = (r << 3) | (r >> 2);
    g = (g << 3) | (g >> 2);
    b = (b << 3) | (b >> 2);
    return r | (g << 8) | (b << 16) | 0xFF000000u; /* little-endian RGBA bytes */
}

/* ---------------------------------------------------------- audio_pc.c */
#define PC_AUDIO_RATE 32768   /* DS mixer output rate */

typedef void (*PcAudioMixFn)(int16_t *stereo_out, int frames, void *user);

bool     pc_audio_init(void);
void     pc_audio_shutdown(void);
void     pc_audio_set_mixer(PcAudioMixFn fn, void *user);
void     pc_audio_pause(bool paused);
bool     pc_audio_active(void);   /* an output device is open */
void     pc_audio_set_muted(bool muted);   /* speakers silent, mixer still runs */
bool     pc_audio_muted(void);
void     pc_audio_lock(void);     /* excludes the mixer callback */
void     pc_audio_unlock(void);

#ifdef __cplusplus
}
#endif

#endif /* PHG_PLATFORM_PC_H */
