/*
 * dsda-doom compatibility module
 */

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>

#include "doomdef.h"
#include "doomstat.h"
#include "d_main.h"
#include "d_ticcmd.h"
#include "g_game.h"
#include "r_main.h"
#include "m_misc.h"
#include "i_main.h"
#include "i_system.h"
#include "i_sound.h"
#include "i_video.h"
#include "p_tick.h"
#include "hu_stuff.h"
#include "st_stuff.h"
#include "dsda/args.h"
#include "dsda/time.h"
#include "dsda/key_frame.h"
#include "dsda/options.h"
#include "z_zone.h"
#include "e6y.h"
#include "SDL.h"
#include "smooth.h"
#include "r_fps.h"

extern SDL_Window *sdl_window;
extern void dsda_UpdateFlag(dsda_arg_identifier_t id, dboolean found);

extern thinker_t thinkercap;
extern int dogs;

/* g_game.c shared */
extern const ticcmd_t *(*g_xdre_cmd_override)(int player);

int palette_ondamage = 0;
int palette_onbonus  = 0;
int palette_onpowers = 0;

ticcmd_t netcmds[4];
int basetic = 0;

static const ticcmd_t *XDRE_GetPlayerCmd(int player) {
    if (player >= 0 && player < 4)
        return &netcmds[player];
    return NULL;
}

/* system funcs */

int signal_context = 0;

dboolean I_Interrupted(void) {
    return false;
}

void I_Init(void) {
    dsda_ResetTimeFunctions(fastdemo);
    if (!dsda_Flag(dsda_arg_nosound))
        I_InitSound();
}

void I_Init2(void) {
    dsda_ResetTimeFunctions(fastdemo);
    force_singletics_to = gametic + BACKUPTICS;
}

typedef struct atexit_listentry_s atexit_listentry_t;
struct atexit_listentry_s {
    atexit_func_t func;
    dboolean run_on_error;
    atexit_listentry_t *next;
    const char* name;
};

static atexit_listentry_t *exit_funcs[exit_priority_max];

void I_AtExit(atexit_func_t func, dboolean run_on_error,
              const char* name, exit_priority_t priority) {
    atexit_listentry_t *entry;

    if (!func || priority < 0 || priority >= exit_priority_max)
        return;

    /* Prevent duplicate registrations of the same function at this priority */
    for (entry = exit_funcs[priority]; entry; entry = entry->next) {
        if (entry->func == func)
            return;
    }

    entry = (atexit_listentry_t*)Z_Malloc(sizeof(*entry));
    entry->func = func;
    entry->run_on_error = run_on_error;
    entry->next = exit_funcs[priority];
    entry->name = name;
    exit_funcs[priority] = entry;
}

void I_SafeExit(int rc) {
    int priority;
    for (priority = 0; priority < exit_priority_max; priority++) {
        while (exit_funcs[priority]) {
            atexit_listentry_t *entry = exit_funcs[priority];
            exit_funcs[priority] = entry->next;
            if (entry->func)
                entry->func();
            Z_Free(entry);
        }
    }
    exit(rc);
}

void mainDoom(int argc, char** argv) {
    dsda_ParseCommandLineArgs(argc, argv);

    /* force -nosound and -nomouse */
    dsda_UpdateFlag(dsda_arg_nosound, true);
    dsda_UpdateFlag(dsda_arg_nomouse, true);

    nosfxparm = true;
    nomusicparm = true;

    /* If neither -warp nor demo was specified on the CLI, configure warp to map 1 natively */
    if (!dsda_Flag(dsda_arg_warp) && !dsda_Flag(dsda_arg_playdemo) && !dsda_Flag(dsda_arg_timedemo)) {
        dsda_arg_t *warp_arg = dsda_Arg(dsda_arg_warp);
        warp_arg->found = true;
        warp_arg->count = 1;
        if (!warp_arg->value.v_int_array) {
            warp_arg->value.v_int_array = Z_Malloc(sizeof(int));
        }
        warp_arg->value.v_int_array[0] = 1;

        if (!dsda_Flag(dsda_arg_skill)) {
            dsda_UpdateIntArg(dsda_arg_skill, "4"); /* UV */
        }
    }

    M_LoadDefaults();
    setlocale(LC_TIME, "");
    I_PreInitGraphics();

    /* D_DoomMain will now autostart MAP01 / E1M1 directly inside G_InitNew */
    D_DoomMain();

    /* g_ticker netcmd hook */
    g_xdre_cmd_override = XDRE_GetPlayerCmd;

    if (!thinkercap.next) {
        P_InitThinkers();
    }

    /* Single-player console/display defaults */
    playeringame[0] = true;
    for (int i = 1; i < g_maxplayers; i++) {
        playeringame[i] = false;
    }
    consoleplayer = 0;
    displayplayer = 0;

    /* status bar and hud */
    ST_Start();
    HU_Start();
	
	/* savepoint and TAS buffer mem leak protection */
    {
        extern void x_clearAllSavepoints(void);
        extern void x_shutdown(void);
        atexit(x_shutdown);
        atexit(x_clearAllSavepoints);
        I_AtExit(x_clearAllSavepoints, true, "x_clearAllSavepoints", exit_priority_normal);
        I_AtExit(x_shutdown, true, "x_shutdown", exit_priority_normal);
    }

    /* keyframe reset */
    dsda_InitAutoKeyFrames();

    movement_smooth = 0;
    demo_smoothturns = 0;

    dsda_UpdateIntConfig(dsda_config_uncapped_framerate, 0, false);
}

/* i/o */
void XDRE_WriteDemoTiccmd(const ticcmd_t *cmd, FILE *file) {
    if (!longtics) {
        fputc(cmd->forwardmove, file);
        fputc(cmd->sidemove, file);
        fputc((cmd->angleturn) >> 8, file);
        fputc(cmd->buttons, file);
    } else {
        fputc(cmd->forwardmove, file);
        fputc(cmd->sidemove, file);
        fputc(cmd->angleturn & 0xff, file);
        fputc((cmd->angleturn >> 8) & 0xff, file);
        fputc(cmd->buttons, file);
    }
}

void XDRE_WriteDemoFooter(FILE *file) {
    (void)file;
}

/* expose demo header to c++ */
unsigned char const* XDRE_ReadDemoHeader(unsigned char const* demo_p, size_t size) {
    return G_ReadDemoHeaderEx(demo_p, size, 0);
}