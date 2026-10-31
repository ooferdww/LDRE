/*
 * XDRE / DSDA-Doom C Implementation Bridge (Strict C90)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cstuff.h"
#include "doomtype.h"
#include "doomdata.h"
#include "d_ticcmd.h"
#include "doomstat.h"
#include "m_random.h"
#include "d_player.h"
#include "g_game.h"
#include "lprintf.h"
#include "p_saveg.h"
#include "p_map.h"
#include "p_mobj.h"
#include "r_draw.h"
#include "r_main.h"
#include "s_sound.h"
#include "p_tick.h"
#include "p_setup.h"
#include "dsda/args.h"
#include "dsda/skill_info.h"
#include "d_main.h"
#include "smooth.h"
#include "r_fps.h"

#ifndef MAXPLAYERS
#define MAXPLAYERS 4
#endif

#define XDRE_SAVEGAMESIZE (256 * 1024 * 1024)

// extern void dsda_SetCompatibilityLevel(int complevel);
extern void G_XDRE_SetDemoOptions(int no_monsters, int respawn, int fast);
extern void RecalculateDrawnSubsectors(void);
extern int totalleveltimes;
extern dboolean setsizeneeded;
extern byte* savebuffer;
// extern size_t save_size;
extern void P_SetSaveBuffer(byte *buf, int size);
extern byte* demobuffer;
extern dboolean respawnmonsters;
extern ticcmd_t netcmds[4];

extern int basetic;

extern int numsectors; 
extern sector_t* sectors; 
int secID = 0;
int xSectorInfo[3];

extern thinker_t thinkerclasscap[];
void P_MobjThinker(mobj_t *mobj);
int thingID = 0;
int xThingInfo[8];

extern int numlines;
extern line_t* lines;
int xLineId = 0;
extern int xLineCrossed;
int xLineVertexes[2][2];

float angles = 16384.0f;

typedef struct {
    int tic;
    byte* save;
    size_t size;
    size_t capacity;
} xSavepoint_t;

#define MAXSSP 100

static xSavepoint_t savepoints[MAXSSP];

int x_longtics = 0;
int usedHeader = 1;

static int local_compatflags = 0;

int playeringameGet(int plr) {
    if (plr < 0 || plr > 3)
        return 0;

    return playeringame[plr];
}

void playeringameSet(int plr, int val) {
    if (plr < 0 || plr > 3)
        return;

    playeringame[plr] = (val == 0) ? 0 : 1;
}

void x_setLinedefCheck(int linedef) {
    if (linedef == -1)
        linedef = xLineId;

    xLineId = -1;
    xLineVertexes[0][0] = 0;
    xLineVertexes[0][1] = 0;
    xLineVertexes[1][0] = 0;
    xLineVertexes[1][1] = 0;

    if (gamestate != GS_LEVEL || !lines || numlines <= 0)
        return;

    if (linedef >= 0 && linedef < numlines) {
        if (lines[linedef].v1 && lines[linedef].v2) {
            xLineId = linedef;
            xLineVertexes[0][0] = lines[linedef].v1->x;
            xLineVertexes[0][1] = lines[linedef].v1->y;
            xLineVertexes[1][0] = lines[linedef].v2->x;
            xLineVertexes[1][1] = lines[linedef].v2->y;
        }
    }
}

void x_setSectorCheck(int sector) {
    int i;

    if (sector == -1)
        sector = secID;

    secID = -1;
    for (i = 0; i < 3; ++i) {
        xSectorInfo[i] = 0;
    }

    if (gamestate != GS_LEVEL || !sectors || numsectors <= 0)
        return;

    if (sector >= 0 && sector < numsectors) {
        secID = sector;
        xSectorInfo[0] = sectors[sector].floorheight;
        xSectorInfo[1] = sectors[sector].ceilingheight;
        xSectorInfo[2] = sectors[sector].special;
    }
}

void x_setThingCheck(int index) {
    int i = 0;
    thinker_t *th;
    mobj_t *target_mo = NULL;

    if (index == -1)
        index = thingID;

    thingID = -1;
    for (i = 0; i < 8; ++i) {
        xThingInfo[i] = 0;
    }

    if (index < 0 || gamestate != GS_LEVEL)
        return;

    /* active console/display player */
    if (index == 0) {
        if (players[displayplayer].mo) {
            target_mo = players[displayplayer].mo;
            thingID = 0;
        }
    } else {
        /* scan thru non-player mobj */
        i = 1;
        for (th = thinkercap.next; th != &thinkercap; th = th->next) {
            if ((void*)th->function == (void*)P_MobjThinker) {
                mobj_t *mo = (mobj_t *)th;
                if (mo->player)
                    continue; /* Skip player mobjs in this list */

                if (i == index) {
                    target_mo = mo;
                    thingID = index;
                    break;
                }
                ++i;
            }
        }
    }

    if (target_mo) {
        xThingInfo[0] = target_mo->x;
        xThingInfo[1] = target_mo->y;
        xThingInfo[2] = target_mo->z;
        xThingInfo[3] = target_mo->radius;
        xThingInfo[4] = target_mo->tics;
        xThingInfo[5] = target_mo->health;
        xThingInfo[6] = target_mo->reactiontime;
        xThingInfo[7] = target_mo->threshold;
    }
}

int x_getRngIndex(void) {
    return rng.rndindex;
}

double x_getXPos(void) {
    if (gamestate != GS_LEVEL || !players[displayplayer].mo)
        return 0.0;
    return players[displayplayer].mo->x / (double)FRACUNIT;
}

double x_getYPos(void) {
    if (gamestate != GS_LEVEL || !players[displayplayer].mo)
        return 0.0;
    return players[displayplayer].mo->y / (double)FRACUNIT;
}

double x_getZPos(void) {
    if (gamestate != GS_LEVEL || !players[displayplayer].mo)
        return 0.0;
    return players[displayplayer].mo->z / (double)FRACUNIT;
}

double x_getPrevXPos(void) {
    if (gamestate != GS_LEVEL || !players[displayplayer].mo)
        return 0.0;
    return players[displayplayer].mo->PrevX / (double)FRACUNIT;
}

double x_getPrevYPos(void) {
    if (gamestate != GS_LEVEL || !players[displayplayer].mo)
        return 0.0;
    return players[displayplayer].mo->PrevY / (double)FRACUNIT;
}

double x_getPrevZPos(void) {
    if (gamestate != GS_LEVEL || !players[displayplayer].mo)
        return 0.0;
    return players[displayplayer].mo->PrevZ / (double)FRACUNIT;
}

double x_getXMom(void) {
    if (gamestate != GS_LEVEL || !players[displayplayer].mo)
        return 0.0;
    return players[displayplayer].mo->momx / (double)FRACUNIT;
}

double x_getYMom(void) {
    if (gamestate != GS_LEVEL || !players[displayplayer].mo)
        return 0.0;
    return players[displayplayer].mo->momy / (double)FRACUNIT;
}

void x_setAngleType(int type) {
    switch (type) {
        case 0: angles = 16384.0f; break;
        case 1: angles = 2048.0f;  break;
        case 2: angles = 90.0f;    break;
        case 3: angles = 64.0f;    break;
        default: angles = 64.0f;   break;
    }
}

unsigned int x_getAngle(short type) {
    (void)type;
    if (gamestate != GS_LEVEL || !players[displayplayer].mo)
        return 0;

    return (unsigned int)(players[displayplayer].mo->angle * (angles / ANG90));
}

int x_GetCompatibility(void) {
    return local_compatflags;
}

void x_SetCompatibility(int compatflags) {
    local_compatflags = compatflags;
}

void x_getOptions(int options[]) {
    int i;

    options[0] = compatibility_level;
    options[1] = gameskill + 1;
    options[2] = gameepisode;
    options[3] = gamemap;
    options[4] = deathmatch;
    options[5] = respawnparm || clrespawnparm || dsda_Flag(dsda_arg_respawn) || (skill_info.respawn_time > 0);
    options[6] = fastparm || clfastparm || dsda_Flag(dsda_arg_fast) || (skill_info.flags & SI_FAST_MONSTERS);
    options[7] = nomonsters || clnomonsters || dsda_Flag(dsda_arg_nomonsters);
    options[8] = consoleplayer;
    options[9] = 0;

    for (i = 0; i < 4; i++) {
        options[9] += (playeringameGet(i) << i);
    }

    options[10] = monsters_remember;
    options[11] = variable_friction;
    options[12] = weapon_recoil;
    options[13] = allow_pushers;
    options[14] = player_bobbing;
    options[15] = demo_insurance;
    options[16] = rngseed;
    options[17] = monster_infighting;
    options[18] = dogs;
    options[19] = distfriend;
    options[20] = monster_backing;
    options[21] = monster_avoid_hazards;
    options[22] = monster_friction;
    options[23] = help_friends;
    options[24] = dog_jumping;
    options[25] = monkeys;
    options[26] = x_GetCompatibility();
    options[27] = x_longtics;
}

void x_changeOptions(int options[]) {
    dboolean plr_active[4];
    int lastplayer = 0;
    int i;

    compatibility_level = options[0];
    gameskill     = options[1] - 1;
    gameepisode   = options[2];
    gamemap       = options[3];
    deathmatch    = options[4];
    respawnparm   = options[5];
    fastparm      = options[6];
    nomonsters    = options[7];
    displayplayer = consoleplayer = options[8];

    G_XDRE_SetDemoOptions(nomonsters, respawnparm, fastparm);

    dboolean any_active = false;
    for (i = 0; i < 4; i++) {
        plr_active[i] = (options[9] & (1 << i)) ? true : false;
        if (plr_active[i]) {
            lastplayer = i;
            any_active = true;
        }
    }

    /* player 0 must be active if mask is empty */
    if (!any_active) {
        plr_active[0] = true;
        lastplayer = 0;
    }

    for (i = 0; i < 4; i++) {
        playeringameSet(i, plr_active[i]);
    }

    if (!plr_active[consoleplayer]) {
        displayplayer = consoleplayer = lastplayer;
    }

    monsters_remember     = options[10];
    variable_friction     = options[11];
    weapon_recoil         = options[12];
    allow_pushers         = options[13];
    player_bobbing        = options[14];
    demo_insurance        = options[15];
    rngseed               = (unsigned int)options[16];
    monster_infighting    = options[17];
    dogs                  = options[18];
    distfriend            = options[19];
    monster_backing       = options[20];
    monster_avoid_hazards = options[21];
    monster_friction      = options[22];
    help_friends          = options[23];
    dog_jumping           = options[24];
    monkeys               = options[25];
    x_SetCompatibility(options[26]);
    x_longtics            = options[27];
    usedHeader            = true;

    /* reinit compat table */
    G_Compatibility();
	
    /* re-seed prng if seed was set */
    if (compatibility_level >= boom_compatibility_compatibility) {
        M_ClearRandom();
    }
}

void x_initHeader(unsigned char* demo_p) {
    int i;

    /* gskill */
    if (gameskill < 0 || gameskill > 4) {
        gameskill = 3;
    }
	
	/* enforce ep/map with null protection */
    if (gameepisode <= 0) {
        gameepisode = 1;
    }
    if (gamemap <= 0) {
        gamemap = 1;
    }

    /* failed dog fix */
    dogs = 0;

    /* force shorttics unless specified otherwise in header editor */
        longtics = x_longtics;

    if (mbf_features) {
        unsigned char v = 0;
        switch(compatibility_level) {
            case mbf_compatibility:      v = 203; break;
            case prboom_2_compatibility: v = 210; break;
            case prboom_3_compatibility: v = 211; break;
            case prboom_4_compatibility: v = 212; break;
            case prboom_5_compatibility: v = 213; break;
            case prboom_6_compatibility: v = 214; break;
            case mbf21_compatibility:    v = 221; break;
            default:                     v = 221; break;
        }
        *demo_p++ = v;
        *demo_p++ = 0x1d;
        *demo_p++ = 'M';
        *demo_p++ = 'B';
        *demo_p++ = 'F';
        *demo_p++ = 0xe6;
        *demo_p++ = '\0';

        if (!mbf21) {
            *demo_p++ = 0;
        }

        *demo_p++ = gameskill;
        *demo_p++ = gameepisode;
        *demo_p++ = gamemap;
        *demo_p++ = deathmatch;
        *demo_p++ = consoleplayer;

        demo_p = G_WriteOptions(demo_p);

        for (i = 0; i < MAXPLAYERS; i++) {
            *demo_p++ = playeringame[i];
        }
        for (; i < 32; i++) {
            *demo_p++ = 0;
        }
    } else if (compatibility_level >= boom_compatibility_compatibility) {
        byte v = 202, c = 0;
        switch (compatibility_level) {
            case boom_compatibility_compatibility: v = 202; c = 1; break;
            case boom_201_compatibility:           v = 201; c = 0; break;
            case boom_202_compatibility:           v = 202; c = 0; break;
            default:                               v = 202; c = 0; break;
        }
        *demo_p++ = v;
        *demo_p++ = 0x1d;
        *demo_p++ = 'B';
        *demo_p++ = 'o';
        *demo_p++ = 'o';
        *demo_p++ = 'm';
        *demo_p++ = 0xe6;
        *demo_p++ = c;

        *demo_p++ = gameskill;
        *demo_p++ = gameepisode;
        *demo_p++ = gamemap;
        *demo_p++ = deathmatch;
        *demo_p++ = consoleplayer;

        demo_p = G_WriteOptions(demo_p);

        for (i = 0; i < MAXPLAYERS; i++) {
            *demo_p++ = playeringame[i];
        }
        for (; i < 32; i++) {
            *demo_p++ = 0;
        }
    } else {
        unsigned char v = 109;

        if (longtics) {
            v = 111;
        } else {
            switch (compatibility_level) {
                case doom_1666_compatibility: v = 106; break;
                case tasdoom_compatibility:   v = 110; break;
                default:                      v = 109; break;
            }
        }
        *demo_p++ = v;
        *demo_p++ = gameskill;
        *demo_p++ = gameepisode;
        *demo_p++ = gamemap;
        *demo_p++ = deathmatch;
        *demo_p++ = respawnparm;
        *demo_p++ = fastparm;
        *demo_p++ = nomonsters;
        *demo_p++ = consoleplayer;
                
        for (i = 0; i < 4; i++) {
            *demo_p++ = playeringame[i];
        }
    }
}

void x_clearMapSavepoints(void) {
    int i;
    for (i = 1; i < MAXSSP; ++i) {
        if (savepoints[i].save) {
            free(savepoints[i].save);
            savepoints[i].save = NULL;
        }
        savepoints[i].tic = 0;
        savepoints[i].size = 0;
        savepoints[i].capacity = 0;
    }
}

void x_clearUserSavepoint(void) {
    if (savepoints[0].save) {
        free(savepoints[0].save);
        savepoints[0].save = NULL;
    }
    savepoints[0].tic = 0;
    savepoints[0].size = 0;
    savepoints[0].capacity = 0;
}

void x_clearAllSavepoints(void) {
    x_clearUserSavepoint();
    x_clearMapSavepoints();
    save_p = savebuffer = NULL;
}

static int x_getSavepointIndex(int dest) {
    int index = -1;
    int savetic = 0;
    int map = gamemap;

    if (map > MAXSSP - 1)
        map = MAXSSP - 1;

    if (savepoints[0].save && savepoints[0].tic <= dest) {
        index = 0;
        savetic = savepoints[0].tic;
    }

    for (; map > 0; --map) {
        if (savetic <= savepoints[map].tic && savepoints[map].tic <= dest) {
            index = map;
            break;
        }
    }

    return index;
}

int x_getSavepointTic(int dest) {
    int index = x_getSavepointIndex(dest);
    if (index < 0)
        return 0;
    return savepoints[index].tic;
}

int x_setSavepoint(int isUserSave) {
    int map = gamemap;
    int i;
    int tic_diff;

    if (gamestate != GS_LEVEL || gameaction != ga_nothing) {
        save_p = savebuffer = NULL;
        return 1;
    }

    if (map < 0)
        map = 0;
    else if (map >= MAXSSP)
        map = MAXSSP - 1;

    /* user and auto at slot 0 */
    if (leveltime != 1 || isUserSave != 0)
        map = 0;

    if (!savepoints[map].save) {
        savepoints[map].save = (byte*)malloc(XDRE_SAVEGAMESIZE);
        if (!savepoints[map].save)
            return 1;
        savepoints[map].capacity = XDRE_SAVEGAMESIZE;
    }

    savepoints[map].tic = gametic;

    /* pass full buffer to avoid overloading CheckSaveGame */
    P_SetSaveBuffer(savepoints[map].save, (int)savepoints[map].capacity);

    *save_p++ = (byte)compatibility_level;
    *save_p++ = (byte)gameskill;
    *save_p++ = (byte)gameepisode;
    *save_p++ = (byte)gamemap;

    for (i = 0; i < 4; ++i) {
        *save_p++ = (byte)playeringame[i];
    }

    *save_p++ = (idmusnum > 0 && idmusnum < 255) ? (byte)idmusnum : 0;
    save_p = G_WriteOptions(save_p);

    memcpy(save_p, &leveltime, sizeof(leveltime));
    save_p += sizeof(leveltime);

    memcpy(save_p, &totalleveltimes, sizeof(totalleveltimes));
    save_p += sizeof(totalleveltimes);

    tic_diff = gametic - basetic;
    memcpy(save_p, &tic_diff, sizeof(tic_diff));
    save_p += sizeof(tic_diff);

    /* dsda_archiveall ripoff */
    P_ArchiveACS();
    P_ArchivePlayers();
    P_ThinkerToIndex();
    P_ArchiveWorld();
    P_ArchivePolyobjs();
    P_ArchiveThinkers();
    P_ArchiveScripts();
    P_ArchiveSounds();
    P_ArchiveAmbientSound();
    P_ArchiveMisc();
    P_IndexToThinker();
    P_ArchiveRNG();
    P_ArchiveMap();

    *save_p++ = 0xe6;

    /* update pointer if realloc was called */
    savepoints[map].save = savebuffer;
    savepoints[map].size = (size_t)(save_p - savebuffer);
    if (savepoints[map].size > savepoints[map].capacity) {
        savepoints[map].capacity = savepoints[map].size;
    }

    save_p = savebuffer = NULL;
    return 0;
}

int x_loadSavepoint(int dest) {
    int i;
    int map;
    byte *src;
    int saved_idmus;
    int tic_diff;
    dboolean saved_playeringame[MAX_MAXPLAYERS];

    map = x_getSavepointIndex(dest);
    if (map < 0 || map >= MAXSSP || !savepoints[map].save) {
        save_p = savebuffer = NULL;
        return 1;
    }

    /* read params from savebuffer header */
    src = savepoints[map].save;

    gametic = savepoints[map].tic;
    gameaction = ga_nothing;

    compatibility_level = *src++;
    gameskill   = *src++;
    gameepisode = *src++;
    gamemap     = *src++;

    /* read 4 players in game */
    for (i = 0; i < 4; ++i) {
        saved_playeringame[i] = *src++;
    }
    for (i = 4; i < g_maxplayers; ++i) {
        saved_playeringame[i] = 0;
    }

    saved_idmus = *src++;
    if (saved_idmus == 0 || saved_idmus == 255)
        saved_idmus = -1;

    src = (byte *)G_ReadOptions(src);
    G_XDRE_SetDemoOptions(nomonsters, respawnparm, fastparm);

    /* init level state */
    idmusnum = -1;
    G_InitNew(gameskill, gameepisode, gamemap, false);
	
	/* enforce ldre longtics */
    if (usedHeader) {
        longtics = x_longtics;
    }

     /* g_initnew resets playeringame to 0. we must restore
     * the active player mask before calling p_unarchiveplayers or
     * we'll get savepoint desync across players */
    for (i = 0; i < g_maxplayers; ++i) {
        playeringame[i] = saved_playeringame[i];
    }

    /* neutralize screen wipe */
    wipegamestate = GS_LEVEL;

    /* restore music */
    if (saved_idmus != -1) {
        idmusnum = saved_idmus;
        S_ChangeMusic(idmusnum, 1);
    }

    /* attach buffer w size */
    P_SetSaveBuffer(savepoints[map].save, savepoints[map].size);
    save_p = src;

    memcpy(&leveltime, save_p, sizeof(leveltime));
    save_p += sizeof(leveltime);

    memcpy(&totalleveltimes, save_p, sizeof(totalleveltimes));
    save_p += sizeof(totalleveltimes);

    memcpy(&tic_diff, save_p, sizeof(tic_diff));
    save_p += sizeof(tic_diff);
    basetic = gametic - tic_diff;

    gamestate = GS_LEVEL;

    /* dsda_archiveall ripoff */
    P_MapStart();
    P_UnArchiveACS();
    P_UnArchivePlayers();
    P_UnArchiveWorld();
    P_UnArchivePolyobjs();
    P_UnArchiveThinkers();
    P_UnArchiveScripts();
    P_UnArchiveSounds();
    P_UnArchiveAmbientSound();
    P_UnArchiveMisc();
    P_UnArchiveRNG();
    P_UnArchiveMap();
    P_MapEnd();

    /* clear pending commands and zero out excmd_t to not
     * get memory corruption */
    for (i = 0; i < 4; ++i) {
        memset(&netcmds[i], 0, sizeof(ticcmd_t));
        if (playeringame[i]) {
            memset(&players[i].cmd, 0, sizeof(ticcmd_t));
        }
    }

    R_ActivateSectorInterpolations();
    R_SmoothPlaying_Reset(NULL);

    RecalculateDrawnSubsectors();

    if (*save_p != 0xe6) {
        I_Error("x_loadSavepoint: Corrupt savepoint buffer on map %d (found 0x%02x, expected 0xe6)", map, *save_p);
    }

    if (setsizeneeded)
        R_ExecuteSetViewSize();

    R_FillBackScreen();
    save_p = savebuffer = NULL;
    return 0;
}

void x_initLongticsDefault(void) {
    x_longtics = dsda_Flag(dsda_arg_longtics);
    longtics = x_longtics;
    usedHeader = true;
}