/*
 * game_main_pc.c - entry point of the real game on PC.
 *
 * Brings up the host layer (window, input, audio) and the Nitro layer
 * (DS memory map, OS, NitroFS, save), then hands control to the game's own
 * NitroMain() (src/main.c), exactly as the DS boot code does. The game's
 * main loop paces itself through OS_WaitIrq(VBlank), which drives
 * pc_nitro_vblank() at 59.83 Hz.
 *
 * Note: this file is compiled with the game's include paths and must not
 * include SDL (see PORTING_ROADMAP.md, M2).
 *
 * Usage: pokeheartgold [--lang fr|en] [--data DIR] [--save FILE] [--scale N]
 *                      [--rom-dir DIR]
 * First run: the player's ROM (*.nds) in the game folder, or else in rom_dir
 * (--rom-dir, config.ini [options] rom_dir), is extracted to
 * pc_data/<lang> (install_pc.c); the language follows the ROM.
 * Test hooks (environment): PHG_EXIT_AFTER, PHG_TEST_RESET_AT (nitro_pc.c).
 */
#include "nitro_pc.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void NitroMain(void);

static int is_auto(const char *s) {
    const char *a = "auto";
    while (*s && *a && tolower((unsigned char)*s) == *a) {
        s++;
        a++;
    }
    return *s == '\0' && *a == '\0';
}

static void run_game(void *unused) {
    (void)unused;
    pc_nitro_run(NitroMain);   /* never returns; soft resets restart it */
}

int main(int argc, char **argv) {
    const char *lang = getenv("PHG_LANG");   /* NULL: the language of the data found */
    const char *data = getenv("PHG_DATA_DIR");
    const char *save = NULL;   /* NULL: save.bin next to the game data */
    int scale = 0;   /* 0: window_scale from config.ini */
    for (int i = 1; i + 1 < argc; i += 2) {
        if (!strcmp(argv[i], "--lang")) lang = argv[i + 1];
        else if (!strcmp(argv[i], "--data")) data = argv[i + 1];
        else if (!strcmp(argv[i], "--save")) save = argv[i + 1];
        else if (!strcmp(argv[i], "--scale")) scale = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--rom-dir")) pc_install_set_rom_dir(argv[i + 1]);
        else {
            fprintf(stderr, "usage: pokeheartgold [--lang fr|en] [--data DIR] [--save FILE] [--scale N] [--rom-dir DIR]\n");
            return 2;
        }
    }

    pc_crash_install();
    if (!pc_os_init()) {
        pc_fatal("timer init failed");
    }
    pc_config_load();
    const PcConfig *cfg = pc_config();
    if (!pc_gfx_init(PC_GAME_TITLE, scale > 0 ? scale : cfg->windowScale)) {
        pc_fatal("graphics init failed (OpenGL 3.3 required)");
    }
    pc_gfx_set_stretch(cfg->stretch);
    pc_gfx_set_filter(cfg->filter);
    pc_gfx_set_layout((PcLayout)cfg->layout);
    pc_gfx_fit_window(scale > 0 ? scale : cfg->windowScale);
    if (cfg->fullscreen) {
        pc_gfx_toggle_fullscreen();
    }
    pc_input_init();
    pc_audio_init();
    pc_nitro_set_window(TRUE);
    if (lang == NULL && cfg->language[0] != '\0' && !is_auto(cfg->language)) {
        lang = cfg->language;              /* config.ini [options] language */
    }
    if (data == NULL) {
        lang = pc_install_prepare(lang);   /* extracts the player's ROM on first run */
        if (lang == NULL) {
            return 1;
        }
    } else if (lang == NULL) {
        lang = PHG_DEFAULT_LANG;
    }
    static char save_path[1024];
    pc_install_save_path(save, save_path, sizeof save_path);
    if (!pc_nitro_init(data, lang, save_path)) {
        pc_fatal("Nitro layer init failed (see log above)");
    }
    pc_log("starting NitroMain (lang=%s)", lang);
    pc_run_on_low_stack(run_game, NULL);   /* game code keeps stack addresses in 32-bit words */
    return 0;
}
