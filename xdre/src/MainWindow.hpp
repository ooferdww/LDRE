#ifndef __MAINWINDOW_HPP
#define __MAINWINDOW_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <limits.h>
#ifndef MAX_PATH
#define MAX_PATH PATH_MAX
#endif
#endif

#include "xdre.hpp"

// brs settings
struct BRSTicItem {
    int ticNumber = 0;
    char runStr[32] = "0:0";
    char strafeStr[32] = "0:0";
    char turnStr[32] = "0:0";
    int fire = 0;       // 0: No, 1: Yes, 2: Both
    int use = 0;        // 0: No, 1: Yes, 2: Both
    int weapon = 0;     // 0: No change, 1-9: Weapon index

    // cached range factors
    std::vector<int> parsedRun;
    std::vector<int> parsedStrafe;
    std::vector<int> parsedTurn;
    int runSize = 1;
    int strafeSize = 1;
    int turnSize = 1;
    std::vector<int> runFactors;
    std::vector<int> strafeFactors;
    std::vector<int> turnFactors;
};

// task queue
struct BRSTask {
    int taskIndex;
    std::string command;
};

// worker tracking
struct BRSWorker {
    int taskIndex;
#ifdef _WIN32
    HANDLE hProcess;
    HANDLE hThread;
    DWORD pid;
#else
    pid_t pid;
#endif
};

enum class XCmd {
    None,
    Fire,
    Use,
    TurnStepLeft,
    TurnStepRight,
	RunStepFwd,
    RunStepBwd,
    StrafeStepLeft,
    StrafeStepRight,
    PrevTic,
    NextTic,
    CopyTic,
    DeleteTic,
    NewTic,
	ClearMovement,
    ToggleBlood,
	ToggleAutomap,
	ToggleAutomapFollow,
	ZoomAutomapIn,
    ZoomAutomapOut,
	PanAutomapLeft,
    PanAutomapRight,
    PanAutomapUp,
    PanAutomapDown,
	ToggleBlockmap,
	ToggleThingHealth,
    ToggleThingIndex,
    SetSavepoint,
    UnsetSavepoint,
    Load,
    Save,
    Exit,
    ChangePlayer,
    TransposeLeft,
    TransposeRight,
    Weapon,
    RunBwd,
    RunFwd,
    StrafeLeft,
    StrafeRight,
    TurnStep,
    CopyTicX,
    DeleteTicX,
    TurnLeft,
    TurnRight,
    BackwardDemo,
    ForwardDemo
};

struct ParsedWinTicCmd {
    int frame = -1;
    int forwardmove = 0;
    int sidemove = 0;
    int angleturn = 0;
    unsigned int buttons = 0;
};

class MainWindow {
public:
    MainWindow(int argc = 0, char** argv = nullptr);
    ~MainWindow();

    void Init();
    void Render();
    void SetKeys();
    void LoadConfig();
    void WriteConfig();
    void DoInput(XCmd cmd, unsigned int val = 0);

    bool IsRunning() const { return m_running; }
    void RequestExit() { m_showExitDialog = true; }
    void ForceExit() { m_running = false; }

    XCmd GetCommandForString(const std::string& str) const {
        if (xdre::getAutomapActive()) {
            auto it = m_automapStringToCommandMap.find(str);
            return (it != m_automapStringToCommandMap.end()) ? it->second : XCmd::None;
        }
        auto it = m_stringToCommandMap.find(str);
        return (it != m_stringToCommandMap.end()) ? it->second : XCmd::None;
    }

    const std::string& GetStringForConfig(const std::string& action) const {
        static const std::string emptyStr = "";
        const auto& activeCfg = xdre::getAutomapActive() ? m_automapConfigToStringMap : m_configToStringMap;
        auto it = activeCfg.find(action);
        return (it != activeCfg.end()) ? it->second : emptyStr;
    }
	
	bool IsInputBufferEmpty() const {
		return m_inputBuffer[0] == '\0';
	}
	
	bool IsTicListHovered() const {
		return m_ticListHovered;
	}
	
	// window position properties
    int GetGuiPosX() const { return m_guiPosX; }
    int GetGuiPosY() const { return m_guiPosY; }
    int GetDoomPosX() const { return m_doomPosX; }
    int GetDoomPosY() const { return m_doomPosY; }

    void SetGuiPos(int x, int y) { m_guiPosX = x; m_guiPosY = y; }
    void SetDoomPos(int x, int y) { m_doomPosX = x; m_doomPosY = y; }

private:
    void RenderBRSDialog();
    void StartBRSSearch();
    void AbortBRSSearch();
    void BRSWorkerThread();
    void CleanupBRSArtifacts();
	void DetectStartupWads(int argc, char** argv);

    bool m_showBRSDialog = false;

    // brs state
    std::vector<BRSTicItem> m_brsTics;
    int m_selectedBRSTic = -1;
    
    // brs engine and path
#ifdef _WIN32
    char m_brsExePath[512] = "dsda-doom.exe";
#else
    char m_brsExePath[512] = "dsda-doom";
#endif
    char m_brsIwadPath[MAX_PATH] = "";
    char m_brsPwadPath[MAX_PATH] = "";
    char m_brsDemoPath[MAX_PATH] = "";
    char m_brsExtraArgs[256] = "-bf_quiet -nosound -nomusic -nojoy -geom 320x240";
    int m_brsThreadGoal = 0;
    bool m_brsNomo = false;
    int m_brsAutoRangePreset = 3; // Default range 3
    int m_brsInitialTic = 0;
	int m_brsTargetTic = 0;
	bool m_brsAutoFireEnabled = false;
    int  m_brsAutoFireStartTic = 0;
    int  m_brsAutoFireGap = 1;
	int GetSelectedComplevel() const;
    std::atomic<bool> m_brsPendingReload{false};
    std::string m_brsPendingReloadPath = "";
	int m_brsPendingWinBaseTic = -1;
    std::vector<ParsedWinTicCmd> m_brsPendingWinCmds;

    // brs conditions
    struct BRSCheckItem {
        bool active = false;
        int op = 0; // 0: =, 1: !=, 2: >=, 3: >, 4: <=, 5: <, 6: Min, 7: Max
        char val[32] = "0";
    };

    BRSCheckItem m_chkX, m_chkY, m_chkZ;
    BRSCheckItem m_chkMomX, m_chkMomY, m_chkSpeed;
    BRSCheckItem m_chkDamage, m_chkRng, m_chkLinedef;
	int m_chkLinedefType = 0; // 0: act, 1: skip
	BRSCheckItem m_chkItem;
	int m_chkItemMode = 0;  // 0: have, 1: lack
    int m_chkItemIndex = 0; // index into item dropdown
    BRSCheckItem m_chkArmour, m_chkHealth;
	BRSCheckItem m_chkUse;
	BRSCheckItem m_chkAngle;
    int m_chkAngleType = 3; // 0: Longtics, 1: Fine Angles, 2: Degrees, 3: Byte Angles (or sync with m_selectedAngleType)
    char m_brsExtraChecks[256] = "";

    // brs background engine
    std::thread m_brsSupervisorThread;
    std::mutex m_brsWorkerMutex;
    std::atomic<bool> m_brsIsRunning{false};
    std::atomic<bool> m_brsAbortRequested{false};
    std::atomic<int> m_brsCompletedTasks{0};
    std::atomic<int> m_brsTotalTasks{0};
    int m_brsWinningTaskIndex = -1;
    std::string m_brsStatusMessage = "";
    std::string m_brsResultDetails = "";

    void ProcessInputField(const char* input);
	
	// dsda segmentation
	void RenderSegmentDialog();
    void LaunchSegmentInstance(bool withDemo = true);
	std::string OpenSegmentFolderDialog();
    std::string OpenSegmentConfigDialog();
	
	bool m_showSegmentDialog = false;
    char m_segmentDirPath[MAX_PATH] = "dsda_segment";
    bool m_segmentUseCustomConfig = false;
    char m_segmentConfigPath[MAX_PATH] = "";
    char m_segmentExtraArgs[250] = "";
    char m_segmentDemoPath[MAX_PATH] = "";
    int  m_segmentSkipTic = 0;
    int  m_segmentGameSpeed = 50;
    std::string m_segmentStatusMessage = "";
	bool m_segmentAutoLoadResult = true;
    bool m_segmentAutoLoadResultEditBuffer = true;
    std::atomic<bool> m_segmentPendingReload{false};
    std::string m_segmentPendingReloadPath = "";
    std::atomic<uint64_t> m_segmentLaunchGen{0};
    
    // ui layout
    void RenderMenuBar();
    void RenderTicList();
    void RenderTrackingPanels();
    void RenderLinedefTracking();
    void RenderSectorTracking();
    void RenderThingTracking();
    void RenderPlayerMovement();
    void RenderGeneralSection();
    
    // native file dialog
    std::string OpenNativeFileDialog(bool isSave);
    void LoadReplayFile();
    bool SaveReplayFile();
    std::string m_lastDirectory = ".";

    void RenderHeaderDialog();
    void RenderKeysDialog();
	void RenderExitDialog();
    void RenderBruteForceDialog();
	void RenderOptionsDialog();

    bool m_running = true;
    bool m_autoScrollTicList = true;
	bool m_showExitDialog = false;
	bool m_showOptionsDialog = false;
	bool m_bloodEnabled = false;
    bool m_bloodEditBuffer = false;
	bool m_blockmapGridEnabled = false;
    bool m_blockmapGridEditBuffer = false;
	bool m_thingHealthEnabled = false;
    bool m_thingHealthEditBuffer = false;
    bool m_thingIndexEnabled = false;
    bool m_thingIndexEditBuffer = false;
	int m_automapPanSpeed = 2;
    int m_automapPanSpeedEditBuffer = 2;
	bool m_dsdaMovementFormat = false;
    bool m_dsdaMovementFormatEditBuffer = false;
	int m_movementLimit = 1; // 0: no limit, 1: non-turbo, 2: stroller
    int m_movementLimitEditBuffer = 1;
	
	// momentum loss highlight
    bool m_momHighlightEnabled = true;
    bool m_momHighlightEditBuffer = true;
    float m_momLossThreshold = 5.0f;
    float m_momLossThresholdEditBuffer = 5.0f;

    std::vector<std::pair<double, double>> m_ticMomentumCache;
	std::vector<uint8_t> m_ticMomentumValid;
    unsigned int m_lastMomCachedTic = static_cast<unsigned int>(-1);
	
	bool m_invertTicScrub = true;
    bool m_invertTicScrubEditBuffer = true;
    unsigned int m_lastRenderedTic = 0xFFFFFFFF;
	int m_ticSelStart = -1;
    int m_ticSelEnd = -1;
    bool m_ticShiftSelecting = false;
    bool m_ticListHovered = false;

    // config and keybindings
    std::unordered_map<std::string, std::string> m_configToStringMap;
    std::unordered_map<std::string, XCmd> m_stringToCommandMap;

    // cmd input
    char m_inputBuffer[128] = "";

    // line sector thing track history
    std::vector<std::string> m_linedefHistory = {"0"};
    char m_linedefInput[32] = "0";
    int m_selectedLinedefIndex = 0;

    std::vector<std::string> m_sectorHistory = {"0"};
    char m_sectorInput[32] = "0";
    int m_selectedSectorIndex = 0;

    std::vector<std::string> m_thingHistory = {"0"};
    char m_thingInput[32] = "0";
    int m_selectedThingIndex = 0;

    int m_selectedAngleType = 3;
    int m_selectedSavepointStyle = 1;

    // modal state toggles
    bool m_showHeaderDialog = false;
    bool m_showKeysDialog = false;
    bool m_showBruteDialog = false;

    // header editor state
    int m_headerOptions[28] = {0};
    int m_headerComplevelIndex = 0;

    // keys dialog state, has buffers for all keys
    struct KeyBindEntry {
        std::string actionName;
        char buffer[32];
    };
    std::vector<KeyBindEntry> m_keyEditList;
	
	// automap controls
	std::unordered_map<std::string, std::string> m_automapConfigToStringMap;
    std::unordered_map<std::string, XCmd> m_automapStringToCommandMap;
    std::vector<KeyBindEntry> m_automapKeyEditList;

    // bruteforce engine state
    struct BruteItem {
        char ticStr[32] = "0";
        char runStr[64] = "0";
        char strafeStr[64] = "0";
        char turnStr[64] = "0";
        int fire = 0;
        int use = 0;
        int turnOrAngle = 0;
    };

    std::vector<BruteItem> m_bruteItems;
    int m_selectedBruteItem = -1;

    // bruteforce checks
    int m_checkTic = 0;
    bool m_checkUse = false;
    bool m_rngActive = false;
    int m_rngOp = 0;
    char m_rngVal[64] = "0";

    bool m_damageActive = false;
    int m_damageOp = 0;
    int m_damageVal = 0;

    bool m_xPosActive = false; int m_xPosOp = 0; double m_xPosVal = 0.0;
    bool m_yPosActive = false; int m_yPosOp = 0; double m_yPosVal = 0.0;
    bool m_zPosActive = false; int m_zPosOp = 0; double m_zPosVal = 0.0;
    bool m_xMomActive = false; int m_xMomOp = 0; double m_xMomVal = 0.0;
    bool m_yMomActive = false; int m_yMomOp = 0; double m_yMomVal = 0.0;
    bool m_speedActive = false; int m_speedOp = 0; double m_speedVal = 0.0;

    int m_autoRangeChoice = 2; // range 3
    bool m_avoidTurbo = true;
	
	// window position
	int m_guiPosX  = -1;
    int m_guiPosY  = -1;
    int m_doomPosX = -1;
    int m_doomPosY = -1;

    // background threading
    std::thread m_bruteThread;
    std::atomic<bool> m_bruteRunning{false};
    std::atomic<bool> m_abortBrute{false};
    std::atomic<bool> m_bruteSuccess{false};
    std::atomic<bool> m_bruteFinishedNotification{false};
};

#endif // __MAINWINDOW_HPP
