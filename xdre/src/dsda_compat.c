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
#include "w_wad.h"
#include "am_map.h"
#include "d_deh.h"
#include "dsda/messenger.h"


extern SDL_Window *sdl_window;
extern void dsda_UpdateFlag(dsda_arg_identifier_t id, dboolean found);

extern thinker_t thinkerclasscap[];
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

/* automap */
extern int automap_full;
int x_automap_wanted = 0;
extern int automap_follow;
extern void XDRE_PanAutomap(int dx, int dy);
extern void XDRE_ZoomAutomap(int direction);

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

    /* prevent duplicate registration */
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

static void XDRE_FreeWarpArg(void) {
    dsda_arg_t *warp_arg = dsda_Arg(dsda_arg_warp);
    if (warp_arg && warp_arg->value.v_int_array) {
        Z_Free(warp_arg->value.v_int_array);
        warp_arg->value.v_int_array = NULL;
    }
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

    /* configure warp option */
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

    /* autostart specified map */
    D_DoomMain();

    /* g_ticker netcmd hook */
    g_xdre_cmd_override = XDRE_GetPlayerCmd;

    if (!thinkercap.next) {
        P_InitThinkers();
    }

    /* console/display defaults */
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
        atexit(XDRE_FreeWarpArg);
        I_AtExit(x_clearAllSavepoints, true, "x_clearAllSavepoints", exit_priority_normal);
        I_AtExit(x_shutdown, true, "x_shutdown", exit_priority_normal);
        I_AtExit(XDRE_FreeWarpArg, true, "XDRE_FreeWarpArg", exit_priority_normal);
    }

    /* keyframe reset */
    dsda_InitAutoKeyFrames();

    movement_smooth = 0;
    demo_smoothturns = 0;

    dsda_UpdateIntConfig(dsda_config_uncapped_framerate, 0, false);
}

void XDRE_WriteDemoTiccmd(const ticcmd_t *cmd, FILE *file) {
    fputc(cmd->forwardmove, file);
    fputc(cmd->sidemove, file);

    // 5 byte turns in mbf21
    if (longtics || compatibility_level == mbf21_compatibility) {
        fputc(cmd->angleturn & 0xff, file);
        fputc((cmd->angleturn >> 8) & 0xff, file);
    } else {
        fputc((cmd->angleturn) >> 8, file);
    }

    fputc(cmd->buttons, file);
}

void XDRE_WriteDemoFooter(FILE *file) {
    (void)file;
}

/* expose demo header to c++ */
unsigned char const* XDRE_ReadDemoHeader(unsigned char const* demo_p, size_t size) {
    return G_ReadDemoHeaderEx(demo_p, size, 0);
}

void XDRE_GetLoadedWads(char *iwad_out, size_t iwad_len, char *pwad_out, size_t pwad_len) {
    size_t i;
    iwad_out[0] = '\0';
    pwad_out[0] = '\0';

    for (i = 0; i < numwadfiles; ++i) {
        const char *path = wadfiles[i].name;
        if (!path || !*path)
            continue;

        /* extract filename from path for comparison */
        const char *slash = strrchr(path, '/');
        const char *bslash = strrchr(path, '\\');
        const char *name = path;
        if (slash && slash >= name) name = slash + 1;
        if (bslash && bslash >= name) name = bslash + 1;

        /* exclude dsda-doom.wad */
        if (strcasecmp(name, "dsda-doom.wad") == 0)
            continue;

        /* check against 4 standard iwads */
        if (strcasecmp(name, "doom.wad") == 0 ||
            strcasecmp(name, "doom2.wad") == 0 ||
            strcasecmp(name, "plutonia.wad") == 0 ||
            strcasecmp(name, "tnt.wad") == 0) {
            strncpy(iwad_out, path, iwad_len - 1);
            iwad_out[iwad_len - 1] = '\0';
        } else {
            /* append pwads with spaces */
            char quoted[1024];
            snprintf(quoted, sizeof(quoted), "\"%s\" ", path);
            if (strlen(pwad_out) + strlen(quoted) < pwad_len) {
                strcat(pwad_out, quoted);
            }
        }
    }

    /* strip space if only one pwad was added */
    size_t pwad_len_actual = strlen(pwad_out);
    if (pwad_len_actual > 0 && pwad_out[pwad_len_actual - 1] == ' ') {
        pwad_out[pwad_len_actual - 1] = '\0';
    }
}

/* automap funcs */
void XDRE_ToggleAutomap(void) {
    if (gamestate != GS_LEVEL)
        return;

    x_automap_wanted = !x_automap_wanted;

    if (x_automap_wanted) {
        AM_Start(AM_OPEN_FULLAUTOMAP);
    } else {
        AM_Stop(false);
    }
}

int XDRE_GetAutomapActive(void) {
    return x_automap_wanted ? 1 : 0;
}

void XDRE_ToggleAutomapFollow(void) {
    int nextVal = automap_follow ? 0 : 1;
    automap_follow = nextVal;
    dsda_UpdateIntConfig(dsda_config_automap_follow, nextVal, true);
    dsda_AddMessage(automap_follow ? s_AMSTR_FOLLOWON : s_AMSTR_FOLLOWOFF);
}

int XDRE_GetAutomapFollow(void) {
    return automap_follow ? 1 : 0;
}

void XDRE_DoPanAutomap(int dx, int dy) {
    if (gamestate != GS_LEVEL)
        return;
    XDRE_PanAutomap(dx, dy);
}

void XDRE_DoZoomAutomap(int direction) {
    if (gamestate != GS_LEVEL)
        return;
    XDRE_ZoomAutomap(direction);
}