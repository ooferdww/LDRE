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
} /* extern "C" */

/*
 * xdre private stuff
 */
constexpr double X_PI = 3.14159265358979323846;

unsigned char* header;
std::size_t headerSize;
std::vector<ticcmd_t> tics[4];
std::vector<ticcmd_t>::iterator nextTic[4];
int savepointStyle;

void display();
bool loadReplay(unsigned char const* demo_p, std::size_t size);
unsigned char const* readTics(unsigned char const* start_p, std::size_t size);
void makeCmds();
void goForward(int numTics);
void goBack(int numTics);

void display() {
    if (gamestate == GS_LEVEL) {
        if (players[displayplayer].mo != NULL) {
            D_Display();
        }
    } else if (gamestate == GS_INTERMISSION || gamestate == GS_FINALE) {
        D_Display();
    }
}

bool loadReplay(unsigned char const* demo_p, std::size_t size) {
    unsigned char const* tics_p = XDRE_ReadDemoHeader(demo_p, size);
    if (!tics_p) {
        return false;
    }

    /* demo opts from first read */
    G_XDRE_SetDemoOptions(nomonsters, respawnparm, fastparm);

    if (headerSize != (tics_p - demo_p)) {
        delete[] header;
        headerSize = tics_p - demo_p;
        header = new unsigned char [headerSize];
    }

    std::memcpy(header, demo_p, headerSize);

    x_clearMapSavepoints();
    x_clearUserSavepoint();
    for (int p = 0; p < 4; ++p) {
        tics[p].clear();
    }

    unsigned char const* demomarker_p = readTics(tics_p, size - (tics_p - demo_p));

    /* reset engine from goback */
    gametic = basetic = 0;
    playeringame[0] = true;
    XDRE_ReadDemoHeader(header, headerSize);
    playeringame[0] = true;

    for (int p = 0; p < 4; ++p) {
        nextTic[p] = std::begin(tics[p]);
    }

    displayplayer = consoleplayer;
	
	/* display fix */
	goForward(1);
	display();

    return true;
}

unsigned char const* readTics(unsigned char const* start_p, std::size_t size) {
    unsigned char const* tic_p = start_p;
    int plr = 0;
    for (int i = 0; i < 4; ++i) {
        if (playeringameGet(i)) {
            plr = i;
            break;
        }
    }

    while (*tic_p != 0x80 && tic_p - start_p < size) {
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
        while (!playeringameGet(plr)) {
            if(plr >= 3) {
                plr = 0;
            } else {
                ++plr;
            }
        }
    }

    return *tic_p == 0x80 ? tic_p : nullptr;
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

        for (int p = 0; p < 4; ++p) {
            if (playeringameGet(p)) {
                players[p].cmd = netcmds[p];
            }
        }

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

    auto oldTic = gametic;
    auto destTic = gametic - numTics;
    if (destTic < 1) {
        destTic = 1;
    }

    if (savepointStyle == SpStyle::SP_USER) {
        auto temp = x_getSavepointTic(gametic);
        destTic = destTic <= temp ? temp + 1 : destTic;
    }

    if (!x_loadSavepoint(destTic)) {
        /* only advance valid player streams */
        for (int p = 0; p < 4; ++p) {
            if (playeringameGet(p) && !tics[p].empty()) {
                std::advance(nextTic[p], gametic - oldTic);
            }
        }
        goForward(destTic - gametic);
    } else {
        x_clearMapSavepoints();
        x_clearUserSavepoint();
        gametic = 0;
        playeringame[0] = true;
        XDRE_ReadDemoHeader(header, headerSize);
        playeringame[0] = true;
        for (int p = 0; p < 4; ++p) {
            if (playeringameGet(p) && !tics[p].empty()) {
                nextTic[p] = std::begin(tics[p]);
            }
        }
        goForward(destTic);
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
    auto bruteForward = [](int numTics) {
        if (numTics < 1) {
            return;
        }

        while (numTics-- > 0) {
            xUseSuccess = 0;
            xLineCrossed = 0;
            xDoneDamage = 0;

            makeCmds();
			for (int p = 0; p < 4; ++p) {
            if (playeringameGet(p)) {
                players[p].cmd = netcmds[p];
            }
        }
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
            std::advance(nextTic[displayplayer], gametic - oldTic);
            bruteForward(gameticOnEnter - gametic);
        } else {
            gametic = 0;
            XDRE_ReadDemoHeader(header, headerSize);
            for (int p = 0; p < 4; ++p) {
                nextTic[p] = std::begin(tics[p]);
            }
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
        tics[p].resize(1000000, {});
        tics[p].resize(10, {});
    }
    nextTic[0] = std::begin(tics[0]);
    for (int p = 1; p < 4; ++p) {
        tics[p].clear();
        nextTic[p] = std::end(tics[p]);
    }

    headerSize = getHeaderSize();
    header = new unsigned char [headerSize];
    x_initHeader(header);

    palette_onbonus = 0;
    palette_ondamage = 0;
    palette_onpowers = 0;
	
	dsda_UpdateIntConfig(dsda_config_palette_ondamage, 0, false);
    dsda_UpdateIntConfig(dsda_config_palette_onbonus,  0, false);
    dsda_UpdateIntConfig(dsda_config_palette_onpowers, 0, false);

    savepointStyle = SpStyle::SP_START;

    if (gamestate == GS_LEVEL) {
        goForward(1);
        display();
    }
    done = true;
}

bool xdre::bruteForce(std::vector<BruteTic> bruteTics,
                      BruteCheck const& bruteCheck,
                      bool const& abort) {
    std::sort(std::begin(bruteTics), std::end(bruteTics), [](BruteTic const& lhs, BruteTic const& rhs){return lhs.tic < rhs.tic;});
    int bruteStart = bruteTics.front().tic;
    int bruteEnd = bruteTics.back().tic;
    if (bruteStart < 0 || tics[displayplayer].size() - 1 < bruteEnd || bruteCheck.tic < bruteStart) {
        return false;
    }

    x_clearMapSavepoints();
    x_clearUserSavepoint();

    gametic = 0;
    XDRE_ReadDemoHeader(header, headerSize);
    for (int p = 0; p < 4; ++p) {
        nextTic[p] = std::begin(tics[p]);
    }

    int destSavepoint = bruteStart;
    while (destSavepoint-- > 0) {
        makeCmds();
		for (int p = 0; p < 4; ++p) {
            if (playeringameGet(p)) {
                players[p].cmd = netcmds[p];
            }
        }
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
    rewind(file);

    fwrite(header, sizeof header[0], headerSize, file);

    decltype(std::begin(tics[0])) iters[4] {
        std::begin(tics[0]),
        std::begin(tics[1]),
        std::begin(tics[2]),
        std::begin(tics[3]),
    };

    decltype(std::end(tics[0])) ends[4] {
        std::end(tics[0]),
        std::end(tics[1]),
        std::end(tics[2]),
        std::end(tics[3]),
    };

    ticcmd_t nulltic {};
    while (iters[0] != ends[0] ||
           iters[1] != ends[1] ||
           iters[2] != ends[2] ||
           iters[3] != ends[3]) {
        for (int p = 0; p < 4; ++p) {
            if (iters[p] != ends[p]) {
                XDRE_WriteDemoTiccmd(&*iters[p], file);
                ++iters[p];
            } else if(playeringameGet(p))
                XDRE_WriteDemoTiccmd(&nulltic, file);
        }
    }

    fputc(0x80, file);

    XDRE_WriteDemoFooter(file);

    return true;
}

bool xdre::load(std::FILE* file) {
    std::fseek(file, 0, SEEK_END);
    auto size = std::ftell(file);
    rewind(file);
    auto data = new unsigned char[size];
    std::fread(&data[0], sizeof data[0], size, file);

    bool ret = loadReplay(data, size);

    delete[] data;
    return ret;
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
    auto tic = std::prev(nextTic[displayplayer]);
    if (tic == std::end(tics[displayplayer])) {
        return;
    }

    tics[displayplayer].emplace(nextTic[displayplayer], *tic);
    goForward(1);
    display();
}

void xdre::copyTicX(int val) {
    auto tic = std::prev(nextTic[displayplayer]);
    if (tic == std::end(tics[displayplayer])) {
        return;
    }

    for (int i = 0; i < val; i++) {
        tics[displayplayer].emplace(nextTic[displayplayer], *tic);
        goForward(1);
    }
    display();
}

void xdre::deleteTic() {
    auto tic = std::prev(nextTic[displayplayer]);
    if (tic == std::end(tics[displayplayer])) {
        return;
    }

    if(!tics[displayplayer].size()) {
        return;
    }
    tics[displayplayer].erase(tic);
    goBack(0);
    display();
}

void xdre::deleteTicX(int val) {
    auto tic = std::prev(nextTic[displayplayer]);
    if (tic == std::end(tics[displayplayer])) {
        return;
    }
    for (int i = 0; i < val; i++) {
        if (!tics[displayplayer].size()) {
            return;
        }
        tics[displayplayer].erase(tic);
        goBack(1);
    }
    display();
}

void xdre::newTic() {
    tics[displayplayer].emplace(nextTic[displayplayer], ticcmd_t{});
    goForward(1);
    display();
}

void xdre::changePlayerView(unsigned int plr) {
    if (plr < 1 || plr > 4) {
        return;
    } else if (!playeringameGet(plr - 1)) {
        return;
    } else if (displayplayer == plr - 1) {
        return;
    } else if (gamestate != GS_LEVEL) {
        return;
    }
    displayplayer = plr - 1;

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
    std::prev(nextTic[displayplayer])->buttons ^= BT_ATTACK;
    goBack(0);
    display();
}

void xdre::toggleUse() {
    std::prev(nextTic[displayplayer])->buttons ^= BT_USE;
    goBack(0);
    display();
}

void xdre::changeWeapon(char wpn) {
    auto tic = std::prev(nextTic[displayplayer]);
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
    headerSize = getHeaderSize();
    delete[] header;
    header = new unsigned char [headerSize];
    x_initHeader(header);

    x_clearMapSavepoints();
    x_clearUserSavepoint();

    gametic = 0;
    playeringame[0] = true;

    /* ensure clnomonsters before reading header */
    G_XDRE_SetDemoOptions(options[7], options[5], options[6]);

    XDRE_ReadDemoHeader(header, headerSize);

    playeringame[0] = true;
    for (int p = 0; p < 4; ++p) {
        nextTic[p] = std::begin(tics[p]);
    }

    /* fix residual state */
    G_XDRE_SetDemoOptions(options[7], options[5], options[6]);

    goForward(1);
    display();
}

void xdre::transpose(bool direction, char mode) {
    if (mode > 2) {
        return;
    }
    signed char sidemove = std::prev(nextTic[displayplayer])->sidemove;
    if (sidemove == -128) {
        sidemove = -127;
    }
    signed char forwardmove = std::prev(nextTic[displayplayer])->forwardmove;
    if (!direction) {
        if (mode != 1) {
            std::prev(nextTic[displayplayer])->forwardmove = -sidemove;
            std::prev(nextTic[displayplayer])->sidemove = forwardmove;
            }
        if (mode != 2) std::prev(nextTic[displayplayer])->angleturn += 64 << 8;
    } else {
        if (mode != 1) {
            std::prev(nextTic[displayplayer])->forwardmove = sidemove;
            std::prev(nextTic[displayplayer])->sidemove = -forwardmove;
            }
        if (mode != 2) std::prev(nextTic[displayplayer])->angleturn -= 64 << 8;
    }
    goBack(0);
    display();
}

void xdre::run(signed char val) {
    if (val == -128) {
        val = -127;
    }
    std::prev(nextTic[displayplayer])->forwardmove = val;
    goBack(0);
    display();
}

void xdre::strafe(signed char val) {
    std::prev(nextTic[displayplayer])->sidemove = val;
    goBack(0);
    display();
}

void xdre::turn(signed short val) {
    if (!longtics) {
        std::prev(nextTic[displayplayer])->angleturn = val << 8;
    } else {
        std::prev(nextTic[displayplayer])->angleturn = val;
    }
    goBack(0);
    display();
}

int turnstep = 1;
void xdre::setTurnStep(int step) {turnstep = step;}

void xdre::turnStepLeft() {
    if (!longtics) {
        std::prev(nextTic[displayplayer])->angleturn += turnstep << 8;
    } else {
        std::prev(nextTic[displayplayer])->angleturn += turnstep;
    }
    goBack(0);
    display();
}

void xdre::turnStepRight() {
    if (!longtics) {
        std::prev(nextTic[displayplayer])->angleturn -= turnstep << 8;
    } else {
        std::prev(nextTic[displayplayer])->angleturn -= turnstep;
    }
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

    int divisor;
    if (longtics) {
        divisor = 1;
    } else {
        divisor = 256;
    }
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

    double dist;
    double t;
    v1.x = getLinedefVertex(0, 0);
    v1.y = getLinedefVertex(0, 1);
    v2.x = getLinedefVertex(1, 0);
    v2.y = getLinedefVertex(1, 1);

    PA.x = v1.x - x_getXPos();
    PA.y = v1.y - x_getYPos();
    AB.x = v2.x - v1.x;
    AB.y = v2.y - v1.y;

    t = -PA.x * AB.x + -PA.y * AB.y;
    t /= AB.x * AB.x + AB.y * AB.y;

    PX.x = PA.x + t * AB.x;
    PX.y = PA.y + t * AB.y;

    dist = std::sqrt(PX.x * PX.x + PX.y * PX.y);
    if (((v2.y - v1.y) / (v2.x - v1.x)) * (x_getXPos() - v1.x) + v1.y < x_getYPos()) {
        return -dist;
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

int xdre::getCurrentStrafe(int i, int listLength) {
    return std::prev(nextTic[displayplayer]-listLength+i+1)->sidemove;
}

int xdre::getCurrentRun(int i, int listLength) {
    return std::prev(nextTic[displayplayer]-listLength+i+1)->forwardmove;
}

int xdre::getCurrentTurn(int i, int listLength) {
    return std::prev(nextTic[displayplayer]-listLength+i+1)->angleturn;
}