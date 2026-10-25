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

#ifndef MAXPLAYERS
#define MAXPLAYERS 4
#endif

#define XDRE_SAVEGAMESIZE (16 * 1024 * 1024)

// extern void dsda_SetCompatibilityLevel(int complevel);
extern void G_XDRE_SetDemoOptions(int no_monsters, int respawn, int fast);
extern void RecalculateDrawnSubsectors(void);
extern int totalleveltimes;
extern dboolean setsizeneeded;
extern byte* savebuffer;
extern size_t save_size;
extern void P_SetSaveBuffer(byte *buf, int size);

extern int basetic;

extern int numsectors; 
extern sector_t* sectors; 
int secID = -1;
int xSectorInfo[3];

extern thinker_t thinkercap;
void P_MobjThinker(mobj_t *mobj);
int thingID = -1;
int xThingInfo[8];

extern int numlines;
extern line_t* lines;
int xLineId = -1;
int xLineCrossed = 0;
int xLineVertexes[2][2];

float angles = 16384.0f;

typedef struct {
    int tic;
    byte* save;
} xSavepoint_t;

#define MAXSSP 100

static xSavepoint_t savepoints[MAXSSP];

dboolean x_longtics = 0;
dboolean usedHeader = false;

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
    int i;

    if (linedef == -1)
        linedef = xLineId;

    xLineId = -1;
    for (i = 0; i < numlines; ++i) {
        if (linedef == i) {
            xLineId = i;
            xLineVertexes[0][0] = lines[i].v1->x;
            xLineVertexes[0][1] = lines[i].v1->y;
            xLineVertexes[1][0] = lines[i].v2->x;
            xLineVertexes[1][1] = lines[i].v2->y;
            break;
        }
    }

    if (xLineId == -1) {
        xLineVertexes[0][0] = 0;
        xLineVertexes[0][1] = 0;
        xLineVertexes[1][0] = 0;
        xLineVertexes[1][1] = 0;
    }
}

void x_setSectorCheck(int sector) {
    int i;

    if (sector == -1)
        sector = secID;

    secID = -1;
    for (i = 0; i < numsectors; ++i) {
        if (sector == i) {
            secID = i;
            xSectorInfo[0] = sectors[i].floorheight;
            xSectorInfo[1] = sectors[i].ceilingheight;
            xSectorInfo[2] = sectors[i].special;
            break;
        }
    }

    if (secID == -1) {
        for (i = 0; i < 3; ++i) {
            xSectorInfo[i] = 0;
        }
    }
}

void x_setThingCheck(int index) {
    int i = 0;
    thinker_t *th;

    if (index == -1)
        index = thingID;

    thingID = -1;
    for (i = 0; i < 8; ++i) {
        xThingInfo[i] = 0;
    }

    if (index < 0 || gamestate != GS_LEVEL)
        return;

    i = 0;
    for (th = thinkercap.next; th != &thinkercap; th = th->next) {
        if ((void*)th->function == (void*)P_MobjThinker) {
            if (i == index) {
                mobj_t *mo = (mobj_t *)th;
                thingID = index;
                xThingInfo[0] = mo->x;
                xThingInfo[1] = mo->y;
                xThingInfo[2] = mo->z;
                xThingInfo[3] = mo->radius;
                xThingInfo[4] = mo->tics;
                xThingInfo[5] = mo->health;
                xThingInfo[6] = mo->reactiontime;
                xThingInfo[7] = mo->threshold;
                return;
            }
            ++i;
        }
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
    options[5] = respawnparm;
    options[6] = fastparm;
    options[7] = nomonsters;
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
    options[27] = longtics;
}

void x_changeOptions(int options[]) {
    dboolean plr_active[4];
    int lastplayer = 0;
    int i;

    compatibility_level = options[0];
    gameskill     = options[1] - 1; /* 1-5 converted to internal 0-4 */
    gameepisode   = options[2];
    gamemap       = options[3];
    deathmatch    = options[4];
    respawnparm   = options[5];
    fastparm      = options[6];
    nomonsters    = options[7];
    displayplayer = consoleplayer = options[8];

    /* update baseline arguments, lowkey broken right now */
    G_XDRE_SetDemoOptions(nomonsters, respawnparm, fastparm);

    for (i = 0; i < 4; i++) {
        plr_active[i] = (options[9] & (1 << i)) ? true : false;
        playeringameSet(i, plr_active[i]);
        if (plr_active[i])
            lastplayer = i;
    }

    for (i = 0; i < 4; i++) {
        if (!plr_active[i] && consoleplayer == i)
            displayplayer = consoleplayer = lastplayer;
    }

    monsters_remember     = options[10];
    variable_friction     = options[11];
    weapon_recoil         = options[12];
    allow_pushers         = options[13];
    player_bobbing        = options[14];
    demo_insurance        = options[15];
    rngseed               = options[16];
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
}

void x_initHeader(unsigned char* demo_p) {
    int i;

    /* gskill */
    if (gameskill < 0 || gameskill > 4) {
        gameskill = 3;
    }

    /* failed dog fix */
    dogs = 0;

    if (mbf_features) {
        unsigned char v = 0;
        switch(compatibility_level) {
            case mbf_compatibility:      v = 203; break;
            case prboom_2_compatibility: v = 210; break;
            case prboom_3_compatibility: v = 211; break;
            case prboom_4_compatibility: v = 212; break;
            case prboom_5_compatibility: v = 213; break;
            case prboom_6_compatibility: v = 214; longtics = 1; break;
            case mbf21_compatibility:    v = 221; longtics = 1; break;
            default:                     v = 221; longtics = 1; break;
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
        if (!usedHeader) {
            longtics = dsda_Flag(dsda_arg_longtics);
        } else {
            longtics = x_longtics;
        }

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
    }
}

void x_clearUserSavepoint(void) {
    if (savepoints[0].save) {
        free(savepoints[0].save);
        savepoints[0].save = NULL;
    }
    savepoints[0].tic = 0;
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

    if (map < 0 || map > MAXSSP - 1 || gamestate != GS_LEVEL || gameaction != ga_nothing)
        return 1;

    if (leveltime != 1 || isUserSave != 0)
        map = 0;

    if (savepoints[map].save) {
        free(savepoints[map].save);
        savepoints[map].save = NULL;
    }

    savepoints[map].save = (byte*)malloc(XDRE_SAVEGAMESIZE);
    savepoints[map].tic = gametic;

    /* init save buffer */
    P_SetSaveBuffer(savepoints[map].save, XDRE_SAVEGAMESIZE);

    *save_p++ = (byte)compatibility_level;
    *save_p++ = (byte)gameskill;
    *save_p++ = (byte)gameepisode;
    *save_p++ = (byte)gamemap;

    /* 4 bytes savepoint */
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

    P_ThinkerToIndex();
    P_ArchiveRNG();
    P_ArchivePlayers();
    P_ArchiveWorld();
    P_ArchiveThinkers();
    P_ArchiveScripts();
    P_IndexToThinker();
    P_ArchiveMap();

    *save_p++ = 0xe6;

    /* savebuffer could have been reallocated if it exceeded XDRE_SAVEGAMESIZE */
    savepoints[map].save = savebuffer;
    save_p = savebuffer = NULL;
    return 0;
}

int x_loadSavepoint(int dest) {
    int i;
    int map;
    byte *src;
    int saved_idmus;
    int tic_diff;

    map = x_getSavepointIndex(dest);

    if (map < 0 || !savepoints[map].save)
        return 1;

    if (map != 0)
        x_clearUserSavepoint();

    /* read params with local pointer first */
    src = savepoints[map].save;

    gametic = savepoints[map].tic;
    gameaction = ga_nothing;

    compatibility_level = *src++;
    gameskill   = *src++;
    gameepisode = *src++;
    gamemap     = *src++;

    /* 4 bytes savepoint */
    for (i = 0; i < 4; ++i) {
        playeringame[i] = *src++;
    }
    for (i = 4; i < g_maxplayers; ++i) {
        playeringame[i] = 0;
    }

    saved_idmus = *src++;
    if (saved_idmus == 0 || saved_idmus == 255)
        saved_idmus = -1;

    src = G_ReadOptions(src);
    G_XDRE_SetDemoOptions(nomonsters, respawnparm, fastparm);

    /* reset music and load level w g_initnew */
    idmusnum = -1;
    G_InitNew(gameskill, gameepisode, gamemap, false);

    /* restore music */
    if (saved_idmus != -1) {
        idmusnum = saved_idmus;
        S_ChangeMusic(idmusnum, 1);
    }

    /* attach buffer */
    P_SetSaveBuffer(savepoints[map].save, XDRE_SAVEGAMESIZE);
    save_p = src;

    memcpy(&leveltime, save_p, sizeof(leveltime));
    save_p += sizeof(leveltime);

    memcpy(&totalleveltimes, save_p, sizeof(totalleveltimes));
    save_p += sizeof(totalleveltimes);

    memcpy(&tic_diff, save_p, sizeof(tic_diff));
    save_p += sizeof(tic_diff);
    basetic = gametic - tic_diff;

    gamestate = GS_LEVEL;

    P_UnArchiveRNG();
    P_UnArchivePlayers();
    P_MapStart();
    P_UnArchiveWorld();
    P_UnArchiveThinkers();
    P_UnArchiveScripts();
    P_UnArchiveMap();
    P_MapEnd();

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