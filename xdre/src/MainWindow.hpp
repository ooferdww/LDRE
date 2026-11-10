#ifndef __MAINWINDOW_HPP
#define __MAINWINDOW_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>
#include <windows.h>
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
    int taskIndex = -1;
    HANDLE hProcess = NULL;
    HANDLE hThread = NULL;
    DWORD pid = 0;
};

enum class XCmd {
    None,
    Fire,
    Use,
    TurnStepLeft,
    TurnStepRight,
    PrevTic,
    NextTic,
    CopyTic,
    DeleteTic,
    NewTic,
    ToggleBlood,
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

    XCmd GetCommandForString(const std::string& key) const {
        auto it = m_stringToCommandMap.find(key);
        if (it != m_stringToCommandMap.end()) return it->second;
        return XCmd::None;
    }

    const std::string& GetStringForConfig(const std::string& configName) const {
        static const std::string empty;
        auto it = m_configToStringMap.find(configName);
        if (it != m_configToStringMap.end()) return it->second;
        return empty;
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
    void CleanupBRSArtifacts(int winningIndex);
	void DetectStartupWads(int argc, char** argv);

    bool m_showBRSDialog = false;

    // brs state
    std::vector<BRSTicItem> m_brsTics;
    int m_selectedBRSTic = -1;
    
    // brs engine and path
    char m_brsExePath[MAX_PATH] = "dsda-doom.exe";
    char m_brsIwadPath[MAX_PATH] = "";
    char m_brsPwadPath[MAX_PATH] = "";
    char m_brsDemoPath[MAX_PATH] = "";
    char m_brsExtraArgs[256] = "-bf_quiet -nosound -geom 320x240";
    int m_brsThreadGoal = 0;
    bool m_brsNomo = false;
    bool m_brsPreserveUndefined = true;
    int m_brsAutoRangePreset = 2; // Default range 3
	bool m_brsAutoLoadResult = true;
    bool m_brsAutoLoadResultEditBuffer = true;
    int  m_brsInitialTic = 0;
    std::atomic<bool> m_brsPendingReload{false};
    std::string m_brsPendingReloadPath = "";

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
    BRSCheckItem m_chkArmour, m_chkHealth;
	BRSCheckItem m_chkUse;
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
	bool m_invertTicScrub = true;
    bool m_invertTicScrubEditBuffer = true;
    unsigned int m_lastRenderedTic = 0xFFFFFFFF;

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