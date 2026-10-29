/*
 * This file is part of XDRE.
 *
 * Description: controls the program.
*/

#include <vector>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <sstream>
#include "xdre.hpp"

extern "C" {
    #include "doomtype.h"
    #include "doomdata.h"
    #include "d_event.h"
    #include "doomstat.h"
    #include "d_ticcmd.h"
    #include "cstuff.h"
	#include "dsda/configuration.h"
	#include "hu_stuff.h"
    #include "st_stuff.h"

    void mainDoom(int argc, char** argv);
    void D_Display(void);
    void G_InitNew(int skill, int episode, int map, dboolean prepare);
    unsigned char const* XDRE_ReadDemoHeader(unsigned char const* demo_p, std::size_t size);
    void XDRE_WriteDemoTiccmd(ticcmd_t const* cmd, FILE* file);
    void G_Ticker(void);
    void I_StartTic(void);
    void M_Ticker(void);
    void ST_Start(void);
    void HU_Start(void);
    void I_FinishUpdate(void);
    void XDRE_WriteDemoFooter(FILE *file);

    extern ticcmd_t netcmds[4];
    extern gamestate_t gamestate;
    extern int leveltime;
    extern int consoleplayer;
    extern int compatibility_level;
    extern int longtics;
    extern int displayplayer;
    extern int gametic;
    extern int basetic;
    extern int palette_ondamage;
    extern int palette_onbonus;
    extern int palette_onpowers;

    extern int thingID;
    extern int xThingInfo[8];

    extern int xSectorInfo[3];
    extern int secID;

    extern int xUseSuccess;
    extern int xDoneDamage;
    extern int xLineId;
    extern int xLineCrossed;
    extern int xLineVertexes[2][2];
	
	extern int nomonsters;
    extern int respawnparm;
    extern int fastparm;
	
	void G_XDRE_SetDemoOptions(int no_monsters, int respawn, int fast);
	int dsda_GameOptionSize(void);
	extern dboolean BorderNeedRefresh;
} /* extern "C" */

/*
 * xdre private stuff
 */
constexpr double X_PI = 3.14159265358979323846;

std::vector<unsigned char> header;
std::vector<ticcmd_t> tics[4];
std::vector<ticcmd_t>::iterator nextTic[4];
int savepointStyle;

extern "C" void x_shutdown(void) {
    header.clear();
    header.shrink_to_fit();
    for (int p = 0; p < 4; ++p) {
        tics[p].clear();
        tics[p].shrink_to_fit();
        nextTic[p] = std::end(tics[p]);
    }
}

void display();
bool loadReplay(unsigned char const* demo_p, std::size_t size);
unsigned char const* readTics(unsigned char const* start_p, std::size_t size);
void makeCmds();
void goForward(int numTics);
void goBack(int numTics);

static void invalidateSavepointsAfter(int tic) {
    /* if savepoint is ahead of current section, clear it */
    if (x_getSavepointTic(tic) >= tic) {
        x_clearUserSavepoint();
    }
}

/* tic padding for multiplayer */
static void syncActivePlayerTics() {
    std::size_t maxTics = 0;
    for (int p = 0; p < 4; ++p) {
        if (playeringameGet(p)) {
            maxTics = std::max(maxTics, tics[p].size());
        }
    }

    for (int p = 0; p < 4; ++p) {
        if (!playeringameGet(p)) {
            continue;
        }

        auto currentOffset = std::distance(std::begin(tics[p]), nextTic[p]);

        if (tics[p].size() < maxTics) {
            tics[p].resize(maxTics, ticcmd_t{});
        }

        if (currentOffset <= static_cast<std::ptrdiff_t>(tics[p].size())) {
            nextTic[p] = std::begin(tics[p]) + currentOffset;
        } else {
            nextTic[p] = std::end(tics[p]);
        }
    }
}

static ticcmd_t* getActiveTic() {
    auto& playerTics = tics[displayplayer];
    if (playerTics.empty() || nextTic[displayplayer] == playerTics.begin()) {
        return nullptr;
    }
    return &(*std::prev(nextTic[displayplayer]));
}

void display() {
    if (gamestate == GS_LEVEL) {
        if (players[displayplayer].mo != NULL) {
            /* recalc hud displays for current player */
            ST_Ticker();
            HU_Ticker();
            BorderNeedRefresh = true;
            D_Display();
        }
    } else if (gamestate == GS_INTERMISSION || gamestate == GS_FINALE) {
        D_Display();
    }
}

static void rewindDemoToStart() {
    gametic = basetic = 0;
    playeringame[0] = true;
    if (!header.empty()) {
        XDRE_ReadDemoHeader(header.data(), header.size());
    }
    playeringame[0] = true;

    for (int p = 0; p < 4; ++p) {
        if (playeringameGet(p) && !tics[p].empty()) {
            nextTic[p] = std::begin(tics[p]);
        } else {
            nextTic[p] = std::end(tics[p]);
        }
    }
}

bool loadReplay(unsigned char const* demo_p, std::size_t size) {
    unsigned char const* tics_p = XDRE_ReadDemoHeader(demo_p, size);
    if (!tics_p) {
        return false;
    }

    /* demo opts from first read */
    G_XDRE_SetDemoOptions(nomonsters, respawnparm, fastparm);

    std::size_t const newHeaderSize = static_cast<std::size_t>(tics_p - demo_p);
    header.assign(demo_p, demo_p + newHeaderSize);

    x_clearMapSavepoints();
    x_clearUserSavepoint();
    for (int p = 0; p < 4; ++p) {
        tics[p].clear();
		tics[p].shrink_to_fit();
    }

    readTics(tics_p, size - (tics_p - demo_p));

    /* reset engine from goback */
    rewindDemoToStart();

    displayplayer = consoleplayer;
	
	/* display fix */
	goForward(1);
	display();

    return true;
}

unsigned char const* readTics(unsigned char const* start_p, std::size_t size) {
    unsigned char const* tic_p = start_p;
    unsigned char const* end_p = start_p + size;
    int plr = 0;

    for (int i = 0; i < 4; ++i) {
        if (playeringameGet(i)) {
            plr = i;
            break;
        }
    }

    std::size_t const ticStride = longtics ? 5 : 4;

    while (tic_p < end_p && *tic_p != 0x80) {
        if (static_cast<std::size_t>(end_p - tic_p) < ticStride) {
            break; // Truncated tic data
        }

        ticcmd_t cmd = {};
        unsigned char at = 0;
        cmd.forwardmove = static_cast<signed char>(*tic_p++);
        cmd.sidemove = static_cast<signed char>(*tic_p++);
        if (!longtics) {
            cmd.angleturn = (at = *tic_p++) << 8;
        } else {
            unsigned int lowbyte = *tic_p++;
            cmd.angleturn = ((static_cast<signed int>(*tic_p++)) << 8) + lowbyte;
        }
        cmd.buttons = *tic_p++;

        if (compatibility_level == tasdoom_compatibility) {
            signed char tmp = cmd.forwardmove;
            cmd.forwardmove = cmd.sidemove;
            cmd.sidemove = static_cast<signed char>(at);
            cmd.angleturn = cmd.buttons << 8;
            cmd.buttons = tmp;
        }

        tics[plr++].push_back(cmd);

        int checkCount = 0;
        while (!playeringameGet(plr)) {
            if (plr >= 3) {
                plr = 0;
            } else {
                ++plr;
            }
            if (++checkCount > 4) {
                plr = 0;
                break;
            }
        }
    }

    return (tic_p < end_p && *tic_p == 0x80) ? tic_p : nullptr;
}

void makeCmds() {
    for (int p = 0; p < 4; ++p) {
        if (!playeringameGet(p)) {
            continue;
        }
        if (nextTic[p] != std::end(tics[p])) {
            netcmds[p] = *nextTic[p]++;
        } else {
            netcmds[p] = {};
        }
    }
}

static void safeAdvanceTic(int p, std::ptrdiff_t diff) {
    if (!playeringameGet(p) || tics[p].empty()) {
        return;
    }
    if (diff < 0) {
        auto distFromBegin = std::distance(tics[p].begin(), nextTic[p]);
        if (-diff >= distFromBegin) {
            nextTic[p] = tics[p].begin();
            return;
        }
    } else if (diff > 0) {
        auto distFromEnd = std::distance(nextTic[p], tics[p].end());
        if (diff >= distFromEnd) {
            nextTic[p] = tics[p].end();
            return;
        }
    }
    std::advance(nextTic[p], diff);
}

void goForward(int numTics) {
    if (numTics < 1 || (gamestate != GS_LEVEL && gamestate != GS_INTERMISSION && gamestate != GS_FINALE)) {
        return;
    }

    auto endCmd = std::end(tics[displayplayer]);
    auto dest = numTics + gametic;
    auto savepoint = x_getSavepointTic(gametic);
    auto doSaveTic = -1;
    if (dest - savepoint > 140) {
        doSaveTic = dest - 70;
        if (doSaveTic < gametic) {
            doSaveTic = gametic;
        }
    }

    while (numTics-- > 0 && nextTic[displayplayer] != endCmd) {
        xUseSuccess = 0;
        xLineCrossed = 0;
        xDoneDamage = 0;

        /* lock savepoints to only happen inside level state */
        if (gamestate == GS_LEVEL) {
            if (savepointStyle == SpStyle::SP_AUTO
                    && gametic > 1
                    && savepoint != gametic
                    && (leveltime == 1
                        || doSaveTic == gametic)) {
                x_setSavepoint(0);
            } else if (savepointStyle == SpStyle::SP_START
                    && gametic > 1
                    && savepoint != gametic
                    && leveltime == 1) {
                x_setSavepoint(0);
            }
        }

        makeCmds();

        G_Ticker();
        ++gametic;
    }

    if (gamestate == GS_LEVEL) {
        xdre::setLinedefCheck(-1);
        xdre::setSectorCheck(-1);
        xdre::setThingCheck(-1);
    }
}

void goBack(int numTics) {
    if (numTics < 0) {
        return;
    }

    int const savedDisplayPlayer = displayplayer;

    auto destTic = gametic - numTics;
    /* block time: 0.00 leak */
    if (destTic < 1) {
        destTic = 1;
    }

    /* enforce savepoint boundary */
    if (savepointStyle == SpStyle::SP_USER) {
        auto temp = x_getSavepointTic(gametic);
        if (temp > 0) {
            destTic = destTic <= temp ? temp + 1 : destTic;
        }
    }

    /* clear netcmds */
    for (int p = 0; p < 4; ++p) {
        netcmds[p] = {};
    }

    if (!x_loadSavepoint(destTic)) {
        for (int p = 0; p < 4; ++p) {
            if (!playeringameGet(p) || tics[p].empty()) continue;
            std::size_t targetIdx = std::min<std::size_t>(gametic, tics[p].size());
            nextTic[p] = tics[p].begin() + targetIdx;
        }

        if (destTic > gametic) {
            goForward(destTic - gametic);
        }
    } else {
        x_clearMapSavepoints();
        x_clearUserSavepoint();
        rewindDemoToStart();

        /* start to desttic */
        if (destTic > 0) {
            goForward(destTic);
        }
    }

    /* clear netcmd again */
    for (int p = 0; p < 4; ++p) {
        netcmds[p] = {};
    }

    if (savedDisplayPlayer >= 0 && savedDisplayPlayer < 4 && playeringameGet(savedDisplayPlayer)) {
        displayplayer = savedDisplayPlayer;
        if (gamestate == GS_LEVEL) {
            ST_Start();
            HU_Start();
        }
    }
}

bool bruteCheckDone(BruteCheck const& check) {
    if (!check.rngFunc(x_getRngIndex()))  return false;
    if (!check.damageFunc(xDoneDamage))   return false;
    if (!check.xPosFunc(xdre::getXPos())) return false;
    if (!check.yPosFunc(xdre::getYPos())) return false;
    if (!check.zPosFunc(xdre::getZPos())) return false;
    if (!check.xMomFunc(xdre::getXMom())) return false;
    if (!check.yMomFunc(xdre::getYMom())) return false;
    if (!check.speedFunc(xdre::getDistanceMoved())) return false;
    if (check.use && !xdre::getUseSuccess()) return false;

    return true;
}

bool bruteForceTics(std::vector<BruteTic>::const_iterator brute_it,
                    std::vector<BruteTic>::const_iterator bruteEnd_it,
                    BruteCheck const& bruteCheck,
                    bool const& abort) {
    if (brute_it == bruteEnd_it) {
        return false;
    }

    auto bruteForward = [](int numTics) {
        if (numTics < 1) {
            return;
        }

        while (numTics-- > 0) {
            xUseSuccess = 0;
            xLineCrossed = 0;
            xDoneDamage = 0;

            makeCmds();
			
            G_Ticker();
            ++gametic;
        }
    };

    int gameticOnEnter = gametic;
    auto bruteLoad = [gameticOnEnter, bruteForward]() {
        if (gametic == gameticOnEnter) {
            return;
        }
        int oldTic = gametic;
        if (!x_loadSavepoint(gametic + 1)) {
            for (int p = 0; p < 4; ++p) {
                safeAdvanceTic(p, gametic - oldTic);
            }
            bruteForward(gameticOnEnter - gametic);
        } else {
            rewindDemoToStart();
        }
    };

    auto const& bruteTic = *brute_it;

    for (auto turn : bruteTic.turnCmds) {
        if (bruteTic.turnsAreAngles) {
            if (!longtics) {
                nextTic[displayplayer]->angleturn = (xdre::getAngle(2) + turn - 128) << 8;
            } else {
                nextTic[displayplayer]->angleturn = (xdre::getAngle(1) + turn);
            }
        } else {
            if (!longtics) {
                nextTic[displayplayer]->angleturn = turn << 8;
            } else {
                nextTic[displayplayer]->angleturn = turn;
            }
        }

        for (auto run : bruteTic.runCmds) {
            nextTic[displayplayer]->forwardmove = run;

            for (auto strafe : bruteTic.strafeCmds) {
                nextTic[displayplayer]->sidemove = strafe;

                nextTic[displayplayer]->buttons &= ~(BT_ATTACK|BT_USE);

                bool fire[2];
                int fireLen;
                if (bruteTic.fire < 0) {
                    fireLen = 1;
                    fire[0] = false;
                } else if (bruteTic.fire > 0) {
                    fireLen = 1;
                    fire[0] = true;
                } else {
                    fireLen = 2;
                    fire[0] = false;
                    fire[1] = true;
                }

                for (int f = 0; f < fireLen; ++f) {
                    if (fire[f]) {
                        nextTic[displayplayer]->buttons |= BT_ATTACK;
                    } else {
                        nextTic[displayplayer]->buttons &= ~BT_ATTACK;
                    }

                    bool use[2];
                    int useLen;
                    if (bruteTic.use < 0) {
                        useLen = 1;
                        use[0] = false;
                    } else if (bruteTic.use > 0) {
                        useLen = 1;
                        use[0] = true;
                    } else {
                        useLen = 2;
                        use[0] = false;
                        use[1] = true;
                    }

                    for (int u = 0; u < useLen; ++u) {
                        if (use[u]) {
                            nextTic[displayplayer]->buttons |= BT_USE;
                        } else {
                            nextTic[displayplayer]->buttons &= ~BT_USE;
                        }

                        auto nextBrute_it = std::next(brute_it);
                        if (nextBrute_it != bruteEnd_it && nextBrute_it->tic <= bruteCheck.tic) {
                            bruteForward(nextBrute_it->tic - gametic);
                            if (bruteForceTics(nextBrute_it, bruteEnd_it, bruteCheck, abort)) {
                                return true;
                            } else if (abort) {
                                return false;
                            }
                        } else {
                            bruteForward(bruteCheck.tic - gametic + 1);
                            if (bruteCheckDone(bruteCheck)) {
                                return true;
                            } else if (abort) {
                                return false;
                            }
                        }
                        bruteLoad();
                    }
                }
            }
        }
    }

    return false;
}

int getHeaderSize(void) {
    if (compatibility_level < boom_compatibility_compatibility) {
        return 13;
    }
    if (compatibility_level == mbf21_compatibility) {
        /* MBF21: 1 (ver) + 6 (sig) + 5 (params) + dsda_GameOptionSize() + 32 (players) = 90 */
        return 1 + 6 + 5 + dsda_GameOptionSize() + 32;
    }
    /* Boom / MBF: 1 (ver) + 6 (sig) + 1 (boom compat) + 5 (params) + dsda_GameOptionSize() + 32 (players) = 109 */
    return 1 + 6 + 1 + 5 + dsda_GameOptionSize() + 32;
}

void xdre::init(int argc, char** argv) {
    static bool done;
    if (done) {
        return;
    }

    mainDoom(argc, argv);

    if (gameskill <= 0 || gameskill > 4) {
        gameskill = 3;
    }

    playeringame[0] = true;
    displayplayer = consoleplayer;

    for (int p = 0; p < 4; ++p) {
        tics[p].resize(10, {});
    }
    nextTic[0] = std::begin(tics[0]);
    for (int p = 1; p < 4; ++p) {
        tics[p].clear();
        nextTic[p] = std::end(tics[p]);
    }

    header.resize(getHeaderSize());
    x_initHeader(header.data());

    palette_onbonus = 0;
    palette_ondamage = 0;
    palette_onpowers = 0;

    dsda_UpdateIntConfig(dsda_config_palette_ondamage, 0, false);
    dsda_UpdateIntConfig(dsda_config_palette_onbonus,  0, false);
    dsda_UpdateIntConfig(dsda_config_palette_onpowers, 0, false);

    savepointStyle = SpStyle::SP_START;

    if (gamestate == GS_LEVEL) {
        x_setSavepoint(0);
        goForward(1);
        display();
    }
    done = true;
}

bool xdre::bruteForce(std::vector<BruteTic> bruteTics,
                      BruteCheck const& bruteCheck,
                      bool const& abort) {
    if (bruteTics.empty()) {
        return false;
    }

    std::sort(std::begin(bruteTics), std::end(bruteTics), [](BruteTic const& lhs, BruteTic const& rhs){return lhs.tic < rhs.tic;});
    int bruteStart = bruteTics.front().tic;
    int bruteEnd = bruteTics.back().tic;
    if (bruteStart < 0 || tics[displayplayer].empty() || static_cast<long long>(tics[displayplayer].size() - 1) < bruteEnd || static_cast<int>(bruteCheck.tic) < bruteStart) {
        return false;
    }

    x_clearMapSavepoints();
    x_clearUserSavepoint();

    rewindDemoToStart();

    int destSavepoint = bruteStart;
    while (destSavepoint-- > 0) {
        makeCmds();
		
        G_Ticker();
        ++gametic;
    }

    if (gametic > 0) {
        if (gamestate == GS_LEVEL) {
            x_setSavepoint(1);
        } else {
            return false;
        }
    }

    xUseSuccess = 0;
    xLineCrossed = 0;
    xDoneDamage = 0;

    if (bruteForceTics(bruteTics.cbegin(), bruteTics.cend(), bruteCheck, abort)) {
        goBack(0);
        display();
        return true;
    }

    if (abort) {
        x_clearMapSavepoints();
        x_clearUserSavepoint();
        goBack(0);
        goForward(1);
    } else {
        goForward(1);
        goBack(0);
    }

    display();
    return false;
}

void xdre::doSdlEvents() {
    I_StartTic();
    I_FinishUpdate();
}

bool xdre::save(std::FILE* file) {
    if (!file) return false;
    rewind(file);

    /* call tic sync */
    syncActivePlayerTics();

    if (!header.empty()) {
        fwrite(header.data(), sizeof(header[0]), header.size(), file);
    }

    std::size_t totalTics = 0;
    for (int p = 0; p < 4; ++p) {
        if (playeringameGet(p)) {
            totalTics = std::max(totalTics, tics[p].size());
        }
    }

    ticcmd_t nulltic {};

    /* write tics in round-robin order */
    for (std::size_t t = 0; t < totalTics; ++t) {
        for (int p = 0; p < 4; ++p) {
            if (playeringameGet(p)) {
                if (t < tics[p].size()) {
                    XDRE_WriteDemoTiccmd(&tics[p][t], file);
                } else {
                    XDRE_WriteDemoTiccmd(&nulltic, file);
                }
            }
        }
    }

    fputc(0x80, file); /* DEMOMARKER */

    XDRE_WriteDemoFooter(file);

    return true;
}

bool xdre::load(std::FILE* file) {
    if (!file) {
        return false;
    }
    std::fseek(file, 0, SEEK_END);
    long size = std::ftell(file);
    if (size <= 0) {
        return false;
    }
    rewind(file);

    std::vector<unsigned char> data(static_cast<std::size_t>(size));
    std::size_t readBytes = std::fread(data.data(), 1, data.size(), file);
    if (readBytes != data.size()) {
        return false;
    }

    return loadReplay(data.data(), data.size());
}

void xdre::setSavepoint() {
    if (!x_setSavepoint(1)) {
        goForward(1);
        display();
    }
}

void xdre::unsetSavepoint() {
    x_clearUserSavepoint();
    if (savepointStyle == SpStyle::SP_AUTO) {
        savepointStyle = SpStyle::SP_START;
    }
    goBack(0);
    display();
}

void xdre::seekDemo(int numTics) {
    if (numTics < 0) {
        goBack(-numTics);
    } else {
        goForward(numTics);
    }
    display();
}

void xdre::copyTic() {
    if (tics[displayplayer].empty() || nextTic[displayplayer] == std::begin(tics[displayplayer])) {
        return;
    }

    /* snapshot the index of insertion */
    auto const insertIdx = static_cast<std::size_t>(std::distance(std::begin(tics[displayplayer]), nextTic[displayplayer]));
    
    /* local copy */
    ticcmd_t const cmdToCopy = *std::prev(nextTic[displayplayer]);

    /* clean WT and wait for others */
    ticcmd_t cleanWaitTic;
    std::memset(&cleanWaitTic, 0, sizeof(cleanWaitTic));

    /* insert into every active player at same index */
    for (int p = 0; p < 4; ++p) {
        if (!playeringameGet(p)) continue;

        auto otherOffset = static_cast<std::size_t>(std::distance(std::begin(tics[p]), nextTic[p]));
        auto safeInsertIdx = std::min(insertIdx, tics[p].size());

        if (p == displayplayer) {
            nextTic[p] = tics[p].emplace(tics[p].begin() + safeInsertIdx, cmdToCopy);
        } else {
            tics[p].emplace(tics[p].begin() + safeInsertIdx, cleanWaitTic);
            if (otherOffset >= safeInsertIdx) {
                ++otherOffset;
            }
            nextTic[p] = tics[p].begin() + std::min(otherOffset, tics[p].size());
        }
    }

    syncActivePlayerTics();
    goForward(1);
    display();
}

void xdre::copyTicX(int val) {
    if (val < 1 || tics[displayplayer].empty() || nextTic[displayplayer] == std::begin(tics[displayplayer])) {
        return;
    }

    // Copy the single source command
    ticcmd_t const cmdToCopy = *std::prev(nextTic[displayplayer]);

    ticcmd_t cleanWaitTic;
    std::memset(&cleanWaitTic, 0, sizeof(cleanWaitTic));

    for (int i = 0; i < val; ++i) {
        auto const insertIdx = static_cast<std::size_t>(std::distance(std::begin(tics[displayplayer]), nextTic[displayplayer]));

        for (int p = 0; p < 4; ++p) {
            if (!playeringameGet(p)) continue;

            auto otherOffset = static_cast<std::size_t>(std::distance(std::begin(tics[p]), nextTic[p]));
            auto safeInsertIdx = std::min(insertIdx, tics[p].size());

            if (p == displayplayer) {
                nextTic[p] = tics[p].emplace(tics[p].begin() + safeInsertIdx, cmdToCopy);
            } else {
                tics[p].emplace(tics[p].begin() + safeInsertIdx, cleanWaitTic);
                if (otherOffset >= safeInsertIdx) {
                    ++otherOffset;
                }
                nextTic[p] = tics[p].begin() + std::min(otherOffset, tics[p].size());
            }
        }
    }

    syncActivePlayerTics();
    goForward(val);
    display();
}

static inline bool isWaitTic(ticcmd_t const& cmd) {
    return cmd.forwardmove == 0 &&
           cmd.sidemove == 0 &&
           cmd.angleturn == 0 &&
           cmd.buttons == 0;
}

void xdre::deleteTic() {
    if (tics[displayplayer].size() <= 1) {
        return;
    }

    bool const atEnd = (nextTic[displayplayer] == std::end(tics[displayplayer]));
    auto ticToErase = atEnd ? std::prev(nextTic[displayplayer]) : nextTic[displayplayer];
    auto const curIdx = static_cast<std::size_t>(std::distance(std::begin(tics[displayplayer]), ticToErase));

    /* prevent time: 0.00 leak */
    if (curIdx < 1) {
        return;
    }

    if (atEnd) {
        tics[displayplayer].erase(ticToErase);
        nextTic[displayplayer] = std::end(tics[displayplayer]);
    } else {
        nextTic[displayplayer] = tics[displayplayer].erase(ticToErase);
    }

    /* critical: only erase tics from others if it's WT */
    for (int p = 0; p < 4; ++p) {
        if (p == displayplayer || !playeringameGet(p)) continue;

        if (curIdx < tics[p].size()) {
            if (isWaitTic(tics[p][curIdx])) {
                auto otherOffset = static_cast<std::size_t>(std::distance(std::begin(tics[p]), nextTic[p]));
                tics[p].erase(tics[p].begin() + curIdx);
                if (otherOffset > curIdx && otherOffset > 0) {
                    --otherOffset;
                }
                nextTic[p] = tics[p].begin() + std::min(otherOffset, tics[p].size());
            }
        }
    }

    invalidateSavepointsAfter(static_cast<int>(curIdx));

    if (atEnd) {
        goBack(1);
    } else {
        goBack(0);
    }

    display();
}

void xdre::deleteTicX(int val) {
    if (val < 1 || tics[displayplayer].size() <= 1) {
        return;
    }

    std::size_t deletedCount = 0;
    bool wasAtEnd = false;
    std::size_t lastIdx = 0;

    for (int i = 0; i < val; ++i) {
        if (tics[displayplayer].size() <= 1) {
            break;
        }

        bool const atEnd = (nextTic[displayplayer] == std::end(tics[displayplayer]));
        wasAtEnd = atEnd;

        auto ticToErase = atEnd ? std::prev(nextTic[displayplayer]) : nextTic[displayplayer];
        auto const curIdx = static_cast<std::size_t>(std::distance(std::begin(tics[displayplayer]), ticToErase));

        if (curIdx < 1) {
            break;
        }

        lastIdx = curIdx;

        if (atEnd) {
            tics[displayplayer].erase(ticToErase);
            nextTic[displayplayer] = std::end(tics[displayplayer]);
        } else {
            nextTic[displayplayer] = tics[displayplayer].erase(ticToErase);
        }

        /* critical: only erase tics from others if it's WT */
        for (int p = 0; p < 4; ++p) {
            if (p == displayplayer || !playeringameGet(p)) continue;

            if (curIdx < tics[p].size()) {
                if (isWaitTic(tics[p][curIdx])) {
                    auto otherOffset = static_cast<std::size_t>(std::distance(std::begin(tics[p]), nextTic[p]));
                    tics[p].erase(tics[p].begin() + curIdx);
                    if (otherOffset > curIdx && otherOffset > 0) {
                        --otherOffset;
                    }
                    nextTic[p] = tics[p].begin() + std::min(otherOffset, tics[p].size());
                }
            }
        }
        ++deletedCount;
    }

    if (deletedCount == 0) return;

    invalidateSavepointsAfter(static_cast<int>(lastIdx));

    if (wasAtEnd) {
        goBack(static_cast<int>(deletedCount));
    } else {
        goBack(0);
    }

    display();
}

void xdre::newTic() {
    auto const curIdx = std::distance(std::begin(tics[displayplayer]), nextTic[displayplayer]);

    nextTic[displayplayer] = tics[displayplayer].emplace(nextTic[displayplayer], ticcmd_t{});

    /* insert WT for all players */
    for (int p = 0; p < 4; ++p) {
        if (p == displayplayer || !playeringameGet(p)) continue;
        auto otherOffset = std::distance(std::begin(tics[p]), nextTic[p]);
        auto insertPos = tics[p].begin() + std::min<std::size_t>(curIdx, tics[p].size());
        tics[p].emplace(insertPos, ticcmd_t{});

        if (otherOffset >= curIdx) ++otherOffset;
        nextTic[p] = tics[p].begin() + std::min<std::size_t>(otherOffset, tics[p].size());
    }

    syncActivePlayerTics();
    goForward(1);
    display();
}

void xdre::changePlayerView(unsigned int plr) {
    if (plr < 1 || plr > 4) {
        return;
    } else if (!playeringameGet(plr - 1)) {
        return;
    } else if (displayplayer == static_cast<int>(plr - 1)) {
        return;
    } else if (gamestate != GS_LEVEL) {
        return;
    }

    /* sync tics */
    syncActivePlayerTics();

    displayplayer = plr - 1;

    /* anchor new playerview iterator to current frame */
    std::size_t targetIndex = (gametic > 0) ? static_cast<std::size_t>(gametic) : 0;
    if (targetIndex > tics[displayplayer].size()) {
        targetIndex = tics[displayplayer].size();
    }
    nextTic[displayplayer] = tics[displayplayer].begin() + targetIndex;

    if (gamestate == GS_LEVEL) {
        ST_Start();
        HU_Start();
    }
    display();
}

void xdre::toggleBlood() {
    palette_onbonus ^= 1;
    palette_ondamage ^= 1;
    palette_onpowers ^= 1;
	
	dsda_UpdateIntConfig(dsda_config_palette_ondamage, palette_ondamage, false);
    dsda_UpdateIntConfig(dsda_config_palette_onbonus,  palette_onbonus,  false);
    dsda_UpdateIntConfig(dsda_config_palette_onpowers, palette_onpowers, false);

    if (gamestate == GS_LEVEL) {
        ST_Start();
        HU_Start();
    }
    display();
}

void xdre::toggleFire() {
    auto tic = getActiveTic();
    if (!tic) return;
    tic->buttons ^= BT_ATTACK;
    goBack(0);
    display();
}

void xdre::toggleUse() {
    auto tic = getActiveTic();
    if (!tic) return;
    tic->buttons ^= BT_USE;
    goBack(0);
    display();
}

void xdre::changeWeapon(char wpn) {
    auto tic = getActiveTic();
    if (!tic) return;
    tic->buttons &= ~(BT_CHANGE | BT_WEAPONMASK);
    if (wpn > 0 && wpn < 10) {
        tic->buttons |= BT_CHANGE;
        tic->buttons |= (wpn - 1) << BT_WEAPONSHIFT;
    }
    goBack(0);
    display();
}

void xdre::getOptions(int options[]) {
    x_getOptions(options);
}

void xdre::changeOptions(int options[]) {
    x_changeOptions(options);
    header.resize(getHeaderSize());
    x_initHeader(header.data());

    x_clearMapSavepoints();
    x_clearUserSavepoint();

    rewindDemoToStart();
    G_XDRE_SetDemoOptions(options[7], options[5], options[6]);

    goForward(1);
    display();
}

static inline signed short toAngleTurn(signed short val) {
    return longtics ? val : static_cast<signed short>(val << 8);
}

void xdre::transpose(bool direction, char mode) {
    if (mode > 2) return;
    auto tic = getActiveTic();
    if (!tic) return;

    signed char sidemove = tic->sidemove;
    if (sidemove == -128) sidemove = -127;
    signed char forwardmove = tic->forwardmove;
    if (forwardmove == -128) forwardmove = -127;

    signed short const angleStep = toAngleTurn(64);

    if (!direction) {
        if (mode != 1) {
            tic->forwardmove = -sidemove;
            tic->sidemove = forwardmove;
        }
        if (mode != 2) tic->angleturn += angleStep;
    } else {
        if (mode != 1) {
            tic->forwardmove = sidemove;
            tic->sidemove = -forwardmove;
        }
        if (mode != 2) tic->angleturn -= angleStep;
    }
    goBack(0);
    display();
}

void xdre::run(signed char val) {
    auto tic = getActiveTic();
    if (!tic) return;
    if (val == -128) val = -127;
    tic->forwardmove = val;
    goBack(0);
    display();
}

void xdre::strafe(signed char val) {
    auto tic = getActiveTic();
    if (!tic) return;
    tic->sidemove = val;
    goBack(0);
    display();
}

/* turn functions */
int turnstep = 1;

void xdre::setTurnStep(int step) {
    turnstep = step;
}

void xdre::turn(signed short val) {
    auto tic = getActiveTic();
    if (!tic) return;
    tic->angleturn = toAngleTurn(val);
    goBack(0);
    display();
}

void xdre::turnStepLeft() {
    auto tic = getActiveTic();
    if (!tic) return;
    tic->angleturn += toAngleTurn(turnstep);
    goBack(0);
    display();
}

void xdre::turnStepRight() {
    auto tic = getActiveTic();
    if (!tic) return;
    tic->angleturn -= toAngleTurn(turnstep);
    goBack(0);
    display();
}

void xdre::setSavepointStyle(int style) {
    if (savepointStyle == style) {
        return;
    }
    savepointStyle = style;
    if (savepointStyle == SpStyle::SP_USER) {
        x_clearMapSavepoints();
    }
}

std::string xdre::getTicString(unsigned int n) {
    if (n >= tics[displayplayer].size()) {
        return "";
    }

    int const divisor = longtics ? 1 : 256;
    std::stringstream ss;
    ticcmd_t const& tic = tics[displayplayer][n];

    if (tic.forwardmove > 0) {
        ss << " MF" << +tic.forwardmove;
    } else if(tic.forwardmove < 0) {
        ss << " MB" << -tic.forwardmove;
    }

    if (tic.sidemove > 0) {
        ss << " SR" << +tic.sidemove;
    } else if(tic.sidemove < 0) {
        ss << " SL" << -tic.sidemove;
    }

    if (tic.angleturn > 0) {
        ss << " TL" << (tic.angleturn / divisor);
    } else if(tic.angleturn < 0) {
        ss << " TR" << -(tic.angleturn / divisor);
    }

    if (tic.buttons & BT_ATTACK) {
        ss << " F";
    }

    if (tic.buttons & BT_USE) {
        ss << " U";
    }

    if (tic.buttons & BT_CHANGE) {
        ss << " G" << (((tic.buttons & BT_WEAPONMASK) >> BT_WEAPONSHIFT) + 1);
    }

    std::string str = ss.str();
    return str.empty() ? " WT" : str;
}

unsigned int xdre::getTiclistSize() {
    return tics[displayplayer].size();
}

int xdre::getRngIndex() {
    return x_getRngIndex();
}

double xdre::getXPos() {
    return x_getXPos();
}

double xdre::getYPos() {
    return x_getYPos();
}

double xdre::getZPos() {
    return x_getZPos();
}

double xdre::getXDist() {
    return x_getXPos() - x_getPrevXPos();
}

double xdre::getYDist() {
    return x_getYPos() - x_getPrevYPos();
}

double xdre::getZDist() {
    return x_getZPos() - x_getPrevZPos();
}

double xdre::getXMom() {
    return x_getXMom();
}

double xdre::getYMom() {
    return x_getYMom();
}

double xdre::getDistanceMoved() {
    auto dx = xdre::getXDist();
    auto dy = xdre::getYDist();
    return std::sqrt(dx * dx + dy * dy);
}

double xdre::getDirectionMoved() {
    auto dx = xdre::getXDist();
    auto dy = xdre::getYDist();
    if (!dx && !dy) {
        return 0;
    }
    auto angle = std::atan(dx / dy) * 180 / X_PI - 90;
    angle = dy >= 0 ? -angle : -angle + 180;
    return angle;
}

double angle_divisor = 1;
void xdre::setAngleType(int type) {
    x_setAngleType(type);
}

unsigned short xdre::getAngle(short type) {
    return x_getAngle(type);
}

bool xdre::getUseSuccess() {
    return xUseSuccess ? true : false;
}

void xdre::setLinedefCheck(int linedef) {
    x_setLinedefCheck(linedef);
}

bool xdre::getLinedefCrossed() {
    return xLineCrossed ? true : false;
}

double xdre::getLinedefVertex(int v, int xy) {
    if (xLineId == -1 || v < 0 || v > 1 || xy < 0 || xy > 1) {
        return 0;
    }
    return xLineVertexes[v][xy] / static_cast<double>(FRACUNIT);
}

void xdre::setSectorCheck(int sector) {
    x_setSectorCheck(sector);
}

double xdre::getSectorInfo(int field) {
    if (secID == -1 || field < 0 || field > 2) {
        return 0;
    }
    if (field > 1) {
        return xSectorInfo[field];
    }
    return xSectorInfo[field] / static_cast<double>(FRACUNIT);
}

void xdre::setThingCheck(int thing) {
    x_setThingCheck(thing);
}

double xdre::getThingInfo(int field) {
    if (thingID == -1 || field < 0 || field > 7) {
        return 0;
    }
    if (field < 4) {
        return xThingInfo[field] / static_cast<double>(FRACUNIT);
    }
    return xThingInfo[field];
}

double xdre::getDistanceFromLine() {
    if (xLineId == -1) {
        return 0;
    }

    struct {
        double x;
        double y;
    } PA, AB, PX, v1, v2;

    v1.x = getLinedefVertex(0, 0);
    v1.y = getLinedefVertex(0, 1);
    v2.x = getLinedefVertex(1, 0);
    v2.y = getLinedefVertex(1, 1);

    PA.x = v1.x - x_getXPos();
    PA.y = v1.y - x_getYPos();
    AB.x = v2.x - v1.x;
    AB.y = v2.y - v1.y;

    double const denom = AB.x * AB.x + AB.y * AB.y;
    if (denom <= 0.0) {
        return 0.0;
    }

    double t = (-PA.x * AB.x + -PA.y * AB.y) / denom;
    PX.x = PA.x + t * AB.x;
    PX.y = PA.y + t * AB.y;

    double dist = std::sqrt(PX.x * PX.x + PX.y * PX.y);
    if (std::abs(v2.x - v1.x) > 1e-9) {
        if (((v2.y - v1.y) / (v2.x - v1.x)) * (x_getXPos() - v1.x) + v1.y < x_getYPos()) {
            return -dist;
        }
    } else {
        if (x_getXPos() < v1.x) {
            return -dist;
        }
    }
    return dist;
}

int xdre::getDoneDamage() {
    return xDoneDamage;
}

float xdre::getTime() {
    return leveltime / 35.0;
}

unsigned int xdre::getCurrentTic() {
    return gametic - 1;
}

unsigned int xdre::getSavepointTic() {
    return x_getSavepointTic(gametic);
}

int xdre::getSavepointStyle() {
    return savepointStyle;
}

bool xdre::getBlood() {
    return palette_ondamage ? true : false;
}

static const ticcmd_t* getRelativeTic(int i, int listLength) {
    auto& curTics = tics[displayplayer];
    if (curTics.empty()) {
        return nullptr;
    }

    auto curIndex = static_cast<std::ptrdiff_t>(std::distance(std::begin(curTics), nextTic[displayplayer]));
    std::ptrdiff_t targetIndex = curIndex - listLength + i;

    if (targetIndex < 0 || static_cast<std::size_t>(targetIndex) >= curTics.size()) {
        return nullptr;
    }

    return &curTics[static_cast<std::size_t>(targetIndex)];
}

int xdre::getCurrentStrafe(int i, int listLength) {
    auto cmd = getRelativeTic(i, listLength);
    return cmd ? cmd->sidemove : 0;
}

int xdre::getCurrentRun(int i, int listLength) {
    auto cmd = getRelativeTic(i, listLength);
    return cmd ? cmd->forwardmove : 0;
}

int xdre::getCurrentTurn(int i, int listLength) {
    auto cmd = getRelativeTic(i, listLength);
    return cmd ? cmd->angleturn : 0;
}
