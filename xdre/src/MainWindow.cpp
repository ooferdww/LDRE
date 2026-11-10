/* LDRE: User interface
 * Authors: ooferdww */

#include "MainWindow.hpp"
#include "../imgui/imgui.h"
#include "SDL.h"

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#endif

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <vector>

extern "C" void XDRE_GetLoadedWads(char *iwad_out, size_t iwad_len, char *pwad_out, size_t pwad_len);

// bruteforce temporary saves
static std::string GetExeDirectory() {
#ifdef _WIN32
    char path[MAX_PATH] = "";
    if (GetModuleFileNameA(NULL, path, MAX_PATH) > 0) {
        char* lastSlash = strrchr(path, '\\');
        if (!lastSlash) lastSlash = strrchr(path, '/');
        if (lastSlash) {
            *(lastSlash + 1) = '\0';
            return std::string(path);
        }
    }
#endif
    return "";
}

static std::string GetBFDirectory() {
    std::string base = GetExeDirectory();
    return base.empty() ? "bf" : (base + "bf");
}

static std::string GetBFBackupDirectory() {
    std::string base = GetExeDirectory();
    return base.empty() ? "bf_backup" : (base + "bf_backup");
}

// ensure dir exists
static void Win32_CreateDirectory(const std::string& path) {
#ifdef _WIN32
    CreateDirectoryA(path.c_str(), NULL);
#endif
}

// slash fixer
static void NormalizePathSeparators(char* path) {
    if (!path) return;
#ifdef _WIN32
    for (size_t i = 0; path[i] != '\0'; ++i) {
        if (path[i] == '/') {
            path[i] = '\\';
        }
    }
#endif
}

// recursively erase dir contents with lingering process handles in mind
static bool Win32_DeleteDirectoryInternal(const std::string& path) {
#ifdef _WIN32
    std::string searchPath = path + "\\*.*";
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return false;

    bool allDeleted = true;
    do {
        if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0) {
            std::string itemPath = path + "\\" + fd.cFileName;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (!Win32_DeleteDirectoryInternal(itemPath)) {
                    allDeleted = false;
                }
            } else {
                SetFileAttributesA(itemPath.c_str(), FILE_ATTRIBUTE_NORMAL);

                // retry loop for locked files
                bool deleted = false;
                for (int attempt = 0; attempt < 10; ++attempt) {
                    if (DeleteFileA(itemPath.c_str())) {
                        deleted = true;
                        break;
                    }
                    Sleep(50);
                }
                if (!deleted) allDeleted = false;
            }
        }
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);

    if (allDeleted) {
        for (int attempt = 0; attempt < 10; ++attempt) {
            if (RemoveDirectoryA(path.c_str())) return true;
            Sleep(50);
        }
    }
    return false;
#else
    return false;
#endif
}

static void Win32_DeleteDirectory(const std::string& path) {
    Win32_DeleteDirectoryInternal(path);
}

// bruteforce iwad/pwad stuff
static bool FileExists(const std::string& path) {
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

static bool HasExtension(const std::string& path, const std::string& ext) {
    if (path.length() < ext.length()) return false;
    std::string sub = path.substr(path.length() - ext.length());
    for (size_t i = 0; i < sub.length(); ++i) sub[i] = (char)tolower((unsigned char)sub[i]);
    return sub == ext;
}

void MainWindow::DetectStartupWads(int argc, char** argv) {
    if (argc <= 1 || !argv) return;

    std::string detectedIwad = "";
    std::vector<std::string> detectedPwads;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        // explicit -iwad flag
        if (arg == "-iwad" && i + 1 < argc) {
            detectedIwad = argv[++i];
            continue;
        }

        // explicit -file flag (1+ arguments)
        if (arg == "-file") {
            while (i + 1 < argc && argv[i + 1][0] != '-') {
                detectedPwads.push_back(argv[++i]);
            }
            continue;
        }

        // drag and drop into exe, probably fails right now
        if (arg[0] != '-') {
            if (HasExtension(arg, ".wad") || HasExtension(arg, ".deh") || HasExtension(arg, ".bex")) {
                // If it looks like a standard IWAD name and we have no IWAD yet, treat as IWAD
                std::string lowerArg = arg;
                for (char& c : lowerArg) c = (char)tolower((unsigned char)c);
                if (detectedIwad.empty() && 
                   (lowerArg.find("doom2.wad") != std::string::npos ||
                    lowerArg.find("doom.wad") != std::string::npos ||
                    lowerArg.find("plutonia.wad") != std::string::npos ||
                    lowerArg.find("tnt.wad") != std::string::npos)) {
                    detectedIwad = arg;
                } else {
                    detectedPwads.push_back(arg);
                }
            }
        }
    }

    // fallback search if nothing provided, probably fails right now
    if (detectedIwad.empty()) {
        const char* commonIwads[] = {
            "doom2.wad", "plutonia.wad", "tnt.wad", "doom.wad", "doom1.wad"
        };
        std::string exeDir = GetExeDirectory();

        for (const char* name : commonIwads) {
            std::string checkPath = exeDir + name;
            if (FileExists(checkPath)) {
                detectedIwad = checkPath;
                break;
            }
            if (FileExists(name)) {
                detectedIwad = name;
                break;
            }
        }

        // check DOOMWADDIR if present, probably fails right now
        if (detectedIwad.empty()) {
            char envBuf[MAX_PATH];
            DWORD res = GetEnvironmentVariableA("DOOMWADDIR", envBuf, MAX_PATH);
            if (res > 0 && res < MAX_PATH) {
                std::string envDir = envBuf;
                if (!envDir.empty() && envDir.back() != '\\' && envDir.back() != '/') envDir += "\\";
                for (const char* name : commonIwads) {
                    if (FileExists(envDir + name)) {
                        detectedIwad = envDir + name;
                        break;
                    }
                }
            }
        }
    }

    // populate brs input buffers if found
    if (!detectedIwad.empty()) {
        strncpy(m_brsIwadPath, detectedIwad.c_str(), sizeof(m_brsIwadPath) - 1);
        m_brsIwadPath[sizeof(m_brsIwadPath) - 1] = '\0';
    }

    if (!detectedPwads.empty()) {
        std::string pwadCombined = "";
        for (size_t p = 0; p < detectedPwads.size(); ++p) {
            // Quote paths in case they contain spaces
            pwadCombined += "\"" + detectedPwads[p] + "\"";
            if (p + 1 < detectedPwads.size()) pwadCombined += " ";
        }
        strncpy(m_brsPwadPath, pwadCombined.c_str(), sizeof(m_brsPwadPath) - 1);
        m_brsPwadPath[sizeof(m_brsPwadPath) - 1] = '\0';
    }
}

// ini functions
static std::unordered_map<std::string, std::string> s_iniData;

static void LoadIni(const std::string& filename) {
    s_iniData.clear();
    std::ifstream file(filename);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        auto sep = line.find('=');
        if (sep != std::string::npos) {
            std::string key = line.substr(0, sep);
            std::string val = line.substr(sep + 1);
            key.erase(key.find_last_not_of(" \t\r\n") + 1);
            val.erase(0, val.find_first_not_of(" \t\r\n"));
            val.erase(val.find_last_not_of(" \t\r\n") + 1);
            s_iniData[key] = val;
        }
    }
}

static void SaveIni(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) return;
    for (auto const& pair : s_iniData) {
        file << pair.first << "=" << pair.second << "\n";
    }
}

static std::string IniRead(const std::string& key, const std::string& def) {
    auto it = s_iniData.find(key);
    return (it != s_iniData.end()) ? it->second : def;
}

static void IniWrite(const std::string& key, const std::string& val) {
    s_iniData[key] = val;
}

static const int s_allowedComplevels[] = { 2, 3, 4, 9, 11, 21 };
static const char* s_complevelNames[] = {
    "2 - Doom v1.9",
    "3 - Ultimate Doom",
    "4 - Final Doom",
    "9 - Boom v2.02",
    "11 - MBF",
    "21 - MBF21"
};

// window core
MainWindow::MainWindow(int argc, char** argv) {
    Init();
    DetectStartupWads(argc, argv);
}

MainWindow::~MainWindow() {
    if (m_bruteThread.joinable()) {
        m_abortBrute = true;
        m_bruteThread.join();
    }
    WriteConfig();
}

void MainWindow::Init() {
    LoadConfig();
    xdre::setAngleType(m_selectedAngleType);
    xdre::setSavepointStyle(m_selectedSavepointStyle);

    if (xdre::getBlood() != m_bloodEnabled) {
        xdre::toggleBlood();
    }

    // default to hardware if nothing found in ini
    if (m_brsThreadGoal <= 0) {
        unsigned int hw = std::thread::hardware_concurrency();
        m_brsThreadGoal = (hw > 0) ? static_cast<int>(hw) : 8;
    }
	
	// populate iwad and pwad paths from internal tables
    XDRE_GetLoadedWads(m_brsIwadPath, sizeof(m_brsIwadPath), m_brsPwadPath, sizeof(m_brsPwadPath));

    // normalize separators to backslahes on wingdings
    NormalizePathSeparators(m_brsIwadPath);
    NormalizePathSeparators(m_brsPwadPath);
}

void MainWindow::RenderExitDialog() {
    if (m_showExitDialog) {
        ImGui::OpenPopup("Save Demo?");
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Save Demo?", &m_showExitDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Do you want to save the demo before quitting?\n\n");
        ImGui::Separator();

        if (ImGui::Button("Save & Exit", ImVec2(120, 0))) {
            if (SaveReplayFile()) {
                m_showExitDialog = false;
                m_running = false;
                ImGui::CloseCurrentPopup();
            } else {
                // keep app open if user presses cancel
                m_showExitDialog = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();

        if (ImGui::Button("Don't Save", ImVec2(120, 0))) {
            m_showExitDialog = false;
            m_running = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showExitDialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void MainWindow::SetKeys() {
    m_stringToCommandMap.clear();
    for (auto const& pair : m_configToStringMap) {
        if (pair.first == "Fire")                m_stringToCommandMap[pair.second] = XCmd::Fire;
        else if (pair.first == "Use")            m_stringToCommandMap[pair.second] = XCmd::Use;
        else if (pair.first == "TurnStepLeft")   m_stringToCommandMap[pair.second] = XCmd::TurnStepLeft;
        else if (pair.first == "TurnStepRight")  m_stringToCommandMap[pair.second] = XCmd::TurnStepRight;
        else if (pair.first == "TurnStep")       m_stringToCommandMap[pair.second] = XCmd::TurnStep;
        else if (pair.first == "PrevTic")        m_stringToCommandMap[pair.second] = XCmd::PrevTic;
        else if (pair.first == "NextTic")        m_stringToCommandMap[pair.second] = XCmd::NextTic;
        else if (pair.first == "CopyTic")        m_stringToCommandMap[pair.second] = XCmd::CopyTic;
        else if (pair.first == "DeleteTic")      m_stringToCommandMap[pair.second] = XCmd::DeleteTic;
        else if (pair.first == "NewTic")         m_stringToCommandMap[pair.second] = XCmd::NewTic;
        else if (pair.first == "ToggleBlood")    m_stringToCommandMap[pair.second] = XCmd::ToggleBlood;
        else if (pair.first == "SetSavepoint")   m_stringToCommandMap[pair.second] = XCmd::SetSavepoint;
        else if (pair.first == "UnsetSavepoint") m_stringToCommandMap[pair.second] = XCmd::UnsetSavepoint;
        else if (pair.first == "Load")           m_stringToCommandMap[pair.second] = XCmd::Load;
        else if (pair.first == "Save")           m_stringToCommandMap[pair.second] = XCmd::Save;
        else if (pair.first == "Exit")           m_stringToCommandMap[pair.second] = XCmd::Exit;
        else if (pair.first == "RunBwd")         m_stringToCommandMap[pair.second] = XCmd::RunBwd;
        else if (pair.first == "RunFwd")         m_stringToCommandMap[pair.second] = XCmd::RunFwd;
        else if (pair.first == "StrafeLeft")     m_stringToCommandMap[pair.second] = XCmd::StrafeLeft;
        else if (pair.first == "StrafeRight")    m_stringToCommandMap[pair.second] = XCmd::StrafeRight;
        else if (pair.first == "TurnLeft")       m_stringToCommandMap[pair.second] = XCmd::TurnLeft;
        else if (pair.first == "TurnRight")      m_stringToCommandMap[pair.second] = XCmd::TurnRight;
        else if (pair.first == "Weapon")         m_stringToCommandMap[pair.second] = XCmd::Weapon;
        else if (pair.first == "TransposeLeft")  m_stringToCommandMap[pair.second] = XCmd::TransposeLeft;
        else if (pair.first == "TransposeRight") m_stringToCommandMap[pair.second] = XCmd::TransposeRight;
        else if (pair.first == "CopyTicX")       m_stringToCommandMap[pair.second] = XCmd::CopyTicX;
        else if (pair.first == "DeleteTicX")     m_stringToCommandMap[pair.second] = XCmd::DeleteTicX;
        else if (pair.first == "BackwardDemo")   m_stringToCommandMap[pair.second] = XCmd::BackwardDemo;
        else if (pair.first == "ForwardDemo")    m_stringToCommandMap[pair.second] = XCmd::ForwardDemo;
        else if (pair.first == "ChangePlayer")   m_stringToCommandMap[pair.second] = XCmd::ChangePlayer;
    }
}

void MainWindow::LoadConfig() {
    std::string iniPath = GetExeDirectory() + "xdre.ini";
    LoadIni(iniPath);

    const char* keyNames[] = {
        "Fire", "Use", "TurnStepLeft", "TurnStepRight", "TurnStep",
        "PrevTic", "NextTic", "CopyTic", "DeleteTic", "NewTic",
        "ToggleBlood", "SetSavepoint", "UnsetSavepoint", "Load", "Save", "Exit",
        "RunBwd", "RunFwd", "StrafeLeft", "StrafeRight", "TurnLeft", "TurnRight",
        "Weapon", "TransposeLeft", "TransposeRight", "CopyTicX", "DeleteTicX",
        "BackwardDemo", "ForwardDemo", "ChangePlayer"
    };
    const char* defaults[] = {
        "f", "u", "-", "+", "ts",
        "j", "i", "c", "x", "n",
        "z", "ssp", "pss", "load", "save", "exit",
        "wq", "we", "sa", "sd", "re", "rt",
        "g", "tr", "ty", "tc", "tx",
        ",", ".", "v"
    };

    m_configToStringMap.clear();
    for (size_t i = 0; i < sizeof(keyNames)/sizeof(keyNames[0]); ++i) {
        m_configToStringMap[keyNames[i]] = IniRead(keyNames[i], defaults[i]);
    }

    m_selectedAngleType = std::stoi(IniRead("AngleType", "3"));
    m_selectedSavepointStyle = std::stoi(IniRead("SavepointMode", "1"));
    m_lastDirectory = IniRead("LastDirectory", ".");
    m_bloodEnabled = (std::stoi(IniRead("Blood", "0")) != 0);
	m_invertTicScrub = (std::stoi(IniRead("InvertTicScrub", "1")) != 0);
	m_brsAutoLoadResult = (std::stoi(IniRead("BRSAutoLoadResult", "1")) != 0);
	m_brsThreadGoal = std::stoi(IniRead("BRSThreadGoal", "0"));
	
    IniWrite("Blood", m_bloodEnabled ? "1" : "0");
	
	m_guiPosX = std::stoi(IniRead("GuiWindowX", "-1"));
    m_guiPosY = std::stoi(IniRead("GuiWindowY", "-1"));
    m_doomPosX = std::stoi(IniRead("DoomWindowX", "-1"));
    m_doomPosY = std::stoi(IniRead("DoomWindowY", "-1"));

    SetKeys();
}

void MainWindow::WriteConfig() {
    // write keybinds from active config map
    for (auto const& pair : m_configToStringMap) {
        IniWrite(pair.first, pair.second);
    }

    // write ui states
    IniWrite("AngleType", std::to_string(m_selectedAngleType));
    IniWrite("SavepointMode", std::to_string(m_selectedSavepointStyle));
    IniWrite("LastDirectory", m_lastDirectory);
    IniWrite("Blood", m_bloodEnabled ? "1" : "0");
	IniWrite("InvertTicScrub", m_invertTicScrub ? "1" : "0");
	IniWrite("BRSAutoLoadResult", m_brsAutoLoadResult ? "1" : "0");
	IniWrite("BRSThreadGoal", std::to_string(m_brsThreadGoal));
	
	IniWrite("GuiWindowX", std::to_string(m_guiPosX));
    IniWrite("GuiWindowY", std::to_string(m_guiPosY));
    IniWrite("DoomWindowX", std::to_string(m_doomPosX));
    IniWrite("DoomWindowY", std::to_string(m_doomPosY));

    std::string iniPath = GetExeDirectory() + "xdre.ini";
    SaveIni(iniPath);
}

void MainWindow::DoInput(XCmd cmd, unsigned int val) {
    switch (cmd) {
		case XCmd::None:                                         break;
        case XCmd::Fire:          xdre::toggleFire();            break;
        case XCmd::Use:           xdre::toggleUse();             break;
        case XCmd::TurnStepLeft:  xdre::turnStepLeft();          break;
        case XCmd::TurnStepRight: xdre::turnStepRight();         break;
        case XCmd::TurnStep:      xdre::setTurnStep(val);        break;
        case XCmd::PrevTic:       xdre::seekDemo(-1);            break;
        case XCmd::NextTic:       xdre::seekDemo(1);             break;
        case XCmd::CopyTic:       xdre::copyTic();               break;
        case XCmd::CopyTicX:      xdre::copyTicX(val);           break;
        case XCmd::DeleteTic:     xdre::deleteTic();             break;
        case XCmd::DeleteTicX:    xdre::deleteTicX(val);         break;
        case XCmd::NewTic:        xdre::newTic();                break;
        case XCmd::ToggleBlood:   xdre::toggleBlood();      m_bloodEnabled = xdre::getBlood();    break;
        case XCmd::SetSavepoint:  xdre::setSavepoint();          break;
        case XCmd::UnsetSavepoint:xdre::unsetSavepoint();        break;
        case XCmd::Load:          LoadReplayFile();              break;
        case XCmd::Save:          SaveReplayFile();              break;
        case XCmd::Exit:          RequestExit();                 break;
        case XCmd::RunBwd:  xdre::run(static_cast<signed char>(-std::abs(static_cast<int>(val)))); break;
        case XCmd::RunFwd:  xdre::run(static_cast<signed char>(val)); break;
        case XCmd::StrafeLeft:  xdre::strafe(static_cast<signed char>(-std::abs(static_cast<int>(val)))); break;
        case XCmd::StrafeRight: xdre::strafe(static_cast<signed char>(val)); break;
        case XCmd::TurnLeft:      xdre::turn(static_cast<signed short>((long)(int)val));          break;
        case XCmd::TurnRight:     xdre::turn(-static_cast<signed short>(std::abs((long)(int)val))); break;
        case XCmd::Weapon:        xdre::changeWeapon(static_cast<char>(val));   break;
        case XCmd::TransposeLeft: xdre::transpose(0, static_cast<char>(val));   break;
        case XCmd::TransposeRight:xdre::transpose(1, static_cast<char>(val));   break;
        case XCmd::BackwardDemo:  xdre::seekDemo(-static_cast<int>(val));       break;
        case XCmd::ForwardDemo:   xdre::seekDemo(static_cast<int>(val));        break;
        case XCmd::ChangePlayer:  xdre::changePlayerView(val);   break;
    }
}

void MainWindow::ProcessInputField(const char* inputStr) {
    std::string input(inputStr);
    if (input.empty()) return;

    // trim whitespace
    size_t first = input.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return;
    input = input.substr(first);

    std::string strCmd = input;
    long val = 0;
    bool hasNum = false;

    // find numeric value
    for (std::size_t i = 1; i < input.length(); ++i) {
        if (std::isdigit(static_cast<unsigned char>(input[i])) || input[i] == '-') {
            strCmd = input.substr(0, i);
            // trim space
            strCmd.erase(strCmd.find_last_not_of(" \t") + 1);

            try {
                val = std::stol(input.substr(i));
                hasNum = true;
            } catch (...) {
                return;
            }
            break;
        }
    }

    auto cmdIt = m_stringToCommandMap.find(strCmd);
    if (cmdIt == m_stringToCommandMap.end()) return;

    // cmds that require numeric arg must have one
    if (cmdIt->second >= XCmd::RunBwd && !hasNum) return;

    DoInput(cmdIt->second, static_cast<unsigned int>(val));
    m_inputBuffer[0] = '\0';
}

void MainWindow::Render() {
	// check if background worker queued a winning demo reload
    if (m_brsPendingReload.load()) {
        m_brsPendingReload = false;
        FILE* f = std::fopen(m_brsPendingReloadPath.c_str(), "rb");
        if (f) {
            xdre::load(f);
            std::fclose(f);

            // seek to original tic
            int currentTic = static_cast<int>(xdre::getCurrentTic());
            int maxListIndex = static_cast<int>(xdre::getTiclistSize()) - 1;
            int targetTic = (std::min)(m_brsInitialTic, maxListIndex);
            if (targetTic >= 0 && targetTic != currentTic) {
                xdre::seekDemo(targetTic - currentTic);
            }
        }
    }
	
    // fill the window with standard docking flags
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar | 
                                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | 
                                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("XDRE Main Host Window", nullptr, windowFlags);

    RenderMenuBar();

    ImGui::Columns(2, "MasterColumns", false);
    ImGui::SetColumnWidth(0, 240.0f);

    RenderTicList();

    ImGui::NextColumn();

    RenderTrackingPanels();

    ImGui::Columns(1);

    // external dialogs
    RenderHeaderDialog();
    RenderKeysDialog();
	RenderExitDialog();
    RenderBRSDialog();
	RenderOptionsDialog();
	
	ImGui::End();
}

void MainWindow::RenderMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Load...", "load")) LoadReplayFile();
            if (ImGui::MenuItem("Save...", "save")) SaveReplayFile();
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "exit")) RequestExit();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools")) {
            if (ImGui::MenuItem("Brute-force...")) {
                m_brsInitialTic = static_cast<int>(xdre::getCurrentTic());
				m_brsTargetTic  = m_brsInitialTic; // default target tic to current tic

                // ensure active options from ldre are refreshed
                xdre::getOptions(m_headerOptions);

                // temporary demo
                std::string tempDemo = GetExeDirectory() + "brs_temp_input.lmp";
                FILE* f = std::fopen(tempDemo.c_str(), "wb");
                if (f) {
                    xdre::save(f);
                    std::fclose(f);
                    std::strncpy(m_brsDemoPath, tempDemo.c_str(), sizeof(m_brsDemoPath) - 1);
                    m_brsDemoPath[sizeof(m_brsDemoPath) - 1] = '\0';
                }
                m_showBRSDialog = true;
            }
            if (ImGui::MenuItem("Edit Header...")) {
                xdre::getOptions(m_headerOptions);
                for (int i = 0; i < 6; ++i) {
                    if (s_allowedComplevels[i] == m_headerOptions[0]) {
                        m_headerComplevelIndex = i;
                        break;
                    }
                }
                m_showHeaderDialog = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Options")) {
			
			if (ImGui::MenuItem("Options...")) {
                m_bloodEditBuffer = xdre::getBlood();
				m_invertTicScrubEditBuffer = m_invertTicScrub;
				m_brsAutoLoadResultEditBuffer = m_brsAutoLoadResult;
                m_showOptionsDialog = true;
            }
			
            if (ImGui::MenuItem("Controls...")) {
                m_keyEditList.clear();
                const char* keyNames[] = {
                    "Fire", "Use", "TurnStepLeft", "TurnStepRight", "TurnStep",
                    "PrevTic", "NextTic", "CopyTic", "DeleteTic", "NewTic",
                    "ToggleBlood", "SetSavepoint", "UnsetSavepoint", "Load", "Save", "Exit",
                    "RunBwd", "RunFwd", "StrafeLeft", "StrafeRight", "TurnLeft", "TurnRight",
                    "Weapon", "TransposeLeft", "TransposeRight", "CopyTicX", "DeleteTicX",
                    "BackwardDemo", "ForwardDemo", "ChangePlayer"
                };
                for (const char* name : keyNames) {
                    KeyBindEntry entry;
                    entry.actionName = name;
                    std::string currentVal = m_configToStringMap[name];
                    std::memset(entry.buffer, 0, sizeof(entry.buffer));
                    std::strncpy(entry.buffer, currentVal.c_str(), sizeof(entry.buffer) - 1);
                    m_keyEditList.push_back(entry);
                }
                m_showKeysDialog = true;
            }

            if (ImGui::MenuItem("Clear All Traces")) {
                m_linedefHistory = {"0"};
                m_sectorHistory  = {"0"};
                m_thingHistory   = {"0"};
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

void MainWindow::RenderTicList() {
    ImGui::Text("Tic Commands");
    ImGui::Separator();

    int const totalTics = static_cast<int>(xdre::getTiclistSize());
    int const currentTic = static_cast<int>(xdre::getCurrentTic());

    ImVec2 outerSize(0.0f, -ImGui::GetFrameHeightWithSpacing());
    if (ImGui::BeginTable("TicTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, outerSize)) {
        
        // when m_invertTicScrub is true, normal wheel switches tics while rmb allows normal scroll
        bool const rmbDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
        bool const shouldScrubTics = m_invertTicScrub ? !rmbDown : rmbDown;

        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && shouldScrubTics) {
            ImGuiIO& io = ImGui::GetIO();
            if (io.MouseWheel != 0.0f) {
                int const stepTics = -static_cast<int>(io.MouseWheel);
                xdre::seekDemo(stepTics);
                io.MouseWheel = 0.0f; // consume wheel event
            }
        }

        ImGui::TableSetupColumn("Tic", ImGuiTableColumnFlags_WidthFixed, 46.0f);
        ImGui::TableSetupColumn("Command", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin(totalTics);

        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                ImGui::TableNextRow();
                bool const isSelected = (i == currentTic);

                if (isSelected) {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImVec4(0.25f, 0.45f, 0.75f, 0.70f)));
                }

                ImGui::TableSetColumnIndex(0);
                char numStr[16];
                snprintf(numStr, sizeof(numStr), "%d", i);
                if (ImGui::Selectable(numStr, isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
                    xdre::seekDemo(i - currentTic);
                }

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(xdre::getTicString(i).c_str());
            }
        }

        if (m_autoScrollTicList && m_lastRenderedTic != static_cast<unsigned int>(currentTic)) {
            float rowHeight = ImGui::GetTextLineHeightWithSpacing();
            ImGui::SetScrollY(currentTic * rowHeight - (ImGui::GetWindowHeight() * 0.5f));
            m_lastRenderedTic = static_cast<unsigned int>(currentTic);
        }

        ImGui::EndTable();
    }

    ImGui::Checkbox("Follow Tic", &m_autoScrollTicList);
}

void MainWindow::RenderTrackingPanels() {
    if (ImGui::BeginChild("TrackingScrollRegion", ImVec2(0, 0), false)) {
        RenderLinedefTracking();
        RenderSectorTracking();
        RenderThingTracking();
        RenderPlayerMovement();
        RenderGeneralSection();
    }
    ImGui::EndChild();
}

void MainWindow::RenderLinedefTracking() {
    if (ImGui::CollapsingHeader("Linedef Tracking", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushItemWidth(90.0f);

        // dropdown history
        std::string preview = (!m_linedefHistory.empty() && m_selectedLinedefIndex >= 0 && m_selectedLinedefIndex < (int)m_linedefHistory.size())
                              ? m_linedefHistory[m_selectedLinedefIndex] : m_linedefInput;

        if (ImGui::BeginCombo("##LinedefCombo", preview.c_str())) {
            for (int i = 0; i < (int)m_linedefHistory.size(); ++i) {
                bool isSelected = (m_selectedLinedefIndex == i);
                if (ImGui::Selectable(m_linedefHistory[i].c_str(), isSelected)) {
                    m_selectedLinedefIndex = i;
                    std::strncpy(m_linedefInput, m_linedefHistory[i].c_str(), sizeof(m_linedefInput) - 1);
                    m_linedefInput[sizeof(m_linedefInput) - 1] = '\0';
                    try {
                        xdre::setLinedefCheck(std::stoi(m_linedefInput));
                    } catch (...) {}
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();

        // direct input
        if (ImGui::InputText("Linedef ID", m_linedefInput, sizeof(m_linedefInput), ImGuiInputTextFlags_CharsDecimal)) {
            try {
                int val = std::stoi(m_linedefInput);
                xdre::setLinedefCheck(val);
            } catch (...) {}
        }
        ImGui::PopItemWidth();

        ImGui::SameLine();

        // add to list
        if (ImGui::Button("Add##Line")) {
            std::string newItem(m_linedefInput);
            if (!newItem.empty()) {
                auto it = std::find(m_linedefHistory.begin(), m_linedefHistory.end(), newItem);
                if (it == m_linedefHistory.end()) {
                    m_linedefHistory.push_back(newItem);
                    m_selectedLinedefIndex = (int)m_linedefHistory.size() - 1;
                } else {
                    m_selectedLinedefIndex = (int)std::distance(m_linedefHistory.begin(), it);
                }
            }
        }

        ImGui::Text("Crossed: %s", xdre::getLinedefCrossed() ? "True" : "False");
        ImGui::SameLine(180.0f);
        ImGui::Text("Dist: %.6f", xdre::getDistanceFromLine());

        ImGui::Text("V1: (%.0f, %.0f)", xdre::getLinedefVertex(0, 0), xdre::getLinedefVertex(0, 1));
        ImGui::SameLine(180.0f);
        ImGui::Text("V2: (%.0f, %.0f)", xdre::getLinedefVertex(1, 0), xdre::getLinedefVertex(1, 1));
        ImGui::Spacing();
    }
}

void MainWindow::RenderSectorTracking() {
    if (ImGui::CollapsingHeader("Sector Tracking", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushItemWidth(90.0f);

        // dropdown history
        std::string preview = (!m_sectorHistory.empty() && m_selectedSectorIndex >= 0 && m_selectedSectorIndex < (int)m_sectorHistory.size())
                              ? m_sectorHistory[m_selectedSectorIndex] : m_sectorInput;

        if (ImGui::BeginCombo("##SectorCombo", preview.c_str())) {
            for (int i = 0; i < (int)m_sectorHistory.size(); ++i) {
                bool isSelected = (m_selectedSectorIndex == i);
                if (ImGui::Selectable(m_sectorHistory[i].c_str(), isSelected)) {
                    m_selectedSectorIndex = i;
                    std::strncpy(m_sectorInput, m_sectorHistory[i].c_str(), sizeof(m_sectorInput) - 1);
                    m_sectorInput[sizeof(m_sectorInput) - 1] = '\0';
                    try {
                        xdre::setSectorCheck(std::stoi(m_sectorInput));
                    } catch (...) {}
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();

        // direct input
        if (ImGui::InputText("Sector ID", m_sectorInput, sizeof(m_sectorInput), ImGuiInputTextFlags_CharsDecimal)) {
            try {
                int val = std::stoi(m_sectorInput);
                xdre::setSectorCheck(val);
            } catch (...) {}
        }
        ImGui::PopItemWidth();

        ImGui::SameLine();

        // add to list
        if (ImGui::Button("Add##Sec")) {
            std::string newItem(m_sectorInput);
            if (!newItem.empty()) {
                auto it = std::find(m_sectorHistory.begin(), m_sectorHistory.end(), newItem);
                if (it == m_sectorHistory.end()) {
                    m_sectorHistory.push_back(newItem);
                    m_selectedSectorIndex = (int)m_sectorHistory.size() - 1;
                } else {
                    m_selectedSectorIndex = (int)std::distance(m_sectorHistory.begin(), it);
                }
            }
        }

        ImGui::Text("Floor: %.2f", xdre::getSectorInfo(0));
        ImGui::SameLine(140.0f);
        ImGui::Text("Ceiling: %.2f", xdre::getSectorInfo(1));
        ImGui::SameLine(280.0f);
        ImGui::Text("Special: %.0f", xdre::getSectorInfo(2));
        ImGui::Spacing();
    }
}

void MainWindow::RenderThingTracking() {
    if (ImGui::CollapsingHeader("Thing Tracking", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushItemWidth(90.0f);

        // dropdown history
        std::string preview = (!m_thingHistory.empty() && m_selectedThingIndex >= 0 && m_selectedThingIndex < (int)m_thingHistory.size())
                              ? m_thingHistory[m_selectedThingIndex] : m_thingInput;

        if (ImGui::BeginCombo("##ThingCombo", preview.c_str())) {
            for (int i = 0; i < (int)m_thingHistory.size(); ++i) {
                bool isSelected = (m_selectedThingIndex == i);
                if (ImGui::Selectable(m_thingHistory[i].c_str(), isSelected)) {
                    m_selectedThingIndex = i;
                    std::strncpy(m_thingInput, m_thingHistory[i].c_str(), sizeof(m_thingInput) - 1);
                    m_thingInput[sizeof(m_thingInput) - 1] = '\0';
                    try {
                        xdre::setThingCheck(std::stoi(m_thingInput));
                    } catch (...) {}
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();

        // direct input
        if (ImGui::InputText("Thing ID", m_thingInput, sizeof(m_thingInput), ImGuiInputTextFlags_CharsDecimal)) {
            try {
                int val = std::stoi(m_thingInput);
                xdre::setThingCheck(val);
            } catch (...) {}
        }
        ImGui::PopItemWidth();

        ImGui::SameLine();

        // add to list
        if (ImGui::Button("Add##Thg")) {
            std::string newItem(m_thingInput);
            if (!newItem.empty()) {
                auto it = std::find(m_thingHistory.begin(), m_thingHistory.end(), newItem);
                if (it == m_thingHistory.end()) {
                    m_thingHistory.push_back(newItem);
                    m_selectedThingIndex = (int)m_thingHistory.size() - 1;
                } else {
                    m_selectedThingIndex = (int)std::distance(m_thingHistory.begin(), it);
                }
            }
        }

        ImGui::Text("Pos: (%.4f, %.4f, %.2f)", xdre::getThingInfo(0), xdre::getThingInfo(1), xdre::getThingInfo(2));
        ImGui::Text("Radius: %.2f", xdre::getThingInfo(3));
        ImGui::SameLine(140.0f);
        ImGui::Text("Health: %.0f", xdre::getThingInfo(5));
        ImGui::SameLine(260.0f);
        ImGui::Text("Tics: %.0f", xdre::getThingInfo(4));

        ImGui::Text("Reaction: %.0f", xdre::getThingInfo(6));
        ImGui::SameLine(140.0f);
        ImGui::Text("Threshold: %.0f", xdre::getThingInfo(7));
        ImGui::Spacing();
    }
}

void MainWindow::RenderPlayerMovement() {
    if (ImGui::CollapsingHeader("Player Movement", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("PlayerMovementTable", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg)) {
            // headers
            ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Z", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableHeadersRow();

            // position
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted("Position");
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.6f", xdre::getXPos());
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.6f", xdre::getYPos());
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.2f", xdre::getZPos());

            // momentum
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted("Momentum");
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.6f", xdre::getXMom());
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.6f", xdre::getYMom());
            ImGui::TableSetColumnIndex(3);
            ImGui::TextDisabled("-");

            // component distances
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted("Distance traveled");
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.6f", xdre::getXDist());
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.6f", xdre::getYDist());
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.2f", xdre::getZDist());

            ImGui::EndTable();
        }

        ImGui::Spacing();

        // summary below columns
        ImGui::Text("Total Moved: %.6f", xdre::getDistanceMoved());
        ImGui::SameLine(180.0f);
        ImGui::Text("Direction: %.6f deg", xdre::getDirectionMoved());
        ImGui::Spacing();
    }
}

static bool s_needInputTextPurge = false;
static bool s_spaceCommandTriggered = false;

static int InputUnifiedCallback(ImGuiInputTextCallbackData* data) {
    MainWindow* window = (MainWindow*)data->UserData;

    // purge buffer only when not in filter and buffer has contents
    if (s_needInputTextPurge && data->EventFlag != ImGuiInputTextFlags_CallbackCharFilter) {
        if (data->BufTextLen > 0) {
            data->DeleteChars(0, data->BufTextLen);
        }
        s_needInputTextPurge = false;
        return 0;
    }

    // single char interceptor
    if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) {
        ImWchar c = data->EventChar;

        if (c == ' ') {
            s_spaceCommandTriggered = true;
            return 1; // discard space
        }

        // single char binding from user
        char keyStr[2] = { static_cast<char>(std::tolower(static_cast<unsigned char>(c))), '\0' };
        XCmd cmd = window->GetCommandForString(keyStr);

        // if char maps to non-numeric command, execute
        if (cmd != XCmd::None && cmd <= XCmd::Exit) {
            window->DoInput(cmd, 0);
            return 1; // discard char
        }

        return 0; // allow thru multichar arguments
    }

    // multichar completion
    if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit) {
        if (data->BufTextLen <= 0) return 0;

        std::string buf(data->Buf, data->BufTextLen);
        std::string lowerBuf = buf;
        for (char& ch : lowerBuf) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

        bool executed = false;

        // check word bindings
        XCmd cmd = window->GetCommandForString(lowerBuf);
        if (cmd != XCmd::None && cmd <= XCmd::Exit) {
            window->DoInput(cmd, 0);
            executed = true;
        }
        // weapon prefix
        else {
            const std::string& weaponKey = window->GetStringForConfig("Weapon");
            if (!weaponKey.empty() && lowerBuf.rfind(weaponKey, 0) == 0 && lowerBuf.length() == weaponKey.length() + 1) {
                char digit = lowerBuf.back();
                if (digit >= '0' && digit <= '7') {
                    unsigned int weaponVal = static_cast<unsigned int>(digit - '0');
                    window->DoInput(XCmd::Weapon, weaponVal);
                    executed = true;
                }
            }
            // player change prefix
            const std::string& playerKey = window->GetStringForConfig("ChangePlayer");
            if (!playerKey.empty() && lowerBuf.rfind(playerKey, 0) == 0 && lowerBuf.length() == playerKey.length() + 1) {
                char digit = lowerBuf.back();
                if (digit >= '1' && digit <= '4') {
                    unsigned int playerVal = static_cast<unsigned int>(digit - '0');
                    window->DoInput(XCmd::ChangePlayer, playerVal);
                    executed = true;
                }
            }
        }

        if (executed && data->BufTextLen > 0) {
            data->DeleteChars(0, data->BufTextLen);
        }
    }

    return 0;
}

void MainWindow::RenderGeneralSection() {
    if (ImGui::CollapsingHeader("General & Console", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Tic: %u", xdre::getCurrentTic());
        ImGui::SameLine(120.0f);
        ImGui::Text("Time: %.2f", xdre::getTime());
        ImGui::SameLine(240.0f);
        ImGui::Text("RNG: %d", xdre::getRngIndex());

        ImGui::Text("Damage: %d", xdre::getDoneDamage());
        ImGui::SameLine(120.0f);
        ImGui::Text("Used: %s", xdre::getUseSuccess() ? "True" : "False");
        ImGui::SameLine(240.0f);
        ImGui::Text("Save Tic: %u", xdre::getSavepointTic());
		
		// pass into getAngle
        unsigned short angleVal = xdre::getAngle(static_cast<short>(m_selectedAngleType));
        char angleStr[48];

        switch (m_selectedAngleType) {
            case 0: // longtics
                snprintf(angleStr, sizeof(angleStr), "%u (LT)", angleVal);
                break;
            case 1: // fine angles
                snprintf(angleStr, sizeof(angleStr), "%u (FA)", angleVal);
                break;
            case 2: // degrees
                // if getAngle(2) returns whole degrees, format as integer or convert to float
                snprintf(angleStr, sizeof(angleStr), "%u deg", angleVal);
                break;
            case 3: // byte angles
            default:
                snprintf(angleStr, sizeof(angleStr), "%u (BA)", angleVal);
                break;
        }

        ImGui::Text("Angle: %s", angleStr);

        ImGui::Separator();

        char const* angleTypes[] = { "Longtics", "Fine Angles", "Degrees", "Byte Angles" };
        ImGui::PushItemWidth(130.0f);
        if (ImGui::Combo("Angle Units", &m_selectedAngleType, angleTypes, IM_ARRAYSIZE(angleTypes))) {
            xdre::setAngleType(m_selectedAngleType);
            WriteConfig();
        }

        char const* savepointStyles[] = { "User", "Start", "Auto" };
        if (ImGui::Combo("Save Mode", &m_selectedSavepointStyle, savepointStyles, IM_ARRAYSIZE(savepointStyles))) {
            xdre::setSavepointStyle(m_selectedSavepointStyle);
        }
        ImGui::PopItemWidth();

        ImGui::Separator();
        ImGui::Text("Command Input:");
        ImGui::PushItemWidth(-1.0f);

        // enterreturns, update later
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_CallbackCharFilter | 
                                    ImGuiInputTextFlags_CallbackEdit | 
                                    ImGuiInputTextFlags_CallbackAlways |
                                    ImGuiInputTextFlags_EnterReturnsTrue;

        bool enterPressed = ImGui::InputText("##CommandLineInput", m_inputBuffer, sizeof(m_inputBuffer), flags, InputUnifiedCallback, this);

        bool submitCommand = (s_spaceCommandTriggered || enterPressed);
        s_spaceCommandTriggered = false;

        if (submitCommand) {
            if (m_inputBuffer[0] != '\0') {
                ProcessInputField(m_inputBuffer);
                s_needInputTextPurge = true;
                m_inputBuffer[0] = '\0';
            }
            ImGui::SetKeyboardFocusHere(-1);
        }

        ImGui::PopItemWidth();
		
    }
}

std::string MainWindow::OpenNativeFileDialog(bool isSave) {
    char filename[MAX_PATH] = "";

#ifdef _WIN32
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = "Doom Demo Files (*.lmp)\0*.lmp\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = m_lastDirectory.c_str();
    ofn.lpstrDefExt = "lmp";

    if (isSave) {
        ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
        if (GetSaveFileNameA(&ofn)) {
            std::string path(filename);
            size_t lastSlash = path.find_last_of("\\/");
            if (lastSlash != std::string::npos) {
                m_lastDirectory = path.substr(0, lastSlash);
                WriteConfig();
            }
            return path;
        }
    } else {
        ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetOpenFileNameA(&ofn)) {
            std::string path(filename);
            size_t lastSlash = path.find_last_of("\\/");
            if (lastSlash != std::string::npos) {
                m_lastDirectory = path.substr(0, lastSlash);
                WriteConfig();
            }
            return path;
        }
    }
#endif

    return "";
}

void MainWindow::LoadReplayFile() {
    std::string filepath = OpenNativeFileDialog(false);
    if (!filepath.empty()) {
        FILE* f = std::fopen(filepath.c_str(), "rb");
        if (f) {
            xdre::load(f);
            std::fclose(f);
        }
    }
}

bool MainWindow::SaveReplayFile() {
    std::string filepath = OpenNativeFileDialog(true);
    if (!filepath.empty()) {
        FILE* f = std::fopen(filepath.c_str(), "wb");
        if (f) {
            xdre::save(f);
            std::fclose(f);
            return true;
        }
    }
    return false;
}

void MainWindow::RenderHeaderDialog() {
    if (m_showHeaderDialog) {
        ImGui::OpenPopup("Header Editor");
        m_showHeaderDialog = false;
    }

    if (ImGui::BeginPopupModal("Header Editor", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (ImGui::Combo("Complevel", &m_headerComplevelIndex, s_complevelNames, 6)) {
            m_headerOptions[0] = s_allowedComplevels[m_headerComplevelIndex];
            xdre::changeOptions(m_headerOptions);
        }

        ImGui::InputInt("Skill", &m_headerOptions[1]);
        ImGui::InputInt("Episode", &m_headerOptions[2]);
        ImGui::InputInt("Map", &m_headerOptions[3]);

        const char* dmChoices[] = { "0 - SP / Co-op", "1 - Deathmatch", "2 - Altdeath" };
        ImGui::Combo("Deathmatch", &m_headerOptions[4], dmChoices, 3);

        ImGui::Separator();
        ImGui::Text("Players In Game:");
        bool p1 = (m_headerOptions[9] & 1) != 0;
        bool p2 = (m_headerOptions[9] & 2) != 0;
        bool p3 = (m_headerOptions[9] & 4) != 0;
        bool p4 = (m_headerOptions[9] & 8) != 0;

        if (ImGui::Checkbox("P1", &p1)) {
            m_headerOptions[9] ^= 1;
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("P2", &p2)) {
            m_headerOptions[9] ^= 2;
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("P3", &p3)) {
            m_headerOptions[9] ^= 4;
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("P4", &p4)) {
            m_headerOptions[9] ^= 8;
        }

        ImGui::Separator();
        auto BoolCheckbox = [](const char* label, int& val) {
            bool b = (val != 0);
            if (ImGui::Checkbox(label, &b)) val = b ? 1 : 0;
        };

        BoolCheckbox("Respawn", m_headerOptions[5]); ImGui::SameLine(180);
        BoolCheckbox("Fast Monsters", m_headerOptions[6]);
        BoolCheckbox("No Monsters", m_headerOptions[7]); ImGui::SameLine(180);
        BoolCheckbox("Longtics", m_headerOptions[27]);

        BoolCheckbox("Monsters Remember", m_headerOptions[10]); ImGui::SameLine(180);
        BoolCheckbox("Weapon Recoil", m_headerOptions[12]);
        BoolCheckbox("Allow Pushers", m_headerOptions[13]); ImGui::SameLine(180);
        BoolCheckbox("Player Bobbing", m_headerOptions[14]);
        BoolCheckbox("Demo Insurance", m_headerOptions[15]); ImGui::SameLine(180);
        BoolCheckbox("Monster Infighting", m_headerOptions[17]);
        BoolCheckbox("Monsters Backwards", m_headerOptions[20]); ImGui::SameLine(180);
        BoolCheckbox("Avoid Hazards", m_headerOptions[21]);
        BoolCheckbox("Monster Friction", m_headerOptions[22]); ImGui::SameLine(180);
        BoolCheckbox("Help Friends", m_headerOptions[23]);
        BoolCheckbox("Dog Jumping", m_headerOptions[24]); ImGui::SameLine(180);
        BoolCheckbox("Climb Steep Stairs", m_headerOptions[25]);

        ImGui::Separator();
        ImGui::InputInt("RNG Seed", &m_headerOptions[16]);
        ImGui::InputInt("Console Player", &m_headerOptions[8]);
        ImGui::InputInt("Variable Friction", &m_headerOptions[11]);
        ImGui::InputInt("Dogs (0-3)", &m_headerOptions[18]);
        ImGui::InputInt("Friend Distance", &m_headerOptions[19]);
        ImGui::InputInt("Compat Flags", &m_headerOptions[26]);

        ImGui::Separator();
        if (ImGui::Button("Apply Options", ImVec2(120, 0))) {
            xdre::changeOptions(m_headerOptions);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void MainWindow::RenderKeysDialog() {
    if (m_showKeysDialog) {
        ImGui::OpenPopup("Configure Keys");
        m_showKeysDialog = false;
    }

    if (ImGui::BeginPopupModal("Configure Keys", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Bind actions to text command shortcuts:");
        ImGui::Separator();

        ImGui::BeginChild("KeyScrollBox", ImVec2(520, 360), true);
        ImGui::Columns(3, "KeysColumns", true);

        for (size_t i = 0; i < m_keyEditList.size(); ++i) {
            auto& item = m_keyEditList[i];

            ImGui::Text("%s", item.actionName.c_str());
            ImGui::PushID(static_cast<int>(i));
            ImGui::SetNextItemWidth(70.0f);
            ImGui::InputText("##key", item.buffer, sizeof(item.buffer));
            ImGui::PopID();

            if ((i + 1) % 10 == 0) {
                ImGui::NextColumn();
            }
        }
        ImGui::Columns(1);
        ImGui::EndChild();

        ImGui::Separator();
        if (ImGui::Button("Save & Apply", ImVec2(140, 0))) {
            for (auto const& item : m_keyEditList) {
                std::string s(item.buffer);
                s.erase(0, s.find_first_not_of(" \t\r\n"));
                s.erase(s.find_last_not_of(" \t\r\n") + 1);
                for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

                if (!s.empty()) {
                    m_configToStringMap[item.actionName] = s;
                }
            }

            WriteConfig();
            SetKeys();

            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(140, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void MainWindow::RenderOptionsDialog() {
    if (m_showOptionsDialog) {
        ImGui::OpenPopup("Options");
        m_showOptionsDialog = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Options", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {

        ImGui::Checkbox("Palette stuff (blood/pickups)", &m_bloodEditBuffer);
        ImGui::Checkbox("Direct scrollwheel tic switching (hold RMB to scroll list)", &m_invertTicScrubEditBuffer);
        ImGui::Checkbox("Auto-load winning brute-force demo into editor", &m_brsAutoLoadResultEditBuffer);

        ImGui::Separator();
        if (ImGui::Button("Save & Apply", ImVec2(120, 0))) {
            m_bloodEnabled = m_bloodEditBuffer;
            m_invertTicScrub = m_invertTicScrubEditBuffer;
            m_brsAutoLoadResult = m_brsAutoLoadResultEditBuffer;

            if (xdre::getBlood() != m_bloodEnabled) {
                xdre::toggleBlood();
            }

            WriteConfig();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

int MainWindow::GetSelectedComplevel() const {
    int options[28] = { 0 };
    xdre::getOptions(options);

    // get complevel from options[0] directly
    for (int cl : s_allowedComplevels) {
        if (options[0] == cl) {
            return cl;
        }
    }

    // fallback to whatever header editor currently has
    for (int cl : s_allowedComplevels) {
        if (m_headerOptions[0] == cl) {
            return cl;
        }
    }

    return 2; // default to doom 2
}

void MainWindow::CleanupBRSArtifacts(int winningIndex) {
    {
        std::lock_guard<std::mutex> lock(m_brsWorkerMutex);
    }

    std::string bfDir = GetBFDirectory();
    std::string backupDir = GetBFBackupDirectory();

    // ensure backup dir exists
    Win32_CreateDirectory(backupDir);

    // if a winner's found, preserve it in bf_backup
    if (winningIndex >= 0) {
        std::string srcDemo = bfDir + "\\outputdemo" + std::to_string(winningIndex) + ".lmp";
        std::string dstDemo = backupDir + "\\outputdemo" + std::to_string(winningIndex) + ".lmp";

        // give filesystem a brief window to flush handles
        for (int retry = 0; retry < 5; ++retry) {
            if (CopyFileA(srcDemo.c_str(), dstDemo.c_str(), FALSE)) {
                break;
            }
            Sleep(20);
        }
    }

    // erase temp folder
    Win32_DeleteDirectory(bfDir);
}

static std::pair<int, int> BRS_ParseRange(const std::string& str, int minLimit, int maxLimit) {
    if (str.empty()) return {0, 0};
    auto colon = str.find(':');
    int low = 0, high = 0;
    try {
        if (colon == std::string::npos) {
            low = high = std::stoi(str);
        } else {
            low = std::stoi(str.substr(0, colon));
            high = std::stoi(str.substr(colon + 1));
        }
    } catch (...) {
        return {0, 0};
    }
    low = (std::max)(minLimit, (std::min)(maxLimit, low));
    high = (std::max)(minLimit, (std::min)(maxLimit, high));
    return {low, high};
}

// generate valid integer sizes for search dimensions
static std::vector<int> BRS_GetFactors(int rangeSize, int maxThreads) {
    std::vector<int> factors;
    if (rangeSize < 2) return factors;
    int limit = (std::min)(rangeSize, maxThreads);
    for (int i = 2; i <= limit; ++i) factors.push_back(i);
    return factors;
}

// factor combination backtracking
static bool BRS_Backtrack(const std::vector<std::vector<int>>& input,
                          std::vector<std::pair<int, int>>& current,
                          int index, int product, int target,
                          std::vector<std::pair<int, int>>& out) {
    if (product == target) {
        out = current;
        return true;
    }
    if (index >= (int)input.size() || product > target || target % product != 0)
        return false;

    if (BRS_Backtrack(input, current, index + 1, product, target, out))
        return true;

    for (int factor : input[index]) {
        if (product * factor <= target && (target % (product * factor) == 0)) {
            current.push_back({factor, index});
            if (BRS_Backtrack(input, current, index + 1, product * factor, target, out))
                return true;
            current.pop_back();
        }
    }
    return false;
}

void MainWindow::StartBRSSearch() {
    if (m_brsIsRunning) return;
    if (m_brsTics.empty()) return;

    if (m_brsSupervisorThread.joinable())
        m_brsSupervisorThread.join();

    m_brsAbortRequested = false;
    m_brsIsRunning = true;
    m_brsCompletedTasks = 0;
    m_brsWinningTaskIndex = -1;
    m_brsStatusMessage = "Partitioning ranges...";
    m_brsResultDetails = "";

    // prepare range factors
    for (auto& tic : m_brsTics) {
        auto r = BRS_ParseRange(tic.runStr, -50, 50);
        auto s = BRS_ParseRange(tic.strafeStr, -50, 50);
        auto t = BRS_ParseRange(tic.turnStr, -128, 127);
        tic.runSize = std::abs(r.first - r.second) + 1;
        tic.strafeSize = std::abs(s.first - s.second) + 1;
        tic.turnSize = std::abs(t.first - t.second) + 1;
        tic.runFactors = BRS_GetFactors(tic.runSize, m_brsThreadGoal * 4);
        tic.strafeFactors = BRS_GetFactors(tic.strafeSize, m_brsThreadGoal * 4);
        tic.turnFactors = BRS_GetFactors(tic.turnSize, m_brsThreadGoal * 4);
    }

    m_brsSupervisorThread = std::thread(&MainWindow::BRSWorkerThread, this);
}

void MainWindow::AbortBRSSearch() {
    m_brsAbortRequested = true;
    m_brsStatusMessage = "Aborting search...";
}

// produce sub-slices for each dimension using integer division and remainder distribution
static std::vector<std::vector<std::string>> BRS_ProduceRangeIncrements(
    const std::vector<std::pair<int, int>>& winningPairs,
    const std::vector<BRSTicItem>& tics) {

    std::vector<int> lowerVals, rangeSizes;
    for (const auto& tic : tics) {
        auto r = BRS_ParseRange(tic.runStr, -128, 127);
        auto s = BRS_ParseRange(tic.strafeStr, -128, 127);
        auto t = BRS_ParseRange(tic.turnStr, -128, 127);

        lowerVals.push_back((std::min)(r.first, r.second));
        lowerVals.push_back((std::min)(s.first, s.second));
        lowerVals.push_back((std::min)(t.first, t.second));

        rangeSizes.push_back(std::abs(r.first - r.second) + 1);
        rangeSizes.push_back(std::abs(s.first - s.second) + 1);
        rangeSizes.push_back(std::abs(t.first - t.second) + 1);
    }

    std::vector<int> splitFactors(rangeSizes.size(), 1);
    for (const auto& pair : winningPairs) {
        if (pair.second >= 0 && pair.second < (int)splitFactors.size())
            splitFactors[pair.second] = pair.first;
    }

    std::vector<std::vector<std::string>> increments(rangeSizes.size());
    for (size_t i = 0; i < rangeSizes.size(); ++i) {
        int factor = splitFactors[i];
        int rSize  = rangeSizes[i];
        int lVal   = lowerVals[i];

        if (factor > 1) {
            int q = rSize / factor;
            int rem = rSize % factor;
            int curr = lVal;

            for (int j = 0; j < factor; ++j) {
                int size = (j < rem) ? (q + 1) : q;
                int startVal = curr;
                int endVal   = curr + size - 1;
                increments[i].push_back(std::to_string(startVal) + ":" + std::to_string(endVal));
                curr += size;
            }
        } else {
            increments[i].push_back(std::to_string(lVal) + ":" + std::to_string(lVal + rSize - 1));
        }
    }

    return increments;
}

// generate unique combination array for each task
static std::vector<std::vector<std::string>> BRS_ProduceFinalCombinations(
    const std::vector<std::vector<std::string>>& increments,
    int taskCount) {

    std::vector<std::vector<std::string>> finalCombinations;
    std::vector<int> indices(increments.size(), 0);

    for (int i = 0; i < taskCount; ++i) {
        std::vector<std::string> combo;
        for (size_t j = 0; j < increments.size(); ++j) {
            combo.push_back(increments[j][indices[j]]);
        }
        finalCombinations.push_back(combo);

        for (size_t j = 0; j < increments.size(); ++j) {
            indices[j]++;
            if (indices[j] < (int)increments[j].size())
                break;
            indices[j] = 0;
        }
    }

    return finalCombinations;
}

void MainWindow::BRSWorkerThread() {
    // force headless sdl drivers for dsda processes
    SetEnvironmentVariableA("SDL_VIDEODRIVER", "dummy");
    SetEnvironmentVariableA("SDL_AUDIODRIVER", "dummy");

    // earliest tic first
    auto ticsAscending = m_brsTics;
    std::sort(ticsAscending.begin(), ticsAscending.end(), [](const BRSTicItem& a, const BRSTicItem& b) {
        return a.ticNumber < b.ticNumber;
    });

    // factor tree order, run/strafe/turn
    std::vector<std::vector<int>> factorTree;
    for (const auto& tic : ticsAscending) {
        factorTree.push_back(tic.runFactors);
        factorTree.push_back(tic.strafeFactors);
        factorTree.push_back(tic.turnFactors);
    }

    int numWorkers = (std::max)(m_brsThreadGoal, 1);

    // combination calculator
    long long totalCombinations = 1;
    for (const auto& tic : ticsAscending) {
    totalCombinations *= (std::max)(1, tic.runSize);
    totalCombinations *= (std::max)(1, tic.strafeSize);
    totalCombinations *= (std::max)(1, tic.turnSize);
    }

    int targetMultiplier = 16;
    if (totalCombinations < static_cast<long long>(numWorkers) * targetMultiplier) {
    targetMultiplier = (std::max)(1, static_cast<int>(totalCombinations / numWorkers));
    }
    int desiredTarget = numWorkers * targetMultiplier;

    int bestFoundSlices = 1;
    std::vector<std::pair<int, int>> cur, winningPairs;

    bool matchFound = false;
    for (int mult = targetMultiplier; mult >= 1; --mult) {
        int baseCand = numWorkers * mult;
        for (int delta = 0; delta <= 2; ++delta) {
            int testSlices = baseCand - delta;
            if (testSlices < 1) continue;

            cur.clear();
            if (BRS_Backtrack(factorTree, cur, 0, 1, testSlices, winningPairs)) {
                bestFoundSlices = testSlices;
                matchFound = true;
                break;
            }
        }
        if (matchFound) break;
    }

    if (!matchFound) {
        // controlled step-down with <= 2 max
        for (int t = desiredTarget; t >= 1; --t) {
            int rem = t % numWorkers;
            if (rem == 0 || rem <= 2 || (numWorkers - rem) <= 2) {
                cur.clear();
                if (BRS_Backtrack(factorTree, cur, 0, 1, t, winningPairs)) {
                    bestFoundSlices = t;
                    break;
                }
            }
        }
    }

    auto increments = BRS_ProduceRangeIncrements(winningPairs, ticsAscending);
    auto finalCombinations = BRS_ProduceFinalCombinations(increments, bestFoundSlices);

    int complevel = GetSelectedComplevel();
    int baseTic   = ticsAscending.empty() ? 0 : ticsAscending.front().ticNumber;
    int endTic    = ticsAscending.empty() ? 0 : ticsAscending.back().ticNumber;
	
	// fetch active map and episode from ldre
    int currentOptions[28] = { 0 };
    xdre::getOptions(currentOptions);
    int curEpisode = currentOptions[2];
    int curMap     = currentOptions[3];
    if (curMap <= 0) curMap = 1;

    int levelStartTic = static_cast<int>(xdre::getLevelStartTic());
    int localSkipTarget = 0;

    if (levelStartTic > 0 && baseTic >= levelStartTic) {
        // multilevel run: levelStartTic was set at leveltime == 1
        localSkipTarget = (std::max)(0, baseTic - levelStartTic);
    } else {
        // first map or il demo: standard zero indexed baseline
        localSkipTarget = (std::max)(0, baseTic - 1);
    }

    std::string bfDir = GetBFDirectory();
    Win32_CreateDirectory(bfDir);

    int actualWorkers = (std::min)(numWorkers, (int)finalCombinations.size());

    // create queue files in bf
    std::vector<std::ofstream> queueFiles(actualWorkers);
    for (int w = 0; w < actualWorkers; ++w) {
        std::string qPath = bfDir + "/brs_queue_" + std::to_string(w) + ".txt";
        queueFiles[w].open(qPath, std::ios::out | std::ios::trunc);
    }

    // output slices matching frame: offset run strafe turn flags weapon
    for (size_t i = 0; i < finalCombinations.size(); ++i) {
        int targetWorker = static_cast<int>(i % actualWorkers);
        std::string sliceLine = "";
        size_t comboIdx = 0;

        for (size_t f = 0; f < ticsAscending.size(); ++f) {
            int frameOffset = ticsAscending[f].ticNumber - baseTic;
            std::string runSlice    = finalCombinations[i][comboIdx++];
            std::string strafeSlice = finalCombinations[i][comboIdx++];
            std::string turnSlice   = finalCombinations[i][comboIdx++];

            sliceLine += "bf.frame " + std::to_string(frameOffset) + " "
                      + runSlice + " " + strafeSlice + " " + turnSlice;

            // combine into single token
            std::string flags = "";
            if (ticsAscending[f].fire == 1) flags += "a";
            if (ticsAscending[f].use == 1)  flags += "u";

            if (!flags.empty()) {
                sliceLine += " " + flags;
            }

            if (ticsAscending[f].weapon > 0) {
                sliceLine += " c" + std::to_string(ticsAscending[f].weapon);
            }

            // only append semicolon if another frame following in this line
            if (f + 1 < ticsAscending.size()) {
                sliceLine += "; ";
            }
        }
        queueFiles[targetWorker] << sliceLine << "\n";
    }

    for (int w = 0; w < actualWorkers; ++w) {
        queueFiles[w].close();
    }

    // build parameter string
    std::vector<std::string> condTokens;
    auto AppendCond = [&](const BRSCheckItem& c, const char* name) {
        if (!c.active) return;
        const char* opStr = "==";
        if (c.op == 1) opStr = "!=";
        else if (c.op == 2) opStr = ">=";
        else if (c.op == 3) opStr = ">";
        else if (c.op == 4) opStr = "<=";
        else if (c.op == 5) opStr = "<";
        else if (c.op == 6) opStr = "min";
        else if (c.op == 7) opStr = "max";

        if (c.op == 6 || c.op == 7) {
            condTokens.push_back(std::string(name) + " " + opStr);
        } else {
            condTokens.push_back(std::string(name) + " " + opStr + " " + c.val);
        }
    };

    // condition build
    AppendCond(m_chkSpeed, "spd");
    AppendCond(m_chkDamage, "dmg");
    AppendCond(m_chkX, "x");
    AppendCond(m_chkY, "y");
    AppendCond(m_chkZ, "z");
    AppendCond(m_chkMomX, "vx");
    AppendCond(m_chkMomY, "vy");
	AppendCond(m_chkUse, "use");

    // append linedef condition
    if (m_chkLinedef.active && strlen(m_chkLinedef.val) > 0) {
        std::string lType = (m_chkLinedefType == 0) ? "act " : "skip ";
        condTokens.push_back(lType + std::string(m_chkLinedef.val));
    }

    std::string condStr = "";
    for (size_t c = 0; c < condTokens.size(); ++c) {
        condStr += condTokens[c];
        if (c + 1 < condTokens.size()) condStr += ", ";
    }

    // safety fallback if ""
    if (condStr.empty()) {
        // if a linedef was typed in tracer panel, assume we want activation
        int trackedLine = -1;
        try { trackedLine = std::stoi(m_linedefInput); } catch (...) {}
        if (trackedLine > 0) {
            condStr = "act " + std::to_string(trackedLine);
        } else {
            // otherwise provide max speed
            condStr = "spd max";
        }
    }

    // assemble task cmds
    std::vector<BRSTask> taskQueue;
    for (int w = 0; w < actualWorkers; ++w) {
        // resolve absolute path
        std::string exePath = m_brsExePath;
        if (exePath.find(":\\") == std::string::npos && exePath.find(":/") == std::string::npos) {
            exePath = GetExeDirectory() + exePath;
        }

        std::string cmd = "\"" + exePath + "\" ";

        if (strlen(m_brsIwadPath) > 0) {
            std::string iwad = m_brsIwadPath;
            if (iwad.find(":\\") == std::string::npos && iwad.find(":/") == std::string::npos)
                iwad = GetExeDirectory() + iwad;
            cmd += "-iwad \"" + iwad + "\" ";
        }
        if (strlen(m_brsPwadPath) > 0) {
            std::string pwadStr = m_brsPwadPath;
            // pass directly after -file
            if (pwadStr.front() == '"' || pwadStr.find(' ') != std::string::npos) {
                cmd += "-file " + pwadStr + " ";
            } else {
                if (pwadStr.find(":\\") == std::string::npos && pwadStr.find(":/") == std::string::npos)
                    pwadStr = GetExeDirectory() + pwadStr;
                cmd += "-file \"" + pwadStr + "\" ";
            }
        }
        if (strlen(m_brsExtraArgs) > 0) cmd += std::string(m_brsExtraArgs) + " ";

        // outputdemo and queue will be local names
        cmd += "-recordfromto \"" + std::string(m_brsDemoPath) + "\" \"outputdemo" + std::to_string(w) + ".lmp\" ";
		
		// append warp if not already present in extra args
        if (std::string(m_brsExtraArgs).find("-warp") == std::string::npos) {
            if (complevel == 3) {
                cmd += "-warp " + std::to_string(curEpisode > 0 ? curEpisode : 1) + " " + std::to_string(curMap) + " ";
            } else {
                cmd += "-warp " + std::to_string(curMap) + " ";
            }
        }

        // use local map skiptic
        cmd += "-skiptic " + std::to_string(localSkipTarget) + " ";

        if (std::string(m_brsExtraArgs).find("-complevel") == std::string::npos &&
            std::string(m_brsExtraArgs).find("-cl") == std::string::npos) {
            cmd += "-complevel " + std::to_string(complevel) + " ";
        }

        cmd += "-bf_queue \"brs_queue_" + std::to_string(w) + ".txt\" ";
        cmd += "-quit_after_brute_force -build -command \"demo.join; ";

        if (m_brsNomo) cmd += "bf.nomo; ";

        // target tic for conditions
        int targetEvalTic = (std::max)(endTic, m_brsTargetTic);

        // must run through target tic
        int maxTargetTic = (std::max)(endTic, targetEvalTic);
        int totalDepth = (maxTargetTic - baseTic) + 1;

        // preserve every tic basetic to maxtargettic that wasn't modulated
        for (int t = baseTic; t <= maxTargetTic; ++t) {
            bool isModulated = false;
            for (const auto& item : ticsAscending) {
                if (item.ticNumber == t) {
                    isModulated = true;
                    break;
                }
            }
            if (!isModulated) {
                cmd += "bf.keep " + std::to_string(t - baseTic) + "; ";
            }
        }

        cmd += "bf.start " + std::to_string(totalDepth) + " " + condStr + "\"";

        taskQueue.push_back({w, cmd});
    }

    bool isMinSearch = false;
    bool isMaxSearch = false;
    for (const auto* item : { &m_chkSpeed, &m_chkDamage, &m_chkX, &m_chkY, &m_chkZ, &m_chkMomX, &m_chkMomY }) {
        if (item->active) {
            if (item->op == 6) isMinSearch = true;
            if (item->op == 7) isMaxSearch = true;
        }
    }
    bool isOptimization = (isMinSearch || isMaxSearch);

    double bestTargetValue = isMinSearch ? (std::numeric_limits<double>::max)()
                                         : -(std::numeric_limits<double>::max)();

    m_brsTotalTasks = (int)taskQueue.size();
    int concurrentLimit = (std::min)(actualWorkers, m_brsTotalTasks.load());
    std::vector<BRSWorker> activeWorkers;

    // supervise execution
    while ((!taskQueue.empty() || !activeWorkers.empty()) && !m_brsAbortRequested) {
        while ((int)activeWorkers.size() < concurrentLimit && !taskQueue.empty() && !m_brsAbortRequested) {
            BRSTask task = taskQueue.back();
            taskQueue.pop_back();

            std::string logName = bfDir + "/process_output" + std::to_string(task.taskIndex) + ".txt";
            SECURITY_ATTRIBUTES sa;
            sa.nLength = sizeof(sa);
            sa.bInheritHandle = TRUE;
            sa.lpSecurityDescriptor = NULL;

            HANDLE hLog = CreateFileA(logName.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

            STARTUPINFOA si;
            PROCESS_INFORMATION pi;
            ZeroMemory(&si, sizeof(si));
            ZeroMemory(&pi, sizeof(pi));
            si.cb = sizeof(si);
            si.dwFlags |= STARTF_USESTDHANDLES;
            si.hStdOutput = hLog;
            si.hStdError = hLog;
            si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

            std::vector<char> cmdBuf(task.command.begin(), task.command.end());
            cmdBuf.push_back('\0');

            // pass bfDir.c_str() as lpCurrentDirectory so dsda runs inside bf
            if (CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, bfDir.c_str(), &si, &pi)) {
                activeWorkers.push_back({task.taskIndex, pi.hProcess, pi.hThread, pi.dwProcessId});
            }
            CloseHandle(hLog);
        }

        for (size_t i = 0; i < activeWorkers.size(); ) {
            DWORD exitCode = 0;
            if (GetExitCodeProcess(activeWorkers[i].hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
                int taskIdx = activeWorkers[i].taskIndex;
                CloseHandle(activeWorkers[i].hProcess);
                CloseHandle(activeWorkers[i].hThread);
                activeWorkers.erase(activeWorkers.begin() + i);
                m_brsCompletedTasks++;

                std::string logFile = bfDir + "/process_output" + std::to_string(taskIdx) + ".txt";
                std::ifstream f(logFile);
                std::string line;
                bool taskSucceeded = false;
                double taskBestVal = isMinSearch ? (std::numeric_limits<double>::max)()
                                                 : -(std::numeric_limits<double>::max)();
                bool foundCandidate = false;

                while (std::getline(f, line)) {
                    auto pos = line.find("New best:");
                    if (pos != std::string::npos) {
                        taskSucceeded = true;
                        auto eqPos = line.find('=', pos);
                        if (eqPos != std::string::npos) {
                            try {
                                double val = std::stod(line.substr(eqPos + 1));
                                if (isMinSearch && val < taskBestVal) {
                                    taskBestVal = val;
                                    foundCandidate = true;
                                } else if (isMaxSearch && val > taskBestVal) {
                                    taskBestVal = val;
                                    foundCandidate = true;
                                }
                            } catch (...) {}
                        }
                    } else if (line.find("complete (SUCCESS)") != std::string::npos) {
                        taskSucceeded = true;
                    }
                }

                if (isOptimization) {
                    if (foundCandidate) {
                        bool isNewGlobalBest = false;
                        if (isMinSearch && taskBestVal < bestTargetValue) isNewGlobalBest = true;
                        if (isMaxSearch && taskBestVal > bestTargetValue) isNewGlobalBest = true;

                        if (isNewGlobalBest) {
                            bestTargetValue = taskBestVal;
                            m_brsWinningTaskIndex = taskIdx;
                            m_brsResultDetails = "Best so far: " + std::to_string(bestTargetValue) + " (worker " + std::to_string(taskIdx) + ")\n";
                        }
                    }
                } else {
                    if (taskSucceeded && m_brsWinningTaskIndex < 0) {
                        m_brsWinningTaskIndex = taskIdx;
                        m_brsStatusMessage = "Target reached in worker " + std::to_string(taskIdx);

                        // discard remaining tasks
                        taskQueue.clear();

                        // kill all remaining workers
                        for (auto& w : activeWorkers) {
                            TerminateProcess(w.hProcess, 0);
                            WaitForSingleObject(w.hProcess, 500);
                            CloseHandle(w.hProcess);
                            CloseHandle(w.hThread);
                        }
                        activeWorkers.clear();

                        // exit loop
                        break;
                    }
                }
            } else {
                ++i;
            }
        }
        
        // break outer dispatch loop if match was found
        if (!isOptimization && m_brsWinningTaskIndex >= 0) {
            break;
        }

        Sleep(20);
    }

    // terminate and wait for all remaining workers to release file locks
    for (auto& w : activeWorkers) {
        TerminateProcess(w.hProcess, 0);
        WaitForSingleObject(w.hProcess, 2000); // wait up to 2s for OS teardown
        CloseHandle(w.hProcess);
        CloseHandle(w.hThread);
    }
    activeWorkers.clear();

    if (m_brsAbortRequested) {
        CleanupBRSArtifacts(-1);
        m_brsStatusMessage = "Search aborted by user.";
    } else {
        CleanupBRSArtifacts(m_brsWinningTaskIndex);
        if (m_brsWinningTaskIndex >= 0) {
            std::string finalDemoPath = GetBFBackupDirectory() + "\\outputdemo" + std::to_string(m_brsWinningTaskIndex) + ".lmp";
            m_brsStatusMessage = "Search complete: solution found (copied to " + finalDemoPath + ").";

            if (m_brsAutoLoadResult) {
                m_brsPendingReloadPath = finalDemoPath;
                m_brsPendingReload = true;
            }
        } else {
            m_brsStatusMessage = "Search finished: no matching condition found.";
        }
    }

    SetEnvironmentVariableA("SDL_VIDEODRIVER", NULL);
    SetEnvironmentVariableA("SDL_AUDIODRIVER", NULL);

    m_brsIsRunning = false;
}

static void BRS_CalcAutoRange(int centerVal, int rangeRadius, int minBound, int maxBound, char* outBuf, size_t bufSize) {
    int fromVal = centerVal + rangeRadius;
    int toVal = centerVal - rangeRadius;

    // determine min/max
    int high = (std::max)(fromVal, toVal);
    int low  = (std::min)(fromVal, toVal);

    int offset = 0;
    if (high > maxBound) {
        offset = high - maxBound;
    } else if (low < minBound) {
        offset = low - minBound;
    }

    fromVal -= offset;
    toVal   -= offset;

    // safety clamp range diameter
    fromVal = (std::max)(minBound, (std::min)(maxBound, fromVal));
    toVal   = (std::max)(minBound, (std::min)(maxBound, toVal));

    snprintf(outBuf, bufSize, "%d:%d", fromVal, toVal);
}

void MainWindow::RenderBRSDialog() {
    if (m_showBRSDialog) {
        ImGui::OpenPopup("Brute-force");
        m_showBRSDialog = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(676.0f, 0), ImGuiCond_Always);

    if (ImGui::BeginPopupModal("Brute-force", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        
        // top section
        ImGui::Columns(2, "BRSTopSplit", true);
        ImGui::SetColumnWidth(0, 380.0f);

        // left column
        ImGui::Text("Tics to Brute-force");
        ImGui::Separator();

        if (ImGui::Button("Add Tic")) {
            BRSTicItem item;

            int startTic = static_cast<int>(xdre::getCurrentTic());
            int maxTic = static_cast<int>(xdre::getTiclistSize()) - 1;
            if (maxTic < 0) maxTic = 0;
            startTic = (std::max)(0, (std::min)(startTic, maxTic));

            if (m_brsTics.empty()) {
                item.ticNumber = startTic;
            } else {
                int candidate = startTic;
                for (; candidate >= 0; --candidate) {
                    bool exists = false;
                    for (const auto& t : m_brsTics) {
                        if (t.ticNumber == candidate) {
                            exists = true;
                            break;
                        }
                    }
                    if (!exists) break;
                }

                if (candidate < 0) {
                    for (candidate = startTic + 1; candidate <= maxTic; ++candidate) {
                        bool exists = false;
                        for (const auto& t : m_brsTics) {
                            if (t.ticNumber == candidate) {
                                exists = true;
                                break;
                            }
                        }
                        if (!exists) break;
                    }
                }

                item.ticNumber = (candidate >= 0 && candidate <= maxTic) ? candidate : 0;
            }

            m_brsTics.push_back(item);

            std::sort(m_brsTics.begin(), m_brsTics.end(), [](const BRSTicItem& a, const BRSTicItem& b) {
                return a.ticNumber > b.ticNumber;
            });

            for (int i = 0; i < static_cast<int>(m_brsTics.size()); ++i) {
                if (m_brsTics[i].ticNumber == item.ticNumber) {
                    m_selectedBRSTic = i;
                    break;
                }
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Remove") && m_selectedBRSTic >= 0 && m_selectedBRSTic < (int)m_brsTics.size()) {
            m_brsTics.erase(m_brsTics.begin() + m_selectedBRSTic);
            if (m_selectedBRSTic >= (int)m_brsTics.size()) m_selectedBRSTic = (int)m_brsTics.size() - 1;
        }

        // tic list
        ImGui::BeginChild("BRSTicList", ImVec2(0, 115), true);
        for (int i = 0; i < (int)m_brsTics.size(); ++i) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Tic %d", m_brsTics[i].ticNumber);
            if (ImGui::Selectable(buf, m_selectedBRSTic == i)) {
                m_selectedBRSTic = i;
            }
        }
        ImGui::EndChild();

        // selected tic attributes
        if (m_selectedBRSTic >= 0 && m_selectedBRSTic < (int)m_brsTics.size()) {
            auto& tic = m_brsTics[m_selectedBRSTic];

            ImGui::Text("Tic Index:");
            ImGui::SameLine(75.0f);
            ImGui::SetNextItemWidth(70.0f);
            ImGui::InputInt("##ticNumber", &tic.ticNumber);

            // run and strafe
            ImGui::Text("Run:");
            ImGui::SameLine(50.0f);
            ImGui::SetNextItemWidth(90.0f);
            ImGui::InputText("##runStr", tic.runStr, sizeof(tic.runStr));

            ImGui::SameLine(175.0f);
            ImGui::Text("Strafe:");
            ImGui::SameLine(230.0f);
            ImGui::SetNextItemWidth(90.0f);
            ImGui::InputText("##strafeStr", tic.strafeStr, sizeof(tic.strafeStr));

            // turn
            ImGui::Text("Turn:");
            ImGui::SameLine(50.0f);
            ImGui::SetNextItemWidth(90.0f);
            ImGui::InputText("##turnStr", tic.turnStr, sizeof(tic.turnStr));

            // fire and use
            const char* triChoices[] = {"No", "Yes"};
            ImGui::Text("Fire:");
            ImGui::SameLine(50.0f);
            ImGui::SetNextItemWidth(70.0f);
            ImGui::Combo("##fire", &tic.fire, triChoices, 2);

            ImGui::SameLine(175.0f);
            ImGui::Text("Use:");
            ImGui::SameLine(230.0f);
            ImGui::SetNextItemWidth(70.0f);
            ImGui::Combo("##use", &tic.use, triChoices, 2);
        } else {
            ImGui::TextDisabled("Select a tic above to configure its ranges.");
        }

        // right column
        ImGui::NextColumn();
        ImGui::Text("Conditions to match on tic:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        ImGui::InputInt("##targetTicInput", &m_brsTargetTic, 0, 0);
        ImGui::Separator();

        const char* ops[] = {"=", "!=", ">=", ">", "<=", "<", "Min", "Max"};
        auto RenderCheckRow = [&](const char* label, BRSCheckItem& item) {
            ImGui::Checkbox(label, &item.active);
            ImGui::SameLine(85.0f);
            ImGui::PushID(label);
            ImGui::SetNextItemWidth(70.0f);
            ImGui::Combo("##op", &item.op, ops, 8);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(95.0f);
            ImGui::InputText("##val", item.val, sizeof(item.val));
            ImGui::PopID();
        };

        RenderCheckRow("Speed", m_chkSpeed);
        RenderCheckRow("Damage", m_chkDamage);
        RenderCheckRow("X Pos", m_chkX);
        RenderCheckRow("Y Pos", m_chkY);
        RenderCheckRow("Z Pos", m_chkZ);
        RenderCheckRow("X Mom", m_chkMomX);
        RenderCheckRow("Y Mom", m_chkMomY);
		
		// linedef action check
        ImGui::Checkbox("Linedef", &m_chkLinedef.active);
        ImGui::SameLine(85.0f);
        ImGui::PushID("LinedefCheck");
        const char* lineTypes[] = {"act", "skip"};
        ImGui::SetNextItemWidth(70.0f);
        ImGui::Combo("##type", &m_chkLinedefType, lineTypes, 2);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(95.0f);
        ImGui::InputText("##id", m_chkLinedef.val, sizeof(m_chkLinedef.val));
        ImGui::PopID();
		
		// use trigger
		if (ImGui::Checkbox("Used Switch/Door", &m_chkUse.active)) {
            m_chkUse.op = 0; // "=="
            strcpy(m_chkUse.val, "1");
        }

        ImGui::Columns(1);
        ImGui::Spacing();
        ImGui::Separator();

        // middle: auto-range
        ImGui::Text("Auto-Range Configuration");

        ImGui::Text("Preset:");
        ImGui::SameLine();
        const char* presets[] = {"1", "2", "3", "4", "5", "Max Range (Pseudo)", "Max Range (True)"};
        ImGui::SetNextItemWidth(160.0f);
        ImGui::Combo("##AutoRangePreset", &m_brsAutoRangePreset, presets, 7);

        ImGui::SameLine();
        ImGui::Checkbox("Avoid Turbo (limit to 50)", &m_avoidTurbo);

        ImGui::SameLine();
        if (ImGui::Button("Apply Auto-Ranges to Tics")) {
            int rad = m_brsAutoRangePreset + 1;
            int runStrafeMax = m_avoidTurbo ? 50 : 127;
            int runStrafeMin = m_avoidTurbo ? -50 : -128;
            int turnMax = 127;
            int turnMin = -128;

            for (size_t i = 0; i < m_brsTics.size(); ++i) {
                int targetTic = m_brsTics[i].ticNumber;

                if (m_brsAutoRangePreset <= 4) {
                    int r = xdre::getTicRun(targetTic);
                    int s = xdre::getTicStrafe(targetTic);
                    int t = xdre::getTicTurn(targetTic);

                    BRS_CalcAutoRange(r, rad, runStrafeMin, runStrafeMax, m_brsTics[i].runStr, sizeof(m_brsTics[i].runStr));
                    BRS_CalcAutoRange(s, rad, runStrafeMin, runStrafeMax, m_brsTics[i].strafeStr, sizeof(m_brsTics[i].strafeStr));
                    BRS_CalcAutoRange(t, rad, turnMin, turnMax, m_brsTics[i].turnStr, sizeof(m_brsTics[i].turnStr));
                } else if (m_brsAutoRangePreset == 5) {
                    strcpy(m_brsTics[i].runStr, "50:-50");
                    strcpy(m_brsTics[i].strafeStr, "50:-50");
                    strcpy(m_brsTics[i].turnStr, "32:-31");
                } else if (m_brsAutoRangePreset == 6) {
                    strcpy(m_brsTics[i].runStr, "50:-50");
                    strcpy(m_brsTics[i].strafeStr, "50:-50");
                    strcpy(m_brsTics[i].turnStr, "127:-128");
                }
            }
        }

        ImGui::Spacing();
        ImGui::Separator();

        // bottom
        ImGui::Text("Worker Settings");

        int maxThreads = static_cast<int>(std::thread::hardware_concurrency());
        if (maxThreads < 1) maxThreads = (m_brsThreadGoal > 0 ? m_brsThreadGoal : 16);
        maxThreads = (std::max)(maxThreads, static_cast<int>(m_brsThreadGoal));

        const float labelWidth = 100.0f;
        const float fieldWidth = 546.0f;

        // threads
        ImGui::Text("CPU Workers:");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(fieldWidth);
        ImGui::SliderInt("##CPUWorkers", &m_brsThreadGoal, 1, maxThreads);

        // paths and arguments
        auto RenderLeftLabeledInput = [&](const char* label, const char* id, char* buf, size_t bufSize) {
            ImGui::Text("%s:", label);
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(fieldWidth);
            ImGui::InputText(id, buf, bufSize);
        };

        RenderLeftLabeledInput("DSDA Exe", "##dsdaExe", m_brsExePath, sizeof(m_brsExePath));
        RenderLeftLabeledInput("IWAD Path", "##iwadPath", m_brsIwadPath, sizeof(m_brsIwadPath));
        RenderLeftLabeledInput("PWAD Path", "##pwadPath", m_brsPwadPath, sizeof(m_brsPwadPath));
        RenderLeftLabeledInput("Extra Args", "##extraArgs", m_brsExtraArgs, sizeof(m_brsExtraArgs));

        ImGui::Checkbox("DSDA NoMo mode (-nomo)", &m_brsNomo);

        ImGui::Separator();

        // execution status
        if (m_brsIsRunning) {
            float progress = (m_brsTotalTasks > 0) ? ((float)m_brsCompletedTasks / (float)m_brsTotalTasks) : 0.0f;
            ImGui::ProgressBar(progress, ImVec2(-1, 0), "");
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Running (%d / %d tasks complete)...", m_brsCompletedTasks.load(), m_brsTotalTasks.load());
            if (ImGui::Button("Cancel Search", ImVec2(130, 0))) {
                AbortBRSSearch();
            }
        } else {
            if (ImGui::Button("Start Search", ImVec2(130, 0))) {
                StartBRSSearch();
            }
            ImGui::SameLine();
            if (ImGui::Button("Close", ImVec2(100, 0))) {
                ImGui::CloseCurrentPopup();
            }
            if (!m_brsStatusMessage.empty()) {
                ImGui::SameLine();
                ImGui::TextUnformatted(m_brsStatusMessage.c_str());
            }
        }

        ImGui::Separator();
        ImGui::Text("Results / Log:");
        ImGui::BeginChild("BRSResultBox", ImVec2(0, 75), true, ImGuiWindowFlags_HorizontalScrollbar);
        if (!m_brsResultDetails.empty()) {
            ImGui::TextUnformatted(m_brsResultDetails.c_str());
        } else {
            ImGui::TextDisabled("Idle - awaiting search output...");
        }
        ImGui::EndChild();

        ImGui::EndPopup();
    }
}