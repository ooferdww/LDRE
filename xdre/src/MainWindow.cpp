/* LDRE: User interface
 * Authors: ooferdww */

#include "MainWindow.hpp"
#include "../imgui/imgui.h"
#include "SDL.h"

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <cstdlib>
#include <cstdio>
#include <limits.h>
#include <chrono>
#include <thread>
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
#else
    char path[MAX_PATH] = "";
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len > 0) {
        path[len] = '\0';
        char* lastSlash = strrchr(path, '/');
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

static std::string GetSegmentDemoOutputDirectory() {
    std::string base = GetExeDirectory();
    return base.empty() ? "dsda_segment_demos" : (base + "dsda_segment_demos");
}

// ensure dir exists
static void Win32_CreateDirectory(const std::string& path) {
#ifdef _WIN32
    CreateDirectoryA(path.c_str(), NULL);
#else
    mkdir(path.c_str(), 0755);
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
    DIR* dir = opendir(path.c_str());
    if (!dir) return false;

    bool allDeleted = true;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        std::string itemPath = path + "/" + entry->d_name;
        struct stat st;
        if (stat(itemPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            if (!Win32_DeleteDirectoryInternal(itemPath)) {
                allDeleted = false;
            }
        } else {
            if (unlink(itemPath.c_str()) != 0) {
                allDeleted = false;
            }
        }
    }
    closedir(dir);

    if (allDeleted) {
        return (rmdir(path.c_str()) == 0);
    }
    return false;
#endif
}

static void Win32_DeleteDirectory(const std::string& path) {
    Win32_DeleteDirectoryInternal(path);
}

// bruteforce iwad/pwad stuff
static bool FileExists(const std::string& path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    return (stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode));
#endif
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
#ifdef _WIN32
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
#else
            const char* envVal = std::getenv("DOOMWADDIR");
            if (envVal && envVal[0] != '\0') {
                std::string envDir = envVal;
                if (envDir.back() != '/') envDir += "/";
                for (const char* name : commonIwads) {
                    if (FileExists(envDir + name)) {
                        detectedIwad = envDir + name;
                        break;
                    }
                }
            }
#endif
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
	xdre::setMovementLimit(m_movementLimit);

    if (xdre::getBlood() != m_bloodEnabled) {
        xdre::toggleBlood();
    }
	
	if (xdre::getBlockmapGrid() != m_blockmapGridEnabled) {
    xdre::toggleBlockmapGrid();
    }
	
	if (xdre::getThingHealth() != m_thingHealthEnabled) {
        xdre::toggleThingHealth();
    }

    if (xdre::getThingIndex() != m_thingIndexEnabled) {
        xdre::toggleThingIndex();
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

static void BuildCommandMap(const std::unordered_map<std::string, std::string>& cfgMap,
                            std::unordered_map<std::string, XCmd>& outCmdMap) {
    outCmdMap.clear();
    for (auto const& pair : cfgMap) {
        if (pair.second.empty()) continue;

        if (pair.first == "Fire")                     outCmdMap[pair.second] = XCmd::Fire;
        else if (pair.first == "Use")                 outCmdMap[pair.second] = XCmd::Use;
        else if (pair.first == "TurnStepLeft")        outCmdMap[pair.second] = XCmd::TurnStepLeft;
        else if (pair.first == "TurnStepRight")       outCmdMap[pair.second] = XCmd::TurnStepRight;
		else if (pair.first == "RunStepFwd")          outCmdMap[pair.second] = XCmd::RunStepFwd;
        else if (pair.first == "RunStepBwd")          outCmdMap[pair.second] = XCmd::RunStepBwd;
        else if (pair.first == "StrafeStepLeft")      outCmdMap[pair.second] = XCmd::StrafeStepLeft;
        else if (pair.first == "StrafeStepRight")     outCmdMap[pair.second] = XCmd::StrafeStepRight;
        else if (pair.first == "TurnStep")            outCmdMap[pair.second] = XCmd::TurnStep;
        else if (pair.first == "PrevTic")             outCmdMap[pair.second] = XCmd::PrevTic;
        else if (pair.first == "NextTic")             outCmdMap[pair.second] = XCmd::NextTic;
        else if (pair.first == "CopyTic")             outCmdMap[pair.second] = XCmd::CopyTic;
        else if (pair.first == "DeleteTic")           outCmdMap[pair.second] = XCmd::DeleteTic;
        else if (pair.first == "NewTic")              outCmdMap[pair.second] = XCmd::NewTic;
		else if (pair.first == "ClearMovement")       outCmdMap[pair.second] = XCmd::ClearMovement;
        else if (pair.first == "ToggleBlood")         outCmdMap[pair.second] = XCmd::ToggleBlood;
        else if (pair.first == "ToggleAutomap")       outCmdMap[pair.second] = XCmd::ToggleAutomap;
        else if (pair.first == "ToggleAutomapFollow") outCmdMap[pair.second] = XCmd::ToggleAutomapFollow;
        else if (pair.first == "ZoomAutomapIn")       outCmdMap[pair.second] = XCmd::ZoomAutomapIn;
        else if (pair.first == "ZoomAutomapOut")      outCmdMap[pair.second] = XCmd::ZoomAutomapOut;
        else if (pair.first == "PanAutomapLeft")      outCmdMap[pair.second] = XCmd::PanAutomapLeft;
        else if (pair.first == "PanAutomapRight")     outCmdMap[pair.second] = XCmd::PanAutomapRight;
        else if (pair.first == "PanAutomapUp")        outCmdMap[pair.second] = XCmd::PanAutomapUp;
        else if (pair.first == "PanAutomapDown")      outCmdMap[pair.second] = XCmd::PanAutomapDown;
		else if (pair.first == "ToggleBlockmap")      outCmdMap[pair.second] = XCmd::ToggleBlockmap;
		else if (pair.first == "ToggleThingHealth")   outCmdMap[pair.second] = XCmd::ToggleThingHealth;
        else if (pair.first == "ToggleThingIndex")    outCmdMap[pair.second] = XCmd::ToggleThingIndex;
        else if (pair.first == "SetSavepoint")        outCmdMap[pair.second] = XCmd::SetSavepoint;
        else if (pair.first == "UnsetSavepoint")      outCmdMap[pair.second] = XCmd::UnsetSavepoint;
        else if (pair.first == "Load")                outCmdMap[pair.second] = XCmd::Load;
        else if (pair.first == "Save")                outCmdMap[pair.second] = XCmd::Save;
        else if (pair.first == "Exit")                outCmdMap[pair.second] = XCmd::Exit;
        else if (pair.first == "RunBwd")              outCmdMap[pair.second] = XCmd::RunBwd;
        else if (pair.first == "RunFwd")              outCmdMap[pair.second] = XCmd::RunFwd;
        else if (pair.first == "StrafeLeft")          outCmdMap[pair.second] = XCmd::StrafeLeft;
        else if (pair.first == "StrafeRight")         outCmdMap[pair.second] = XCmd::StrafeRight;
        else if (pair.first == "TurnLeft")            outCmdMap[pair.second] = XCmd::TurnLeft;
        else if (pair.first == "TurnRight")           outCmdMap[pair.second] = XCmd::TurnRight;
        else if (pair.first == "Weapon")              outCmdMap[pair.second] = XCmd::Weapon;
        else if (pair.first == "TransposeLeft")       outCmdMap[pair.second] = XCmd::TransposeLeft;
        else if (pair.first == "TransposeRight")      outCmdMap[pair.second] = XCmd::TransposeRight;
        else if (pair.first == "CopyTicX")            outCmdMap[pair.second] = XCmd::CopyTicX;
        else if (pair.first == "DeleteTicX")          outCmdMap[pair.second] = XCmd::DeleteTicX;
        else if (pair.first == "BackwardDemo")        outCmdMap[pair.second] = XCmd::BackwardDemo;
        else if (pair.first == "ForwardDemo")         outCmdMap[pair.second] = XCmd::ForwardDemo;
        else if (pair.first == "ChangePlayer")        outCmdMap[pair.second] = XCmd::ChangePlayer;
    }
}

void MainWindow::SetKeys() {
    // keep toggleautomap synced between both control sets
    m_automapConfigToStringMap["ToggleAutomap"] = m_configToStringMap["ToggleAutomap"];

    BuildCommandMap(m_configToStringMap, m_stringToCommandMap);
    BuildCommandMap(m_automapConfigToStringMap, m_automapStringToCommandMap);
}

void MainWindow::LoadConfig() {
    std::string iniPath = GetExeDirectory() + "xdre.ini";
    LoadIni(iniPath);

    const char* keyNames[] = {
        "Fire", "Use", "TurnStepLeft", "TurnStepRight", "TurnStep",
        "RunStepFwd", "RunStepBwd", "StrafeStepLeft", "StrafeStepRight",
        "PrevTic", "NextTic", "CopyTic", "DeleteTic", "NewTic",
        "ToggleBlood", "ClearMovement", "ToggleAutomap",
        "SetSavepoint", "UnsetSavepoint", "Load", "Save", "Exit",
        "RunBwd", "RunFwd", "StrafeLeft", "StrafeRight", "TurnLeft", "TurnRight",
        "Weapon", "TransposeLeft", "TransposeRight", "CopyTicX", "DeleteTicX",
        "BackwardDemo", "ForwardDemo", "ChangePlayer"
    };
    const char* defaults[] = {
        "f", "u", "-", "+", "ts",
        "o", "p", "k", "l",
        "j", "i", "c", "x", "n",
        "z", "h", "m",
        "ssp", "pss", "load", "save", "exit",
        "wq", "we", "sa", "sd", "re", "rt",
        "g", "tr", "ty", "tc", "tx",
        ",", ".", "v"
    };

    m_configToStringMap.clear();
    for (size_t i = 0; i < sizeof(keyNames)/sizeof(keyNames[0]); ++i) {
        m_configToStringMap[keyNames[i]] = IniRead(keyNames[i], defaults[i]);
    }
	
	// automap mode binds
    const char* automapKeyNames[] = {
        "Fire", "Use", "TurnStepLeft", "TurnStepRight", "TurnStep",
        "PrevTic", "NextTic", "CopyTic", "DeleteTic", "NewTic",
        "ToggleAutomap", "ToggleAutomapFollow", "ZoomAutomapIn", "ZoomAutomapOut",
        "PanAutomapUp", "PanAutomapLeft", "PanAutomapDown", "PanAutomapRight",
        "ToggleBlockmap", "ToggleThingHealth", "ToggleThingIndex",
        "SetSavepoint", "UnsetSavepoint", "Load", "Save", "Exit",
        "RunBwd", "RunFwd", "StrafeLeft", "StrafeRight", "TurnLeft", "TurnRight",
        "Weapon", "TransposeLeft", "TransposeRight", "CopyTicX", "DeleteTicX",
        "BackwardDemo", "ForwardDemo", "ChangePlayer"
    };
    const char* automapDefaults[] = {
        "k", "u", "-", "+", "ts",
        "j", "i", "c", "x", "n",
        "m", "f", "l", "o",
        "w", "a", "s", "d",
        "b", "z", "q",
        "ssp", "pss", "load", "save", "exit",
        "bq", "be", "pg", "ph", "re", "rt",
        "g", "tr", "ty", "tc", "tx",
        ",", ".", "v"
    };

    m_automapConfigToStringMap.clear();
    for (size_t i = 0; i < sizeof(automapKeyNames)/sizeof(automapKeyNames[0]); ++i) {
        std::string iniKey = std::string("Automap_") + automapKeyNames[i];
        m_automapConfigToStringMap[automapKeyNames[i]] = IniRead(iniKey, automapDefaults[i]);
    }

    m_selectedAngleType = std::stoi(IniRead("AngleType", "3"));
    m_selectedSavepointStyle = std::stoi(IniRead("SavepointMode", "1"));
    m_lastDirectory = IniRead("LastDirectory", ".");
    m_bloodEnabled = (std::stoi(IniRead("Blood", "0")) != 0);
	m_momHighlightEnabled = (std::stoi(IniRead("MomHighlightEnabled", "1")) != 0);
    m_momLossThreshold = std::stof(IniRead("MomLossThreshold", "5.0"));
    if (m_momLossThreshold < 0.0f) { m_momLossThreshold = 0.0f; }
	m_movementLimit = std::stoi(IniRead("MovementLimit", "1"));
    if (m_movementLimit < 0 || m_movementLimit > 2) { m_movementLimit = 1; }
	m_automapPanSpeed = (std::max)(1, std::stoi(IniRead("AutomapPanSpeed", "2")));
	m_blockmapGridEnabled = (std::stoi(IniRead("BlockmapGrid", "0")) != 0);
	m_thingHealthEnabled  = (std::stoi(IniRead("ThingHealth", "0")) != 0);
    m_thingIndexEnabled   = (std::stoi(IniRead("ThingIndex", "0")) != 0);
	m_dsdaMovementFormat = (std::stoi(IniRead("DsdaMovementFormat", "0")) != 0);
	m_invertTicScrub = (std::stoi(IniRead("InvertTicScrub", "1")) != 0);
	m_brsThreadGoal = std::stoi(IniRead("BRSThreadGoal", "0"));
	
	std::string segDir = IniRead("SegmentFolder", "dsda_segment");
    std::strncpy(m_segmentDirPath, segDir.c_str(), sizeof(m_segmentDirPath) - 1);
    m_segmentDirPath[sizeof(m_segmentDirPath) - 1] = '\0';

    std::string segArgs = IniRead("SegmentExtraArgs", "");
    std::strncpy(m_segmentExtraArgs, segArgs.c_str(), sizeof(m_segmentExtraArgs) - 1);
    m_segmentExtraArgs[sizeof(m_segmentExtraArgs) - 1] = '\0';
	
	m_segmentGameSpeed = std::stoi(IniRead("SegmentGameSpeed", "50"));
    m_segmentGameSpeed = (std::max)(5, (std::min)(1000, m_segmentGameSpeed));
	
	m_segmentUseCustomConfig = (std::stoi(IniRead("SegmentUseCustomConfig", "0")) != 0);
    std::string segCfg = IniRead("SegmentConfigPath", "");
    std::strncpy(m_segmentConfigPath, segCfg.c_str(), sizeof(m_segmentConfigPath) - 1);
    m_segmentConfigPath[sizeof(m_segmentConfigPath) - 1] = '\0';
	
	m_segmentAutoLoadResult = (std::stoi(IniRead("SegmentAutoLoadResult", "1")) != 0);
	
    IniWrite("Blood", m_bloodEnabled ? "1" : "0");
	IniWrite("BlockmapGrid", m_blockmapGridEnabled ? "1" : "0");
	IniWrite("ThingHealth", m_thingHealthEnabled ? "1" : "0");
    IniWrite("ThingIndex", m_thingIndexEnabled ? "1" : "0");
	IniWrite("DsdaMovementFormat", m_dsdaMovementFormat ? "1" : "0");
	IniWrite("MovementLimit", std::to_string(m_movementLimit));
	
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
	
	// write automap mode binds
    for (auto const& pair : m_automapConfigToStringMap) {
        IniWrite(std::string("Automap_") + pair.first, pair.second);
    }

    // write ui states
    IniWrite("AngleType", std::to_string(m_selectedAngleType));
    IniWrite("SavepointMode", std::to_string(m_selectedSavepointStyle));
    IniWrite("LastDirectory", m_lastDirectory);
    IniWrite("Blood", m_bloodEnabled ? "1" : "0");
	IniWrite("MomHighlightEnabled", m_momHighlightEnabled ? "1" : "0");
    IniWrite("MomLossThreshold", std::to_string(m_momLossThreshold));
	IniWrite("AutomapPanSpeed", std::to_string(m_automapPanSpeed));
	IniWrite("BlockmapGrid", m_blockmapGridEnabled ? "1" : "0");
	IniWrite("ThingHealth", m_thingHealthEnabled ? "1" : "0");
    IniWrite("ThingIndex", m_thingIndexEnabled ? "1" : "0");
	IniWrite("DsdaMovementFormat", m_dsdaMovementFormat ? "1" : "0");
	IniWrite("MovementLimit", std::to_string(m_movementLimit));
	IniWrite("InvertTicScrub", m_invertTicScrub ? "1" : "0");
	IniWrite("BRSThreadGoal", std::to_string(m_brsThreadGoal));
	IniWrite("SegmentFolder", m_segmentDirPath);
    IniWrite("SegmentExtraArgs", m_segmentExtraArgs);
	IniWrite("SegmentGameSpeed", std::to_string(m_segmentGameSpeed));
	IniWrite("SegmentUseCustomConfig", m_segmentUseCustomConfig ? "1" : "0");
    IniWrite("SegmentConfigPath", m_segmentConfigPath);
	IniWrite("SegmentAutoLoadResult", m_segmentAutoLoadResult ? "1" : "0");
	
	IniWrite("GuiWindowX", std::to_string(m_guiPosX));
    IniWrite("GuiWindowY", std::to_string(m_guiPosY));
    IniWrite("DoomWindowX", std::to_string(m_doomPosX));
    IniWrite("DoomWindowY", std::to_string(m_doomPosY));

    std::string iniPath = GetExeDirectory() + "xdre.ini";
    SaveIni(iniPath);
}

static bool s_needInputTextPurge = false;
static bool s_spaceCommandTriggered = false;

void MainWindow::DoInput(XCmd cmd, unsigned int val) {
    switch (cmd) {
		case XCmd::None:                                         break;
        case XCmd::Fire:          xdre::toggleFire();            break;
        case XCmd::Use:           xdre::toggleUse();             break;
        case XCmd::TurnStepLeft:  xdre::turnStepLeft();          break;
        case XCmd::TurnStepRight: xdre::turnStepRight();         break;
        case XCmd::TurnStep:      xdre::setTurnStep(val);        break;
		case XCmd::RunStepFwd:      xdre::runStepFwd();          break;
        case XCmd::RunStepBwd:      xdre::runStepBwd();          break;
        case XCmd::StrafeStepLeft:  xdre::strafeStepLeft();      break;
        case XCmd::StrafeStepRight: xdre::strafeStepRight();     break;
        case XCmd::PrevTic:       xdre::seekDemo(-1);            break;
        case XCmd::NextTic:       xdre::seekDemo(1);             break;
        case XCmd::CopyTic:       xdre::copyTic();               break;
        case XCmd::CopyTicX:      xdre::copyTicX(val);           break;
        case XCmd::DeleteTic:     xdre::deleteTic();             break;
        case XCmd::DeleteTicX:    xdre::deleteTicX(val);         break;
        case XCmd::NewTic:        xdre::newTic();                break;
		case XCmd::ClearMovement: xdre::run(0); xdre::strafe(0); break;
        case XCmd::ToggleBlood:   xdre::toggleBlood();      m_bloodEnabled = xdre::getBlood();    break;
		case XCmd::ToggleAutomap: xdre::toggleAutomap(); m_inputBuffer[0] = '\0'; s_needInputTextPurge = true; break;
		case XCmd::ToggleAutomapFollow: xdre::toggleAutomapFollow(); break;
		case XCmd::ZoomAutomapIn:       xdre::zoomAutomap(1);        break;
        case XCmd::ZoomAutomapOut:      xdre::zoomAutomap(-1);       break;
		case XCmd::PanAutomapLeft:      xdre::panAutomap(-m_automapPanSpeed, 0);     break;
        case XCmd::PanAutomapRight:     xdre::panAutomap(m_automapPanSpeed, 0);      break;
        case XCmd::PanAutomapUp:        xdre::panAutomap(0, m_automapPanSpeed);      break;
        case XCmd::PanAutomapDown:      xdre::panAutomap(0, -m_automapPanSpeed);     break;
		case XCmd::ToggleBlockmap:    xdre::toggleBlockmapGrid(); m_blockmapGridEnabled = xdre::getBlockmapGrid(); break;
        case XCmd::ToggleThingHealth: xdre::toggleThingHealth();  m_thingHealthEnabled  = xdre::getThingHealth();  break;
        case XCmd::ToggleThingIndex:  xdre::toggleThingIndex();   m_thingIndexEnabled   = xdre::getThingIndex();   break;
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
	
	// invalidate cached momentum from current tic onward when tic data changes
    if (cmd != XCmd::None && cmd != XCmd::PrevTic && cmd != XCmd::NextTic &&
        cmd != XCmd::BackwardDemo && cmd != XCmd::ForwardDemo &&
        cmd != XCmd::ToggleBlood && cmd != XCmd::ToggleAutomap &&
        cmd != XCmd::ToggleAutomapFollow && cmd != XCmd::ZoomAutomapIn &&
        cmd != XCmd::ZoomAutomapOut && cmd != XCmd::PanAutomapLeft &&
        cmd != XCmd::PanAutomapRight && cmd != XCmd::PanAutomapUp &&
        cmd != XCmd::PanAutomapDown && cmd != XCmd::SetSavepoint &&
        cmd != XCmd::UnsetSavepoint && cmd != XCmd::Save && cmd != XCmd::Exit &&
        cmd != XCmd::ChangePlayer) {
        unsigned int editTic = xdre::getCurrentTic();
        if (editTic < m_ticMomentumValid.size()) {
            std::fill(m_ticMomentumValid.begin() + editTic, m_ticMomentumValid.end(), 0);
        }
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

    XCmd cmd = GetCommandForString(strCmd);
    if (cmd == XCmd::None) return;

    // cmds that require numeric arg must have one
    if (cmd >= XCmd::RunBwd && !hasNum) return;

    DoInput(cmd, static_cast<unsigned int>(val));
    m_inputBuffer[0] = '\0';
}

void MainWindow::Render() {
	// check if background worker queued winning inputs to apply directly in editor
    if (m_brsPendingReload.load()) {
        m_brsPendingReload = false;

        int opts[28] = { 0 };
        xdre::getOptions(opts);
        bool isLongtics = (opts[27] != 0);

        int maxListIndex = static_cast<int>(xdre::getTiclistSize()) - 1;

        for (const auto& winCmd : m_brsPendingWinCmds) {
            if (winCmd.frame < 0) continue;
            int ticIdx = m_brsPendingWinBaseTic + winCmd.frame;
            if (ticIdx < 0 || ticIdx > maxListIndex) continue;

            int cur = static_cast<int>(xdre::getCurrentTic());
            if (ticIdx != cur) {
                xdre::seekDemo(ticIdx - cur);
            }

            // read existing buttons on this tic so toggles match exact target state
            unsigned char existingBtns = 0;
            auto existingRange = xdre::getTicDataRange(ticIdx, ticIdx);
            if (!existingRange.empty()) {
                existingBtns = existingRange[0].buttons;
            }

            xdre::run(static_cast<signed char>(winCmd.forwardmove));
            xdre::strafe(static_cast<signed char>(winCmd.sidemove));

            if (isLongtics) {
                xdre::turn(static_cast<signed short>(winCmd.angleturn));
            } else {
                signed char turnUnits = static_cast<signed char>((winCmd.angleturn + 128) >> 8);
                xdre::turn(static_cast<signed short>(turnUnits));
            }

            bool wantFire = (winCmd.buttons & 1) != 0;
            bool hasFire  = (existingBtns & 1) != 0;
            if (wantFire != hasFire) {
                xdre::toggleFire();
            }

            bool wantUse = (winCmd.buttons & 2) != 0;
            bool hasUse  = (existingBtns & 2) != 0;
            if (wantUse != hasUse) {
                xdre::toggleUse();
            }

            if (winCmd.buttons & 4) {
                char wp = static_cast<char>(((winCmd.buttons >> 3) & 7) + 1);
                xdre::changeWeapon(wp);
            }
        }

        if (m_brsPendingWinBaseTic >= 0 && m_brsPendingWinBaseTic < (int)m_ticMomentumValid.size()) {
            std::fill(m_ticMomentumValid.begin() + m_brsPendingWinBaseTic, m_ticMomentumValid.end(), 0);
        }

        // seek to target tic
        int currentTic = static_cast<int>(xdre::getCurrentTic());
        int destTic = (m_brsTargetTic >= 0) ? m_brsTargetTic : m_brsInitialTic;
        int targetTic = (std::min)(destTic, maxListIndex);
        if (targetTic >= 0 && targetTic != currentTic) {
            xdre::seekDemo(targetTic - currentTic);
        }

        m_brsPendingWinCmds.clear();
        m_brsPendingWinBaseTic = -1;
    }
	
	// check if a segment recording session queued the latest output demo for reload
    if (m_segmentPendingReload.load()) {
        m_segmentPendingReload = false;
        FILE* f = std::fopen(m_segmentPendingReloadPath.c_str(), "rb");
        if (f) {
            xdre::load(f);

            m_ticMomentumCache.clear();
            m_ticMomentumValid.clear();
            m_brsTics.clear();
            m_selectedBRSTic = -1;

            std::fclose(f);

            // seek to the tic where the segment started (or end of demo if shorter)
            int currentTic = static_cast<int>(xdre::getCurrentTic());
            int maxListIndex = static_cast<int>(xdre::getTiclistSize()) - 1;
            int targetTic = (std::min)((std::max)(0, m_segmentSkipTic), maxListIndex);
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
    RenderSegmentDialog();
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
				m_chkAngleType = m_selectedAngleType; // default angle type to selected type

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
			if (ImGui::MenuItem("Slowmotion/Segment...")) {
                m_segmentSkipTic = static_cast<int>(xdre::getCurrentTic());
                m_segmentStatusMessage = "";

                std::string tempDemo = GetExeDirectory() + "segment_temp_input.lmp";
                FILE* f = std::fopen(tempDemo.c_str(), "wb");
                if (f) {
                    xdre::save(f);
                    std::fclose(f);
                    std::strncpy(m_segmentDemoPath, tempDemo.c_str(), sizeof(m_segmentDemoPath) - 1);
                    m_segmentDemoPath[sizeof(m_segmentDemoPath) - 1] = '\0';
                }

                m_showSegmentDialog = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Options")) {
			
			if (ImGui::MenuItem("Options...")) {
                m_bloodEditBuffer = xdre::getBlood();
                m_invertTicScrubEditBuffer = m_invertTicScrub;
                m_automapPanSpeedEditBuffer = m_automapPanSpeed;
				m_blockmapGridEditBuffer = xdre::getBlockmapGrid();
				m_thingHealthEditBuffer  = xdre::getThingHealth();
                m_thingIndexEditBuffer   = xdre::getThingIndex();
				m_dsdaMovementFormatEditBuffer = m_dsdaMovementFormat;
                m_momHighlightEditBuffer = m_momHighlightEnabled;
                m_momLossThresholdEditBuffer = m_momLossThreshold;
				m_movementLimitEditBuffer = m_movementLimit;
				m_segmentAutoLoadResultEditBuffer = m_segmentAutoLoadResult;
                m_showOptionsDialog = true;
            }
			
            if (ImGui::MenuItem("Controls...")) {
                m_keyEditList.clear();
                const char* keyNames[] = {
                    "Fire", "Use", "TurnStepLeft", "TurnStepRight", "TurnStep",
                    "RunStepFwd", "RunStepBwd", "StrafeStepLeft", "StrafeStepRight",
                    "PrevTic", "NextTic", "CopyTic", "DeleteTic", "NewTic",
                    "ToggleBlood", "ClearMovement", "ToggleAutomap", "SetSavepoint", "UnsetSavepoint", "Load", "Save", "Exit",
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

                m_automapKeyEditList.clear();
                const char* automapKeyNames[] = {
                    "PanAutomapUp", "PanAutomapLeft", "PanAutomapDown", "PanAutomapRight",
                    "ToggleAutomapFollow", "ZoomAutomapIn", "ZoomAutomapOut",
                    "ToggleBlockmap", "ToggleThingHealth", "ToggleThingIndex",
                    "Fire", "Use", "TurnStepLeft", "TurnStepRight", "TurnStep",
                    "PrevTic", "NextTic", "CopyTic", "DeleteTic", "NewTic",
                    "SetSavepoint", "UnsetSavepoint", "Load", "Save", "Exit",
                    "RunBwd", "RunFwd", "StrafeLeft", "StrafeRight", "TurnLeft", "TurnRight",
                    "Weapon", "TransposeLeft", "TransposeRight", "CopyTicX", "DeleteTicX",
                    "BackwardDemo", "ForwardDemo", "ChangePlayer"
                };
                for (const char* name : automapKeyNames) {
                    KeyBindEntry entry;
                    entry.actionName = name;
                    std::string currentVal = m_automapConfigToStringMap[name];
                    std::memset(entry.buffer, 0, sizeof(entry.buffer));
                    std::strncpy(entry.buffer, currentVal.c_str(), sizeof(entry.buffer) - 1);
                    m_automapKeyEditList.push_back(entry);
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

static std::string SerializeTicsToClipboard(const std::vector<xdre::TicData>& tics) {
    std::ostringstream oss;
    oss << "LDRE_TICS_V1\n";
    for (const auto& t : tics) {
        oss << static_cast<int>(t.forwardmove) << " "
            << static_cast<int>(t.sidemove) << " "
            << static_cast<int>(t.angleturn) << " "
            << static_cast<unsigned int>(t.buttons) << "\n";
    }
    return oss.str();
}

static bool DeserializeTicsFromClipboard(const char* text, std::vector<xdre::TicData>& outTics) {
    outTics.clear();
    if (!text || text[0] == '\0') return false;

    std::istringstream iss(text);
    std::string header;
    if (!std::getline(iss, header)) return false;
    while (!header.empty() && (header.back() == '\r' || header.back() == '\n' || header.back() == ' ')) {
        header.pop_back();
    }
    if (header != "LDRE_TICS_V1") return false;

    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty() || line == "\r") continue;
        std::istringstream lineStream(line);
        int fwd = 0, str = 0, turn = 0;
        unsigned int btns = 0;
        if (!(lineStream >> fwd >> str >> turn >> btns)) {
            return false;
        }
        xdre::TicData td;
        td.forwardmove = static_cast<signed char>(fwd);
        td.sidemove = static_cast<signed char>(str);
        td.angleturn = static_cast<signed short>(turn);
        td.buttons = static_cast<unsigned char>(btns);
        outTics.push_back(td);
    }
    return !outTics.empty();
}

void MainWindow::RenderTicList() {
    ImGui::Text("Tic Commands");
    ImGui::Separator();

    int const totalTics = static_cast<int>(xdre::getTiclistSize());
    int const currentTic = static_cast<int>(xdre::getCurrentTic());

    if (m_ticSelStart >= totalTics || m_ticSelEnd >= totalTics) {
        m_ticSelStart = -1;
        m_ticSelEnd = -1;
        m_ticShiftSelecting = false;
    }

    ImVec2 outerSize(0.0f, -ImGui::GetFrameHeightWithSpacing());
    if (ImGui::BeginTable("TicTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, outerSize)) {
        ImGuiIO& io = ImGui::GetIO();
        bool const shiftHeld = io.KeyShift || ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);

        ImVec2 winPos = ImGui::GetWindowPos();
        ImVec2 winSize = ImGui::GetWindowSize();
        bool const anyPopupOpen = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
        bool const isHovered = !anyPopupOpen && ImGui::IsMouseHoveringRect(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y), false);
        m_ticListHovered = isHovered;

        // release shift --> copy selected range to clipboard
        if (m_ticShiftSelecting && !shiftHeld) {
            m_ticShiftSelecting = false;
            if (m_ticSelStart >= 0 && m_ticSelEnd >= 0) {
                auto copied = xdre::getTicDataRange(m_ticSelStart, m_ticSelEnd);
                if (!copied.empty()) {
                    std::string clipText = SerializeTicsToClipboard(copied);
                    ImGui::SetClipboardText(clipText.c_str());
                }
            }
        }

        // shift+v while hovering over tic list --> paste to end of demo
        if (isHovered && shiftHeld && ImGui::IsKeyPressed(ImGuiKey_V, false)) {
            const char* clipText = ImGui::GetClipboardText();
            std::vector<xdre::TicData> pastedTics;
            if (DeserializeTicsFromClipboard(clipText, pastedTics)) {
                xdre::appendTicsToEnd(pastedTics);
            }
        }

        // when m_invertTicScrub is true, normal wheel switches tics while rmb allows normal scroll
        bool const rmbDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
        bool const shouldScrubTics = m_invertTicScrub ? !rmbDown : rmbDown;

        if (isHovered && shouldScrubTics) {
            if (io.MouseWheel != 0.0f) {
                int const stepTics = -static_cast<int>(io.MouseWheel);
                xdre::seekDemo(stepTics);
                io.MouseWheel = 0.0f; // consume wheel event
            }
        }

        ImGui::TableSetupColumn("Tic", ImGuiTableColumnFlags_WidthFixed, 46.0f);
        ImGui::TableSetupColumn("Command", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        int selMin = (m_ticSelStart >= 0 && m_ticSelEnd >= 0) ? (std::min)(m_ticSelStart, m_ticSelEnd) : -1;
        int selMax = (m_ticSelStart >= 0 && m_ticSelEnd >= 0) ? (std::max)(m_ticSelStart, m_ticSelEnd) : -1;

        ImGuiListClipper clipper;
        clipper.Begin(totalTics);

        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                ImGui::TableNextRow();
                bool const isSelected = (i == currentTic);
                bool const isInRange = (selMin >= 0 && i >= selMin && i <= selMax);

                if (isSelected && isInRange) {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImVec4(0.35f, 0.55f, 0.70f, 0.85f)));
                } else if (isSelected) {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImVec4(0.25f, 0.45f, 0.75f, 0.70f)));
                } else if (isInRange) {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImVec4(0.25f, 0.60f, 0.45f, 0.50f)));
                }

                ImGui::TableSetColumnIndex(0);
                char numStr[16];
                snprintf(numStr, sizeof(numStr), "%d", i);
                if (ImGui::Selectable(numStr, isSelected || isInRange, ImGuiSelectableFlags_SpanAllColumns)) {
                    if (shiftHeld) {
                        if (!m_ticShiftSelecting) {
                            m_ticSelStart = i;
                            m_ticSelEnd = i;
                            m_ticShiftSelecting = true;
                        } else {
                            m_ticSelEnd = i;
                        }
                    } else {
                        m_ticSelStart = -1;
                        m_ticSelEnd = -1;
                        m_ticShiftSelecting = false;
                        xdre::seekDemo(i - currentTic);
                    }
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

static void FormatMovementValue(char* buf, size_t bufSize, double val, bool dsdaFormat, const char* decFmt = "%.6f") {
    if (!dsdaFormat) {
        snprintf(buf, bufSize, decFmt, val);
        return;
    }

    long long fixedVal = static_cast<long long>(std::llround(val * 65536.0));
    bool neg = (fixedVal < 0);
    unsigned long long absFixed = neg ? static_cast<unsigned long long>(-fixedVal)
                                      : static_cast<unsigned long long>(fixedVal);
    unsigned long long intPart  = absFixed >> 16;
    unsigned int fracPart       = static_cast<unsigned int>(absFixed & 0xFFFF);

    if (neg) {
        snprintf(buf, bufSize, "-%llu/%05u", intPart, fracPart);
    } else {
        snprintf(buf, bufSize, "%llu/%05u", intPart, fracPart);
    }
}

void MainWindow::RenderPlayerMovement() {
    if (ImGui::CollapsingHeader("Player Movement", ImGuiTreeNodeFlags_DefaultOpen)) {
        unsigned int curTic = xdre::getCurrentTic();
        size_t totalTics = xdre::getTiclistSize();

        if (m_ticMomentumCache.size() != totalTics) {
            m_ticMomentumCache.resize(totalTics, { 0.0, 0.0 });
            m_ticMomentumValid.resize(totalTics, 0);
        }

        double curXMom = xdre::getXMom();
        double curYMom = xdre::getYMom();
        if (curTic < totalTics) {
            m_ticMomentumCache[curTic] = { curXMom, curYMom };
            m_ticMomentumValid[curTic] = 1;
        }

        bool highlightX = false;
        bool highlightY = false;

        if (m_momHighlightEnabled && curTic > 0 && curTic < m_ticMomentumCache.size() && m_ticMomentumValid[curTic - 1]) {
            double prevXMom = m_ticMomentumCache[curTic - 1].first;
            double prevYMom = m_ticMomentumCache[curTic - 1].second;

            double lostX = std::abs(prevXMom) - std::abs(curXMom);
            double lostY = std::abs(prevYMom) - std::abs(curYMom);

            if (lostX >= static_cast<double>(m_momLossThreshold)) highlightX = true;
            if (lostY >= static_cast<double>(m_momLossThreshold)) highlightY = true;
        }

        double curXDist = xdre::getXDist();
        double curYDist = xdre::getYDist();

        // 0: normal, 1: red (< 0.9x), 2: green (> 1.4x), 3: blue (> 1.8x)
        int distColorX = 0;
        int distColorY = 0;

        double absMomX = std::abs(curXMom);
        double absDistX = std::abs(curXDist);
        if (absMomX > 1e-6 || absDistX > 1e-6) {
            if (absDistX < 0.9 * absMomX) {
                distColorX = 1; // red
            } else if (absDistX > 1.8 * absMomX) {
                distColorX = 3; // blue
            } else if (absDistX > 1.4 * absMomX) {
                distColorX = 2; // green
            }
        }

        double absMomY = std::abs(curYMom);
        double absDistY = std::abs(curYDist);
        if (absMomY > 1e-6 || absDistY > 1e-6) {
            if (absDistY < 0.9 * absMomY) {
                distColorY = 1; // red
            } else if (absDistY > 1.8 * absMomY) {
                distColorY = 3; // blue
            } else if (absDistY > 1.4 * absMomY) {
                distColorY = 2; // green
            }
        }

        if (ImGui::BeginTable("PlayerMovementTable", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg)) {
            // headers
            ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Z", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableHeadersRow();

            char valBuf[48];

            // position
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted("Position");
            ImGui::TableSetColumnIndex(1);
            FormatMovementValue(valBuf, sizeof(valBuf), xdre::getXPos(), m_dsdaMovementFormat);
            ImGui::TextUnformatted(valBuf);
            ImGui::TableSetColumnIndex(2);
            FormatMovementValue(valBuf, sizeof(valBuf), xdre::getYPos(), m_dsdaMovementFormat);
            ImGui::TextUnformatted(valBuf);
            ImGui::TableSetColumnIndex(3);
            FormatMovementValue(valBuf, sizeof(valBuf), xdre::getZPos(), m_dsdaMovementFormat, "%.2f");
            ImGui::TextUnformatted(valBuf);

            // momentum
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted("Momentum");

            ImGui::TableSetColumnIndex(1);
            FormatMovementValue(valBuf, sizeof(valBuf), curXMom, m_dsdaMovementFormat);
            if (highlightX) {
                ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "%s", valBuf);
            } else {
                ImGui::TextUnformatted(valBuf);
            }

            ImGui::TableSetColumnIndex(2);
            FormatMovementValue(valBuf, sizeof(valBuf), curYMom, m_dsdaMovementFormat);
            if (highlightY) {
                ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "%s", valBuf);
            } else {
                ImGui::TextUnformatted(valBuf);
            }

            ImGui::TableSetColumnIndex(3);
            ImGui::TextDisabled("-");

            // component distances
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted("Distance traveled");

            ImGui::TableSetColumnIndex(1);
            FormatMovementValue(valBuf, sizeof(valBuf), curXDist, m_dsdaMovementFormat);
            if (distColorX == 1) {
                ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "%s", valBuf);
            } else if (distColorX == 2) {
                ImGui::TextColored(ImVec4(0.25f, 1.0f, 0.25f, 1.0f), "%s", valBuf);
            } else if (distColorX == 3) {
                ImGui::TextColored(ImVec4(0.30f, 0.70f, 1.0f, 1.0f), "%s", valBuf);
            } else {
                ImGui::TextUnformatted(valBuf);
            }

            ImGui::TableSetColumnIndex(2);
            FormatMovementValue(valBuf, sizeof(valBuf), curYDist, m_dsdaMovementFormat);
            if (distColorY == 1) {
                ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "%s", valBuf);
            } else if (distColorY == 2) {
                ImGui::TextColored(ImVec4(0.25f, 1.0f, 0.25f, 1.0f), "%s", valBuf);
            } else if (distColorY == 3) {
                ImGui::TextColored(ImVec4(0.30f, 0.70f, 1.0f, 1.0f), "%s", valBuf);
            } else {
                ImGui::TextUnformatted(valBuf);
            }

            ImGui::TableSetColumnIndex(3);
            FormatMovementValue(valBuf, sizeof(valBuf), xdre::getZDist(), m_dsdaMovementFormat, "%.2f");
            ImGui::TextUnformatted(valBuf);

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

static int InputUnifiedCallback(ImGuiInputTextCallbackData* data) {
    MainWindow* window = (MainWindow*)data->UserData;

    // purge old submitted text without eating a newly typed character on the same frame
    if (s_needInputTextPurge && data->EventFlag != ImGuiInputTextFlags_CallbackCharFilter) {
      if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit && ImGui::GetIO().InputQueueCharacters.Size > 0) {
          if (data->BufTextLen > 1) {
              data->DeleteChars(0, data->BufTextLen - 1);
          }
      } else {
          if (data->BufTextLen > 0) {
              data->DeleteChars(0, data->BufTextLen);
          }
      }
      s_needInputTextPurge = false;
      if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways) {
          return 0;
      }
  }

    // single char interceptor
    if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) {
        ImGuiIO& io = ImGui::GetIO();
        bool shiftHeld = io.KeyShift || ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
        if (shiftHeld && window->IsTicListHovered()) {
            return 1; // block inputs from leaking out of tic list while shift is held
        }

        ImWchar c = data->EventChar;

        // space always submits the current command regardless of buffer contents
        if (c == ' ') {
            s_spaceCommandTriggered = true;
            return 1; // discard space
        }

        // single-char instant bindings only trigger when the input box is empty
        if (!window->IsInputBufferEmpty()) {
            return 0;
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
            // transpose left prefix (e.g. tr1-tr9)
            const std::string& transposeLeftKey = window->GetStringForConfig("TransposeLeft");
            if (!executed && !transposeLeftKey.empty() && lowerBuf.rfind(transposeLeftKey, 0) == 0 && lowerBuf.length() == transposeLeftKey.length() + 1) {
                char digit = lowerBuf.back();
                if (digit >= '1' && digit <= '9') {
                    unsigned int transposeVal = static_cast<unsigned int>(digit - '0');
                    window->DoInput(XCmd::TransposeLeft, transposeVal);
                    executed = true;
                }
            }
            // transpose right prefix (e.g. ty1-ty9)
            const std::string& transposeRightKey = window->GetStringForConfig("TransposeRight");
            if (!executed && !transposeRightKey.empty() && lowerBuf.rfind(transposeRightKey, 0) == 0 && lowerBuf.length() == transposeRightKey.length() + 1) {
                char digit = lowerBuf.back();
                if (digit >= '1' && digit <= '9') {
                    unsigned int transposeVal = static_cast<unsigned int>(digit - '0');
                    window->DoInput(XCmd::TransposeRight, transposeVal);
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

        ImGuiInputTextFlags flags = ImGuiInputTextFlags_CallbackCharFilter | 
                                    ImGuiInputTextFlags_CallbackEdit | 
                                    ImGuiInputTextFlags_CallbackAlways |
                                    ImGuiInputTextFlags_EnterReturnsTrue;

        static bool s_initialCommandFocus = true;
        if (!ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId) &&
            !ImGui::IsAnyItemActive() &&
            !ImGui::GetIO().KeyShift &&
            (s_initialCommandFocus || ImGui::GetIO().InputQueueCharacters.Size > 0)) {
            ImGui::SetKeyboardFocusHere();
            s_initialCommandFocus = false;
        }

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
#else
    char resolvedBuf[PATH_MAX] = "";
    std::string rawDir = m_lastDirectory.empty() ? "." : m_lastDirectory;
    std::string initDir;
    if (realpath(rawDir.c_str(), resolvedBuf) != nullptr) {
        initDir = resolvedBuf;
    } else {
        initDir = GetExeDirectory();
        if (initDir.empty()) initDir = ".";
    }
    if (initDir.back() != '/') initDir += '/';

    auto runDialogCmd = [](const std::string& cmd, char* outBuf, size_t outSize) -> bool {
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return false;
        bool gotLine = (fgets(outBuf, static_cast<int>(outSize), pipe) != nullptr);
        int status = pclose(pipe);
        return (gotLine && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    };

    const char* desktop = std::getenv("XDG_CURRENT_DESKTOP");
    std::string desktopStr = desktop ? desktop : "";
    for (char& c : desktopStr) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    bool hasKdialog = (system("command -v kdialog >/dev/null 2>&1") == 0);
    bool hasZenity  = (system("command -v zenity >/dev/null 2>&1") == 0);
    bool preferZenity = (desktopStr.find("gnome") != std::string::npos && hasZenity);

    bool ok = false;
    if (hasKdialog && !preferZenity) {
        std::string kdialogCmd = isSave
        ? ("kdialog --title \"Save Demo\" --getsavefilename \"" + initDir + "demo.lmp\" \"*.lmp|Doom Demo Files (*.lmp)\" 2>/dev/null")
        : ("kdialog --title \"Load Demo\" --getopenfilename \"" + initDir + "\" \"*.lmp|Doom Demo Files (*.lmp)\" 2>/dev/null");
        ok = runDialogCmd(kdialogCmd, filename, sizeof(filename));
    } else if (hasZenity) {
        std::string zenityCmd = isSave
        ? ("zenity --file-selection --save --confirm-overwrite --title=\"Save Demo\" --filename=\"" + initDir + "demo.lmp\" --file-filter=\"Doom Demo Files (*.lmp) | *.lmp\" --file-filter=\"All Files | *\" 2>/dev/null")
        : ("zenity --file-selection --title=\"Load Demo\" --filename=\"" + initDir + "\" --file-filter=\"Doom Demo Files (*.lmp) | *.lmp\" --file-filter=\"All Files | *\" 2>/dev/null");
        ok = runDialogCmd(zenityCmd, filename, sizeof(filename));
    }

    if (ok) {
        std::string path(filename);
        while (!path.empty() && (path.back() == '\n' || path.back() == '\r')) {
            path.pop_back();
        }
        if (!path.empty()) {
            if (isSave && path.find('.') == std::string::npos) {
                path += ".lmp";
            }
            size_t lastSlash = path.find_last_of('/');
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
            ++m_segmentLaunchGen;
            m_segmentPendingReload = false;

            xdre::load(f);
			
			m_ticMomentumCache.clear();
            m_ticMomentumValid.clear();
            m_brsTics.clear();
            m_selectedBRSTic = -1;
			
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
        const float labelWidth = 115.0f;
        const float controlWidth = 175.0f;

        auto RenderLeftLabeledInt = [&](const char* label, const char* id, int& val) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s", label);
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(controlWidth);
            ImGui::InputInt(id, &val);
        };

        auto BoolCheckbox = [](const char* label, int& val) {
            bool b = (val != 0);
            if (ImGui::Checkbox(label, &b)) val = b ? 1 : 0;
        };

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Complevel");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(controlWidth);
        if (ImGui::Combo("##Complevel", &m_headerComplevelIndex, s_complevelNames, 6)) {
            m_headerOptions[0] = s_allowedComplevels[m_headerComplevelIndex];
            xdre::changeOptions(m_headerOptions);
        }

        RenderLeftLabeledInt("Skill", "##Skill", m_headerOptions[1]);
        RenderLeftLabeledInt("Episode", "##Episode", m_headerOptions[2]);
        RenderLeftLabeledInt("Map", "##Map", m_headerOptions[3]);

        const char* dmChoices[] = { "0 - SP / Co-op", "1 - Deathmatch", "2 - Altdeath" };
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Deathmatch");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(controlWidth);
        ImGui::Combo("##Deathmatch", &m_headerOptions[4], dmChoices, 3);

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
        BoolCheckbox("Respawn", m_headerOptions[5]); ImGui::SameLine(155);
        BoolCheckbox("Fast Monsters", m_headerOptions[6]);
        BoolCheckbox("No Monsters", m_headerOptions[7]); ImGui::SameLine(155);
        BoolCheckbox("Longtics", m_headerOptions[27]);

#if 0
        // temporarily hidden
        BoolCheckbox("Monsters Remember", m_headerOptions[10]); ImGui::SameLine(155);
        BoolCheckbox("Weapon Recoil", m_headerOptions[12]);
        BoolCheckbox("Allow Pushers", m_headerOptions[13]); ImGui::SameLine(155);
        BoolCheckbox("Player Bobbing", m_headerOptions[14]);
        BoolCheckbox("Demo Insurance", m_headerOptions[15]); ImGui::SameLine(155);
        BoolCheckbox("Monster Infighting", m_headerOptions[17]);
        BoolCheckbox("Monsters Backwards", m_headerOptions[20]); ImGui::SameLine(155);
        BoolCheckbox("Avoid Hazards", m_headerOptions[21]);
        BoolCheckbox("Monster Friction", m_headerOptions[22]); ImGui::SameLine(155);
        BoolCheckbox("Help Friends", m_headerOptions[23]);
        BoolCheckbox("Dog Jumping", m_headerOptions[24]); ImGui::SameLine(155);
        BoolCheckbox("Climb Steep Stairs", m_headerOptions[25]);
#endif

        ImGui::Separator();
        RenderLeftLabeledInt("RNG Seed", "##RNGSeed", m_headerOptions[16]);

#if 0
        // temporarily hidden
        RenderLeftLabeledInt("Console Player", "##ConsolePlayer", m_headerOptions[8]);
        RenderLeftLabeledInt("Variable Friction", "##VariableFriction", m_headerOptions[11]);
        RenderLeftLabeledInt("Dogs (0-3)", "##Dogs", m_headerOptions[18]);
        RenderLeftLabeledInt("Friend Distance", "##FriendDistance", m_headerOptions[19]);
        RenderLeftLabeledInt("Compat Flags", "##CompatFlags", m_headerOptions[26]);
#endif

        ImGui::Separator();
        if (ImGui::Button("Apply Options", ImVec2(140, 0))) {
            xdre::changeOptions(m_headerOptions);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(140, 0))) {
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
        ImGui::Text("Bind actions to text command shortcuts (leave blank to unbind):");
        ImGui::Separator();

        ImGui::BeginChild("KeyScrollBox", ImVec2(560, 420), true);

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Main Controls");
        ImGui::Separator();
        ImGui::Columns(3, "MainKeysColumns", true);
        for (size_t i = 0; i < m_keyEditList.size(); ++i) {
            auto& item = m_keyEditList[i];
            ImGui::Text("%s", item.actionName.c_str());
            ImGui::PushID(static_cast<int>(i));
            ImGui::SetNextItemWidth(70.0f);
            ImGui::InputText("##key", item.buffer, sizeof(item.buffer));
            ImGui::PopID();
            ImGui::NextColumn();
        }
        ImGui::Columns(1);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), "Controls while Automap is enabled (ToggleAutomap is shared)");
        ImGui::Separator();
        ImGui::Columns(3, "AutomapKeysColumns", true);
        for (size_t i = 0; i < m_automapKeyEditList.size(); ++i) {
            auto& item = m_automapKeyEditList[i];
            ImGui::Text("%s", item.actionName.c_str());
            ImGui::PushID(static_cast<int>(1000 + i));
            ImGui::SetNextItemWidth(70.0f);
            ImGui::InputText("##amkey", item.buffer, sizeof(item.buffer));
            ImGui::PopID();
            ImGui::NextColumn();
        }
        ImGui::Columns(1);

        ImGui::EndChild();

        ImGui::Separator();
        if (ImGui::Button("Save & Apply", ImVec2(140, 0))) {
            auto TrimLower = [](const char* raw) {
                std::string s(raw);
                size_t first = s.find_first_not_of(" \t\r\n");
                if (first == std::string::npos) return std::string("");
                size_t last = s.find_last_not_of(" \t\r\n");
                s = s.substr(first, last - first + 1);
                for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                return s;
            };

            for (auto const& item : m_keyEditList) {
                std::string s = TrimLower(item.buffer);
                if (!s.empty()) {
                    m_configToStringMap[item.actionName] = s;
                }
            }

            for (auto const& item : m_automapKeyEditList) {
                // allow empty strings for unbinding
                m_automapConfigToStringMap[item.actionName] = TrimLower(item.buffer);
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
		ImGui::Checkbox("Auto-load latest segment demo into editor", &m_segmentAutoLoadResultEditBuffer);
		ImGui::Checkbox("Show movement values in DSDA format (int/65536)", &m_dsdaMovementFormatEditBuffer);
		ImGui::Checkbox("Show blockmap grid on automap", &m_blockmapGridEditBuffer);
		ImGui::Checkbox("Show thing health on automap", &m_thingHealthEditBuffer);
        ImGui::Checkbox("Show thing indices on automap", &m_thingIndexEditBuffer);

        ImGui::Spacing();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Automap pan speed:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::SliderInt("##AutomapPanSpeed", &m_automapPanSpeedEditBuffer, 1, 10, "%dx");
		
		const char* movementLimitModes[] = { "No limit", "Cap to non-turbo", "Cap to stroller" };
        ImGui::Spacing();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Movement limiter:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.0f);
        ImGui::Combo("##MovementLimit", &m_movementLimitEditBuffer, movementLimitModes, IM_ARRAYSIZE(movementLimitModes));
		
		ImGui::Spacing();
        ImGui::Checkbox("Highlight sudden momentum loss", &m_momHighlightEditBuffer);
        if (!m_momHighlightEditBuffer) ImGui::BeginDisabled();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Momentum loss threshold:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputFloat("##MomLossThreshold", &m_momLossThresholdEditBuffer, 0.5f, 1.0f, "%.2f");
        if (!m_momHighlightEditBuffer) ImGui::EndDisabled();

        ImGui::Separator();
        if (ImGui::Button("Save & Apply", ImVec2(120, 0))) {
            m_bloodEnabled = m_bloodEditBuffer;
            m_invertTicScrub = m_invertTicScrubEditBuffer;
			m_segmentAutoLoadResult = m_segmentAutoLoadResultEditBuffer;
            m_automapPanSpeed = (std::max)(1, m_automapPanSpeedEditBuffer);
			m_blockmapGridEnabled = m_blockmapGridEditBuffer;
			m_thingHealthEnabled  = m_thingHealthEditBuffer;
            m_thingIndexEnabled   = m_thingIndexEditBuffer;
			m_dsdaMovementFormat = m_dsdaMovementFormatEditBuffer;
			m_momHighlightEnabled = m_momHighlightEditBuffer;
            m_momLossThreshold = (std::max)(0.0f, m_momLossThresholdEditBuffer);
			m_movementLimit = m_movementLimitEditBuffer;             xdre::setMovementLimit(m_movementLimit);

            if (xdre::getBlood() != m_bloodEnabled) {
                xdre::toggleBlood();
            }
			
			if (xdre::getBlockmapGrid() != m_blockmapGridEnabled) {
                xdre::toggleBlockmapGrid();
            }
			
			if (xdre::getThingHealth() != m_thingHealthEnabled) {
                xdre::toggleThingHealth();
            }

            if (xdre::getThingIndex() != m_thingIndexEnabled) {
                xdre::toggleThingIndex();
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

void MainWindow::CleanupBRSArtifacts() {
    std::lock_guard<std::mutex> lock(m_brsWorkerMutex);
    Win32_DeleteDirectory(GetBFDirectory());
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

    ++m_segmentLaunchGen;
    m_segmentPendingReload = false;

    // refresh options and re-save current demo state right before starting workers
    xdre::getOptions(m_headerOptions);
    {
        std::string tempDemo = GetExeDirectory() + "brs_temp_input.lmp";
        FILE* f = std::fopen(tempDemo.c_str(), "wb");
        if (f) {
            xdre::save(f);
            std::fclose(f);
            std::strncpy(m_brsDemoPath, tempDemo.c_str(), sizeof(m_brsDemoPath) - 1);
            m_brsDemoPath[sizeof(m_brsDemoPath) - 1] = '\0';
        }
    }

    if (m_brsSupervisorThread.joinable())
        m_brsSupervisorThread.join();

    m_brsAbortRequested = false;
    m_brsIsRunning = true;
    m_brsCompletedTasks = 0;
    m_brsWinningTaskIndex = -1;
    m_brsStatusMessage = "Partitioning ranges...";
    m_brsResultDetails = "";

    // prepare range factors honoring m_movementLimit
    int runMinLim    = (m_movementLimit == 0) ? -128 : -50;
    int runMaxLim    = (m_movementLimit == 0) ?  127 :  50;
    int strafeMinLim = (m_movementLimit == 0) ? -128 : (m_movementLimit == 1 ? -50 : 0);
    int strafeMaxLim = (m_movementLimit == 0) ?  127 : (m_movementLimit == 1 ?  50 : 0);

    for (auto& tic : m_brsTics) {
        auto r = BRS_ParseRange(tic.runStr, runMinLim, runMaxLim);
        auto s = BRS_ParseRange(tic.strafeStr, strafeMinLim, strafeMaxLim);
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

#ifndef _WIN32
static std::vector<std::string> SplitCommandLinePosix(const std::string& cmd) {
    std::vector<std::string> args;
    std::string current;
    bool inQuotes = false;

    for (size_t i = 0; i < cmd.size(); ++i) {
        char c = cmd[i];
        if (c == '"') {
            inQuotes = !inQuotes;
        } else if (!inQuotes && std::isspace(static_cast<unsigned char>(c))) {
            if (!current.empty()) {
                args.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        args.push_back(current);
    }
    return args;
}
#endif

void MainWindow::BRSWorkerThread() {
    // force headless sdl drivers for dsda processes
#ifdef _WIN32
    SetEnvironmentVariableA("SDL_VIDEODRIVER", "dummy");
    SetEnvironmentVariableA("SDL_AUDIODRIVER", "dummy");
#else
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    setenv("SDL_AUDIODRIVER", "dummy", 1);
#endif

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

    // enforce active movement limiter (0: no limit, 1: non-turbo [-50,50], 2: stroller [-25,25] run, 0 strafe)
    for (auto& tic : ticsAscending) {
        if (m_movementLimit == 1 || m_movementLimit == 2) {
            auto r = BRS_ParseRange(tic.runStr, -50, 50);
            snprintf(tic.runStr, sizeof(tic.runStr), "%d:%d", r.first, r.second);
        }
        if (m_movementLimit == 1) {
            auto s = BRS_ParseRange(tic.strafeStr, -50, 50);
            snprintf(tic.strafeStr, sizeof(tic.strafeStr), "%d:%d", s.first, s.second);
        } else if (m_movementLimit == 2) {
            strcpy(tic.strafeStr, "0:0");
        }
    }

    auto increments = BRS_ProduceRangeIncrements(winningPairs, ticsAscending);
    auto finalCombinations = BRS_ProduceFinalCombinations(increments, bestFoundSlices);

    int complevel = GetSelectedComplevel();
    int baseTic   = ticsAscending.empty() ? 0 : ticsAscending.front().ticNumber;
    int endTic    = ticsAscending.empty() ? 0 : ticsAscending.back().ticNumber;

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
        else if (c.op == 8) opStr = "acap";

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
    AppendCond(m_chkRng, "rng");
	
	if (m_chkAngle.active) {
        const char* angNames[] = { "ang_lt", "ang_fa", "ang_deg", "ang_ba" };
        int idx = (m_chkAngleType >= 0 && m_chkAngleType < 4) ? m_chkAngleType : 0;
        AppendCond(m_chkAngle, angNames[idx]);
    }

    if (m_chkItem.active) {
        const char* itemCodes[] = {
            "rkc", "ykc", "bkc", "rsk", "ysk", "bsk",
            "f", "p", "sg", "cg", "rl", "pg", "bfg", "cs", "ssg"
        };
        if (m_chkItemIndex >= 0 && m_chkItemIndex < (int)(sizeof(itemCodes) / sizeof(itemCodes[0]))) {
            std::string prefix = (m_chkItemMode == 0) ? "have " : "lack ";
            condTokens.push_back(prefix + itemCodes[m_chkItemIndex]);
        }
    }

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

    auto IsRelativePath = [](const std::string& p) -> bool {
#ifdef _WIN32
        return (p.find(":\\") == std::string::npos && p.find(":/") == std::string::npos);
#else
        return (!p.empty() && p[0] != '/');
#endif
    };

    // assemble task cmds
    std::vector<BRSTask> taskQueue;
    for (int w = 0; w < actualWorkers; ++w) {
        // resolve absolute path
        std::string exePath = m_brsExePath;
        if (IsRelativePath(exePath)) {
            exePath = GetExeDirectory() + exePath;
        }

        std::string cmd = "\"" + exePath + "\" ";

        if (strlen(m_brsIwadPath) > 0) {
            std::string iwad = m_brsIwadPath;
            if (IsRelativePath(iwad))
                iwad = GetExeDirectory() + iwad;
            cmd += "-iwad \"" + iwad + "\" ";
        }
        if (strlen(m_brsPwadPath) > 0) {
            std::string pwadStr = m_brsPwadPath;
            // pass directly after -file
            if (pwadStr.front() == '"' || pwadStr.find(' ') != std::string::npos) {
                cmd += "-file " + pwadStr + " ";
            } else {
                if (IsRelativePath(pwadStr))
                    pwadStr = GetExeDirectory() + pwadStr;
                cmd += "-file \"" + pwadStr + "\" ";
            }
        }
        if (strlen(m_brsExtraArgs) > 0) cmd += std::string(m_brsExtraArgs) + " ";

        // outputdemo and queue will be local names
        cmd += "-recordfromto \"" + std::string(m_brsDemoPath) + "\" \"outputdemo" + std::to_string(w) + ".lmp\" ";

        // fast-forward in skip mode until exact 0-based .lmp tic index (dsda_DemoTic) is reached
        cmd += "-skiptic " + std::to_string(baseTic + 100000) + " ";
        cmd += "-bf_start_tic " + std::to_string(baseTic) + " ";

        if (std::string(m_brsExtraArgs).find("-complevel") == std::string::npos &&
            std::string(m_brsExtraArgs).find("-cl") == std::string::npos) {
            cmd += "-complevel " + std::to_string(complevel) + " ";
        }

        cmd += "-bf_queue \"brs_queue_" + std::to_string(w) + ".txt\" ";
        cmd += "-quit_after_brute_force -command \"";

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
    bool isAcapSearch = false;
    double acapTarget = 0.0;

    auto ParseAcapVal = [](const char* s) -> double {
        std::string str(s);
        auto slash = str.find('/');
        if (slash != std::string::npos) {
            long long intPart = std::stoll(str.substr(0, slash));
            unsigned long long fracPart = std::stoull(str.substr(slash + 1));
            double mag = static_cast<double>(std::llabs(intPart)) + (static_cast<double>(fracPart) / 65536.0);
            return (!str.empty() && str[0] == '-') ? -mag : mag;
        }
        return std::stod(str);
    };

    for (const auto* item : { &m_chkSpeed, &m_chkDamage, &m_chkX, &m_chkY, &m_chkZ, &m_chkMomX, &m_chkMomY, &m_chkRng, &m_chkAngle }) {
        if (item->active) {
            if (item->op == 6) isMinSearch = true;
            if (item->op == 7) isMaxSearch = true;
            if (item->op == 8) {
                isAcapSearch = true;
                try { acapTarget = ParseAcapVal(item->val); } catch (...) { acapTarget = 0.0; }
            }
        }
    }
    bool isOptimization = (isMinSearch || isMaxSearch || isAcapSearch);

    double bestTargetValue = isMinSearch ? (std::numeric_limits<double>::max)()
    : -(std::numeric_limits<double>::max)();
    double bestAcapDiff = (std::numeric_limits<double>::max)();

    m_brsTotalTasks = (int)taskQueue.size();
    int concurrentLimit = (std::min)(actualWorkers, m_brsTotalTasks.load());
    std::vector<BRSWorker> activeWorkers;

    // supervise execution
    while ((!taskQueue.empty() || !activeWorkers.empty()) && !m_brsAbortRequested) {
        while ((int)activeWorkers.size() < concurrentLimit && !taskQueue.empty() && !m_brsAbortRequested) {
            BRSTask task = taskQueue.back();
            taskQueue.pop_back();

            std::string logName = bfDir + "/process_output" + std::to_string(task.taskIndex) + ".txt";
#ifdef _WIN32
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
#else
            std::vector<std::string> argStrings = SplitCommandLinePosix(task.command);
            if (!argStrings.empty()) {
                pid_t pid = fork();
                if (pid == 0) {
                    if (chdir(bfDir.c_str()) != 0) {
                        _exit(127);
                    }
                    int logFd = open(logName.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (logFd >= 0) {
                        dup2(logFd, STDOUT_FILENO);
                        dup2(logFd, STDERR_FILENO);
                        close(logFd);
                    }
                    std::vector<char*> argv;
                    argv.reserve(argStrings.size() + 1);
                    for (auto& s : argStrings) {
                        argv.push_back(&s[0]);
                    }
                    argv.push_back(nullptr);
                    execvp(argv[0], argv.data());
                    _exit(127);
                } else if (pid > 0) {
                    activeWorkers.push_back({task.taskIndex, pid});
                }
            }
#endif
        }

        for (size_t i = 0; i < activeWorkers.size(); ) {
            bool finished = false;
            int taskIdx = activeWorkers[i].taskIndex;

#ifdef _WIN32
            DWORD exitCode = 0;
            if (GetExitCodeProcess(activeWorkers[i].hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
                finished = true;
                CloseHandle(activeWorkers[i].hProcess);
                CloseHandle(activeWorkers[i].hThread);
            }
#else
            int status = 0;
            pid_t res = waitpid(activeWorkers[i].pid, &status, WNOHANG);
            if (res > 0) {
                finished = true;
            }
#endif

            if (finished) {
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
                                if (isAcapSearch) {
                                    double diff = std::abs(val - acapTarget);
                                    if (diff < bestAcapDiff) {
                                        bestAcapDiff = diff;
                                        taskBestVal = val;
                                        foundCandidate = true;
                                    }
                                } else if (isMinSearch && val < taskBestVal) {
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
                        if (isAcapSearch) {
                            // bestAcapDiff already tracks global minimum distance across workers
                            isNewGlobalBest = true;
                        } else if (isMinSearch && taskBestVal < bestTargetValue) {
                            isNewGlobalBest = true;
                        } else if (isMaxSearch && taskBestVal > bestTargetValue) {
                            isNewGlobalBest = true;
                        }

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
#ifdef _WIN32
                            TerminateProcess(w.hProcess, 0);
                            WaitForSingleObject(w.hProcess, 500);
                            CloseHandle(w.hProcess);
                            CloseHandle(w.hThread);
#else
                            kill(w.pid, SIGKILL);
                            waitpid(w.pid, nullptr, 0);
#endif
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

#ifdef _WIN32
        Sleep(20);
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
#endif
    }

    // terminate and wait for all remaining workers to release file locks
    for (auto& w : activeWorkers) {
#ifdef _WIN32
        TerminateProcess(w.hProcess, 0);
        WaitForSingleObject(w.hProcess, 2000); // wait up to 2s for OS teardown
        CloseHandle(w.hProcess);
        CloseHandle(w.hThread);
#else
        kill(w.pid, SIGKILL);
        waitpid(w.pid, nullptr, 0);
#endif
    }
    activeWorkers.clear();

    if (m_brsAbortRequested) {
        CleanupBRSArtifacts();
        m_brsStatusMessage = "Search aborted by user.";
    } else {
        if (m_brsWinningTaskIndex >= 0) {
            int maxTargetTic = (std::max)(endTic, m_brsTargetTic);
            int writeDepth = (maxTargetTic - baseTic) + 1;
            std::string winLogPath = GetBFDirectory() + "/process_output" + std::to_string(m_brsWinningTaskIndex) + ".txt";

            std::vector<ParsedWinTicCmd> parsedCmds;
            {
                std::ifstream logIn(winLogPath);
                std::string line;
                while (std::getline(logIn, line)) {
                    auto pos = line.find("BF_WIN_CMD ");
                    if (pos != std::string::npos) {
                        int frame = 0, fwd = 0, str = 0, turn = 0;
                        unsigned int btns = 0;
                        if (std::sscanf(line.c_str() + pos + 11, "%d: %d %d %d %u", &frame, &fwd, &str, &turn, &btns) == 5) {
                            if (frame >= 0 && frame < writeDepth) {
                                if ((int)parsedCmds.size() <= frame) parsedCmds.resize(frame + 1);
                                parsedCmds[frame] = { frame, fwd, str, turn, btns };
                            }
                        }
                    }
                }
            }

            CleanupBRSArtifacts();

            std::ostringstream summary;
            summary << "Winning inputs starting at tic " << baseTic << ":\n";
            for (const auto& c : parsedCmds) {
                if (c.frame >= 0) {
                    summary << "  Tic " << (baseTic + c.frame)
                            << ": F=" << c.forwardmove
                            << " S=" << c.sidemove
                            << " T=" << ((c.angleturn + 128) >> 8)
                            << " B=" << c.buttons << "\n";
                }
            }
            m_brsResultDetails += summary.str();
            m_brsStatusMessage = "Search complete: solution found (worker " + std::to_string(m_brsWinningTaskIndex) + ").";

            if (!parsedCmds.empty()) {
                m_brsPendingWinBaseTic = baseTic;
                m_brsPendingWinCmds = std::move(parsedCmds);
                m_brsPendingReload = true;
            }
        } else {
            CleanupBRSArtifacts();
            m_brsStatusMessage = "Search finished: no matching condition found.";
        }
    }

#ifdef _WIN32
    SetEnvironmentVariableA("SDL_VIDEODRIVER", NULL);
    SetEnvironmentVariableA("SDL_AUDIODRIVER", NULL);
#else
    unsetenv("SDL_VIDEODRIVER");
    unsetenv("SDL_AUDIODRIVER");
#endif

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
    ImGui::SetNextWindowSize(ImVec2(656.0f, 0), ImGuiCond_Always);

    if (ImGui::BeginPopupModal("Brute-force", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        
        // top section
        ImGui::Columns(2, "BRSTopSplit", true);
        ImGui::SetColumnWidth(0, 330.0f);

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

            // ascending order
            std::sort(m_brsTics.begin(), m_brsTics.end(), [](const BRSTicItem& a, const BRSTicItem& b) {
                return a.ticNumber < b.ticNumber;
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

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Tic Index:");
            ImGui::SameLine(80.0f);
            ImGui::SetNextItemWidth(90.0f);
            ImGui::InputInt("##ticNumber", &tic.ticNumber);

            // run and strafe
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Run:");
            ImGui::SameLine(50.0f);
            ImGui::SetNextItemWidth(90.0f);
            ImGui::InputText("##runStr", tic.runStr, sizeof(tic.runStr));

            ImGui::SameLine(165.0f);
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Strafe:");
            ImGui::SameLine(215.0f);
            ImGui::SetNextItemWidth(90.0f);
            ImGui::InputText("##strafeStr", tic.strafeStr, sizeof(tic.strafeStr));

            // turn
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Turn:");
            ImGui::SameLine(50.0f);
            ImGui::SetNextItemWidth(90.0f);
            ImGui::InputText("##turnStr", tic.turnStr, sizeof(tic.turnStr));

            // fire and use
            const char* triChoices[] = {"No", "Yes"};
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Fire:");
            ImGui::SameLine(50.0f);
            ImGui::SetNextItemWidth(70.0f);
            ImGui::Combo("##fire", &tic.fire, triChoices, 2);

            ImGui::SameLine(165.0f);
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Use:");
            ImGui::SameLine(215.0f);
            ImGui::SetNextItemWidth(70.0f);
            ImGui::Combo("##use", &tic.use, triChoices, 2);
        } else {
            ImGui::TextDisabled("Select a tic above to configure its ranges.");
        }

        // right column
        ImGui::NextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Conditions to match on tic:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        ImGui::InputInt("##targetTicInput", &m_brsTargetTic, 0, 0);
        ImGui::Separator();

        const char* ops[] = {"=", "!=", ">=", ">", "<=", "<", "Min", "Max", "acap"};
        auto RenderCheckRow = [&](const char* label, BRSCheckItem& item, bool allowAcap = false) {
            ImGui::Checkbox(label, &item.active);
            ImGui::SameLine(105.0f);
            ImGui::PushID(label);
            ImGui::SetNextItemWidth(70.0f);
            ImGui::Combo("##op", &item.op, ops, allowAcap ? 9 : 8);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100.0f);
            ImGui::InputText("##val", item.val, sizeof(item.val));
            ImGui::PopID();
        };

        RenderCheckRow("Speed", m_chkSpeed);
        RenderCheckRow("Damage", m_chkDamage);
        RenderCheckRow("X Pos", m_chkX, true);
        RenderCheckRow("Y Pos", m_chkY, true);
        RenderCheckRow("Z Pos", m_chkZ);
        RenderCheckRow("X Mom", m_chkMomX);
        RenderCheckRow("Y Mom", m_chkMomY);
        RenderCheckRow("RNG Index", m_chkRng);
		
		// angle check
        ImGui::Checkbox("Angle", &m_chkAngle.active);
        ImGui::SameLine(105.0f);
        ImGui::PushID("AngleCheck");
        ImGui::SetNextItemWidth(70.0f);
        ImGui::Combo("##op", &m_chkAngle.op, ops, 9); // includes acap
        ImGui::SameLine();
        const char* bfAngleUnits[] = { "LT", "FA", "Deg", "BA" };
        ImGui::SetNextItemWidth(46.0f);
        ImGui::Combo("##unit", &m_chkAngleType, bfAngleUnits, IM_ARRAYSIZE(bfAngleUnits));
        ImGui::SameLine();
        float remainingWidth = 100.0f - 46.0f - ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetNextItemWidth(remainingWidth);
        ImGui::InputText("##val", m_chkAngle.val, sizeof(m_chkAngle.val));
        ImGui::PopID();

        // item check
        ImGui::Checkbox("Item", &m_chkItem.active);
        ImGui::SameLine(105.0f);
        ImGui::PushID("ItemCheck");
        const char* itemModes[] = { "have", "lack" };
        ImGui::SetNextItemWidth(70.0f);
        ImGui::Combo("##mode", &m_chkItemMode, itemModes, 2);
        ImGui::SameLine();
        const char* itemLabels[] = {
            "Red Card (rkc)", "Yellow Card (ykc)", "Blue Card (bkc)",
            "Red Skull (rsk)", "Yellow Skull (ysk)", "Blue Skull (bsk)",
            "Fist (f)", "Pistol (p)", "Shotgun (sg)", "Chaingun (cg)",
            "Rocket L. (rl)", "Plasma (pg)", "BFG (bfg)", "Chainsaw (cs)", "SSG (ssg)"
        };
        ImGui::SetNextItemWidth(100.0f);
        ImGui::Combo("##item", &m_chkItemIndex, itemLabels, IM_ARRAYSIZE(itemLabels));
        ImGui::PopID();

        // linedef action check
        ImGui::Checkbox("Linedef", &m_chkLinedef.active);
        ImGui::SameLine(105.0f);
        ImGui::PushID("LinedefCheck");
        const char* lineTypes[] = {"act", "skip"};
        ImGui::SetNextItemWidth(70.0f);
        ImGui::Combo("##type", &m_chkLinedefType, lineTypes, 2);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
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

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Preset:");
        ImGui::SameLine();
        const char* presets[] = {
            "Same as source",
            "1", "2", "3", "4", "5",
            "Max Range (Pseudo)", "Max Range (True)",
            "lynx 1 (3 tics)", "lynx 2 (3 tics)", "lynx 3 (8-10 tics)"
        };
        ImGui::SetNextItemWidth(155.0f);
        ImGui::Combo("##AutoRangePreset", &m_brsAutoRangePreset, presets, IM_ARRAYSIZE(presets));

        ImGui::SameLine();
        bool applyAutoRanges = ImGui::Button("Apply Auto-Ranges to Tics");

        ImGui::SameLine();
        const char* limitLabels[] = { "Limit: None (Turbo)", "Limit: Non-Turbo", "Limit: Stroller" };
        int limIdx = (m_movementLimit >= 0 && m_movementLimit <= 2) ? m_movementLimit : 1;
        ImGui::TextDisabled("(%s)", limitLabels[limIdx]);

        // gap based fire tic insertion row
        ImGui::Checkbox("Auto-Fire (gap)", &m_brsAutoFireEnabled);
        if (!m_brsAutoFireEnabled) ImGui::BeginDisabled();
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Start Tic:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(75.0f);
        if (ImGui::InputInt("##AutoFireStart", &m_brsAutoFireStartTic)) {
            if (m_brsAutoFireStartTic < 0) m_brsAutoFireStartTic = 0;
        }
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Gap:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(75.0f);
        if (ImGui::InputInt("##AutoFireGap", &m_brsAutoFireGap)) {
            if (m_brsAutoFireGap < 1) m_brsAutoFireGap = 1;
        }
        if (!m_brsAutoFireEnabled) ImGui::EndDisabled();

        if (applyAutoRanges) {
            int runMax    = (m_movementLimit == 0) ? 127 : 50;
            int runMin    = (m_movementLimit == 0) ? -128 : -50;
            int strafeMax = (m_movementLimit == 0) ? 127 : (m_movementLimit == 1 ? 50 : 0);
            int strafeMin = (m_movementLimit == 0) ? -128 : (m_movementLimit == 1 ? -50 : 0);
            int turnMax   = 127;
            int turnMin   = -128;

            // ensure tics are evaluated in ascending ticNumber order
            std::sort(m_brsTics.begin(), m_brsTics.end(), [](const BRSTicItem& a, const BRSTicItem& b) {
                return a.ticNumber < b.ticNumber;
            });

            for (size_t i = 0; i < m_brsTics.size(); ++i) {
                int targetTic = m_brsTics[i].ticNumber;

                if (m_brsAutoRangePreset == 0) {
                    int r = xdre::getTicRun(targetTic);
                    int s = xdre::getTicStrafe(targetTic);
                    int t = xdre::getTicTurn(targetTic);

                    r = (std::max)(runMin, (std::min)(runMax, r));
                    s = (std::max)(strafeMin, (std::min)(strafeMax, s));
                    t = (std::max)(turnMin, (std::min)(turnMax, t));

                    snprintf(m_brsTics[i].runStr, sizeof(m_brsTics[i].runStr), "%d", r);
                    snprintf(m_brsTics[i].strafeStr, sizeof(m_brsTics[i].strafeStr), "%d", s);
                    snprintf(m_brsTics[i].turnStr, sizeof(m_brsTics[i].turnStr), "%d", t);
                } else if (m_brsAutoRangePreset >= 1 && m_brsAutoRangePreset <= 5) {
                    int rad = m_brsAutoRangePreset;
                    int r = xdre::getTicRun(targetTic);
                    int s = xdre::getTicStrafe(targetTic);
                    int t = xdre::getTicTurn(targetTic);

                    BRS_CalcAutoRange(r, rad, runMin, runMax, m_brsTics[i].runStr, sizeof(m_brsTics[i].runStr));
                    BRS_CalcAutoRange(s, rad, strafeMin, strafeMax, m_brsTics[i].strafeStr, sizeof(m_brsTics[i].strafeStr));
                    BRS_CalcAutoRange(t, rad, turnMin, turnMax, m_brsTics[i].turnStr, sizeof(m_brsTics[i].turnStr));
                } else if (m_brsAutoRangePreset == 6) {
                    snprintf(m_brsTics[i].runStr, sizeof(m_brsTics[i].runStr), "%d:%d", runMax, runMin);
                    snprintf(m_brsTics[i].strafeStr, sizeof(m_brsTics[i].strafeStr), "%d:%d", strafeMax, strafeMin);
                    strcpy(m_brsTics[i].turnStr, "32:-31");
                } else if (m_brsAutoRangePreset == 7) {
                    snprintf(m_brsTics[i].runStr, sizeof(m_brsTics[i].runStr), "%d:%d", runMax, runMin);
                    snprintf(m_brsTics[i].strafeStr, sizeof(m_brsTics[i].strafeStr), "%d:%d", strafeMax, strafeMin);
                    strcpy(m_brsTics[i].turnStr, "127:-128");
                } else if (m_brsAutoRangePreset == 8) {
                    int r = xdre::getTicRun(targetTic);
                    int s = xdre::getTicStrafe(targetTic);
                    int t = xdre::getTicTurn(targetTic);

                    int rLow  = (std::max)(runMin, r - 1);
                    int rHigh = (std::min)(runMax, r + 1);
                    snprintf(m_brsTics[i].runStr, sizeof(m_brsTics[i].runStr), "%d:%d",
                             (r == runMax) ? rHigh : rLow,
                             (r == runMax) ? rLow  : rHigh);

                    int sLow  = (std::max)(strafeMin, s - 1);
                    int sHigh = (std::min)(strafeMax, s + 1);
                    snprintf(m_brsTics[i].strafeStr, sizeof(m_brsTics[i].strafeStr), "%d:%d",
                             (s == strafeMax) ? sHigh : sLow,
                             (s == strafeMax) ? sLow  : sHigh);

                    BRS_CalcAutoRange(t, 8, turnMin, turnMax, m_brsTics[i].turnStr, sizeof(m_brsTics[i].turnStr));
                } else if (m_brsAutoRangePreset == 9) {
                    int r = xdre::getTicRun(targetTic);
                    int s = xdre::getTicStrafe(targetTic);
                    int t = xdre::getTicTurn(targetTic);

                    BRS_CalcAutoRange(r, 1, runMin, runMax, m_brsTics[i].runStr, sizeof(m_brsTics[i].runStr));
                    BRS_CalcAutoRange(s, 1, strafeMin, strafeMax, m_brsTics[i].strafeStr, sizeof(m_brsTics[i].strafeStr));
                    BRS_CalcAutoRange(t, 6, turnMin, turnMax, m_brsTics[i].turnStr, sizeof(m_brsTics[i].turnStr));
                } else if (m_brsAutoRangePreset == 10) {
                    int r = xdre::getTicRun(targetTic);
                    int s = xdre::getTicStrafe(targetTic);
                    int t = xdre::getTicTurn(targetTic);

                    r = (std::max)(runMin, (std::min)(runMax, r));
                    s = (std::max)(strafeMin, (std::min)(strafeMax, s));

                    snprintf(m_brsTics[i].runStr, sizeof(m_brsTics[i].runStr), "%d", r);
                    snprintf(m_brsTics[i].strafeStr, sizeof(m_brsTics[i].strafeStr), "%d", s);
                    BRS_CalcAutoRange(t, 1, turnMin, turnMax, m_brsTics[i].turnStr, sizeof(m_brsTics[i].turnStr));
                }

                // apply gap based fire insertion strictly by actual demo ticNumber
                if (m_brsAutoFireEnabled) {
                    long long ticNum   = static_cast<long long>(m_brsTics[i].ticNumber);
                    long long startTic = static_cast<long long>(m_brsAutoFireStartTic);
                    long long gap      = (std::max)(1LL, static_cast<long long>(m_brsAutoFireGap));

                    if (ticNum >= startTic && ((ticNum - startTic) % gap == 0)) {
                        m_brsTics[i].fire = 1; // "Yes"
                    } else {
                        m_brsTics[i].fire = 0; // "No"
                    }
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
        const float fieldWidth = 496.0f;

        // threads
        ImGui::AlignTextToFramePadding();
        ImGui::Text("CPU Workers:");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(fieldWidth);
        ImGui::SliderInt("##CPUWorkers", &m_brsThreadGoal, 1, maxThreads);

        // paths and arguments
        auto RenderLeftLabeledInput = [&](const char* label, const char* id, char* buf, size_t bufSize) {
            ImGui::AlignTextToFramePadding();
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

std::string MainWindow::OpenSegmentFolderDialog() {
#ifdef _WIN32
    BROWSEINFOA bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.lpszTitle = "Select DSDA Segment Folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl != nullptr) {
        char path[MAX_PATH] = "";
        bool ok = (SHGetPathFromIDListA(pidl, path) != FALSE);
        CoTaskMemFree(pidl);
        if (ok && path[0] != '\0') {
            return std::string(path);
        }
    }
#else
    char path[PATH_MAX] = "";
    std::string initDir = GetExeDirectory();
    if (initDir.empty()) initDir = ".";

    auto runDialogCmd = [](const std::string& cmd, char* outBuf, size_t outSize) -> bool {
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return false;
        bool gotLine = (fgets(outBuf, static_cast<int>(outSize), pipe) != nullptr);
        int status = pclose(pipe);
        return (gotLine && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    };

    bool ok = false;
    if (system("command -v kdialog >/dev/null 2>&1") == 0) {
        std::string kdialogCmd = "kdialog --title \"Select DSDA Segment Folder\" --getexistingdirectory \"" + initDir + "\" 2>/dev/null";
        ok = runDialogCmd(kdialogCmd, path, sizeof(path));
    } else if (system("command -v zenity >/dev/null 2>&1") == 0) {
        std::string zenityCmd = "zenity --file-selection --directory --title=\"Select DSDA Segment Folder\" --filename=\"" + initDir + "\" 2>/dev/null";
        ok = runDialogCmd(zenityCmd, path, sizeof(path));
    }

    if (ok) {
        std::string res(path);
        while (!res.empty() && (res.back() == '\n' || res.back() == '\r')) {
            res.pop_back();
        }
        return res;
    }
#endif
    return "";
}

std::string MainWindow::OpenSegmentConfigDialog() {
    char filename[MAX_PATH] = "";
#ifdef _WIN32
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = "DSDA Config Files (*.cfg)\0*.cfg\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "cfg";
    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn)) {
        return std::string(filename);
    }
#else
    std::string initDir = GetExeDirectory();
    if (initDir.empty()) initDir = ".";

    auto runDialogCmd = [](const std::string& cmd, char* outBuf, size_t outSize) -> bool {
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return false;
        bool gotLine = (fgets(outBuf, static_cast<int>(outSize), pipe) != nullptr);
        int status = pclose(pipe);
        return (gotLine && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    };

    bool ok = false;
    if (system("command -v kdialog >/dev/null 2>&1") == 0) {
        std::string kdialogCmd = "kdialog --title \"Select Config File\" --getopenfilename \"" + initDir + "\" \"*.cfg|DSDA Config Files (*.cfg)\" 2>/dev/null";
        ok = runDialogCmd(kdialogCmd, filename, sizeof(filename));
    } else if (system("command -v zenity >/dev/null 2>&1") == 0) {
        std::string zenityCmd = "zenity --file-selection --title=\"Select Config File\" --filename=\"" + initDir + "\" --file-filter=\"DSDA Config Files (*.cfg) | *.cfg\" --file-filter=\"All Files | *\" 2>/dev/null";
        ok = runDialogCmd(zenityCmd, filename, sizeof(filename));
    }

    if (ok) {
        std::string res(filename);
        while (!res.empty() && (res.back() == '\n' || res.back() == '\r')) {
            res.pop_back();
        }
        return res;
    }
#endif
    return "";
}

static std::string FindLatestSegmentOutputDemo(const std::string& dir) {
    std::string bestPath = "";
    long long bestNum = -1; // 0 for base "output.lmp", >= 2 for "output-00002.lmp", etc.

    auto ConsiderFilename = [&](const std::string& name, const std::string& fullPath) {
        std::string lower = name;
        for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        if (lower == "output.lmp") {
            if (bestNum < 1) {
                bestNum = 1;
                bestPath = fullPath;
            }
            return;
        }

        // match "output-<digits>.lmp"
        if (lower.rfind("output-", 0) == 0 && lower.size() > 11 &&
            lower.substr(lower.size() - 4) == ".lmp") {
            std::string numPart = lower.substr(7, lower.size() - 11);
            bool allDigits = !numPart.empty();
            for (char c : numPart) {
                if (!std::isdigit(static_cast<unsigned char>(c))) {
                    allDigits = false;
                    break;
                }
            }
            if (allDigits) {
                try {
                    long long idx = std::stoll(numPart);
                    if (idx > bestNum) {
                        bestNum = idx;
                        bestPath = fullPath;
                    }
                } catch (...) {}
            }
        }
    };

#ifdef _WIN32
    std::string searchPattern = dir + "\\output*.lmp";
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(searchPattern.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                ConsiderFilename(fd.cFileName, dir + "\\" + fd.cFileName);
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
#else
    DIR* d = opendir(dir.c_str());
    if (d) {
        struct dirent* entry;
        while ((entry = readdir(d)) != nullptr) {
            std::string fullPath = dir + "/" + entry->d_name;
            struct stat st;
            if (stat(fullPath.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
                ConsiderFilename(entry->d_name, fullPath);
            }
        }
        closedir(d);
    }
#endif

    return bestPath;
}

void MainWindow::LaunchSegmentInstance(bool withDemo) {
    auto IsRelativePath = [](const std::string& p) -> bool {
#ifdef _WIN32
        return (p.find(":\\") == std::string::npos && p.find(":/") == std::string::npos);
#else
        return (!p.empty() && p[0] != '/');
#endif
    };

    std::string segDir = m_segmentDirPath;
    if (segDir.empty()) segDir = "dsda_segment";
    if (IsRelativePath(segDir)) {
        segDir = GetExeDirectory() + segDir;
    }

    // trim trailing slashes
    while (segDir.size() > 1 && (segDir.back() == '\\' || segDir.back() == '/')) {
        segDir.pop_back();
    }

    Win32_CreateDirectory(segDir);

    #ifdef _WIN32
    std::string exePath = segDir + "\\dsda-doom.exe";
    #else
    std::string exePath = "";
    DIR* d = opendir(segDir.c_str());
    if (d) {
        struct dirent* entry;
        while ((entry = readdir(d)) != nullptr) {
            std::string name = entry->d_name;
            std::string lower = name;
            for (char& c : lower) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }

            std::string candidate = segDir + "/" + name;
            if (!FileExists(candidate)) continue;

            // prefer exact case-insensitive "dsda-doom" binary if present
            if (lower == "dsda-doom") {
                exePath = candidate;
                break;
            }

            // otherwise match any "dsda-doom*.appimage" regardless of case
            if (exePath.empty() &&
                lower.rfind("dsda-doom", 0) == 0 &&
                HasExtension(lower, ".appimage")) {
                exePath = candidate;
                }
        }
        closedir(d);
    }

    if (exePath.empty()) {
        m_segmentStatusMessage = "Error: dsda-doom executable (or dsda-doom*.AppImage) not found in " + segDir;
        return;
    }

    // Ensure executable bit is set so users don't have to manually run chmod +x
    struct stat exeStat;
    if (stat(exePath.c_str(), &exeStat) == 0 && !(exeStat.st_mode & S_IXUSR)) {
        chmod(exePath.c_str(), exeStat.st_mode | S_IXUSR | S_IXGRP | S_IXOTH);
    }
    #endif

    if (!FileExists(exePath)) {
        m_segmentStatusMessage = "Error: dsda-doom executable (or dsda-doom*.AppImage) not found in " + segDir;
        return;
    }

    std::string cmd = "\"" + exePath + "\" ";

    // hardcode iwad using brs logic
    if (strlen(m_brsIwadPath) > 0) {
        std::string iwad = m_brsIwadPath;
        if (IsRelativePath(iwad)) {
            iwad = GetExeDirectory() + iwad;
        }
        cmd += "-iwad \"" + iwad + "\" ";
    }

    // hardcode pwad using brs logic, resolving relative paths against ldre's directory
    if (strlen(m_brsPwadPath) > 0) {
        std::string pwadStr = m_brsPwadPath;
        std::vector<std::string> pwadTokens;
        std::string cur;
        bool inQuotes = false;
        for (char c : pwadStr) {
            if (c == '"') {
                inQuotes = !inQuotes;
            } else if (!inQuotes && std::isspace(static_cast<unsigned char>(c))) {
                if (!cur.empty()) { pwadTokens.push_back(cur); cur.clear(); }
            } else {
                cur.push_back(c);
            }
        }
        if (!cur.empty()) pwadTokens.push_back(cur);

        if (!pwadTokens.empty()) {
            cmd += "-file ";
            for (const auto& rawTok : pwadTokens) {
                std::string resolved = rawTok;
                if (IsRelativePath(resolved)) {
                    std::string inExeDir = GetExeDirectory() + resolved;
                    std::string inLastDir = m_lastDirectory.empty() ? "" : (m_lastDirectory + "/" + resolved);
                    if (FileExists(inExeDir)) {
                        resolved = inExeDir;
                    } else if (!inLastDir.empty() && FileExists(inLastDir)) {
                        resolved = inLastDir;
                    } else {
                        resolved = inExeDir;
                    }
                }
                cmd += "\"" + resolved + "\" ";
            }
        }
    }
	
	// hardcode -config only when checkbox is enabled and path is non-empty
    if (m_segmentUseCustomConfig && strlen(m_segmentConfigPath) > 0) {
        std::string cfgPath = m_segmentConfigPath;
        if (IsRelativePath(cfgPath)) {
            cfgPath = GetExeDirectory() + cfgPath;
        }
        cmd += "-config \"" + cfgPath + "\" ";
    }

    if (withDemo) {
        int complevel = GetSelectedComplevel();

        int headerOpts[28] = { 0 };
        xdre::getOptions(headerOpts);
        int curEpisode = headerOpts[2];
        int curMap     = headerOpts[3];
        if (curMap <= 0) curMap = 1;

        if (!headerOpts[27]) {
            cmd += "-shorttics ";
        } else {
            cmd += "-longtics ";
        }

        // hardcode complevel using brs logic
        if (std::string(m_segmentExtraArgs).find("-complevel") == std::string::npos &&
            std::string(m_segmentExtraArgs).find("-cl") == std::string::npos) {
            cmd += "-complevel " + std::to_string(complevel) + " ";
        }

        int levelStartTic = static_cast<int>(xdre::getLevelStartTic());
        if (levelStartTic > 0 && std::string(m_segmentExtraArgs).find("-warp") == std::string::npos) {
            if (complevel == 3) {
                cmd += "-warp " + std::to_string(curEpisode > 0 ? curEpisode : 1) + " " + std::to_string(curMap) + " ";
            } else {
                cmd += "-warp " + std::to_string(curMap) + " ";
            }
        }

        // hardcode -skiptic TIC
        int skipTic = 0;
        if (levelStartTic > 0 && m_segmentSkipTic >= levelStartTic) {
            skipTic = (std::max)(0, m_segmentSkipTic - levelStartTic - 1);
        } else {
            skipTic = (std::max)(0, m_segmentSkipTic - 1);
        }
        cmd += "-skiptic " + std::to_string(skipTic) + " ";

        // hardcode -game_speed (clamped 5..1000)
        int gameSpeed = (std::max)(5, (std::min)(1000, m_segmentGameSpeed));
        if (std::string(m_segmentExtraArgs).find("-game_speed") == std::string::npos) {
            cmd += "-game_speed " + std::to_string(gameSpeed) + " ";
        }

        // create output directory next to ldre executable
        std::string outDemoDir = GetSegmentDemoOutputDirectory();
        Win32_CreateDirectory(outDemoDir);

#ifdef _WIN32
        std::string outDemoPath = outDemoDir + "\\output.lmp";
#else
        std::string outDemoPath = outDemoDir + "/output.lmp";
#endif

        // re-save current editor state right before launching in case tics changed
        if (strlen(m_segmentDemoPath) > 0) {
            FILE* f = std::fopen(m_segmentDemoPath, "wb");
            if (f) {
                xdre::save(f);
                std::fclose(f);
            }
            cmd += "-recordfromto \"" + std::string(m_segmentDemoPath) + "\" \"" + outDemoPath + "\" ";
        }

        if (strlen(m_segmentExtraArgs) > 0) {
            cmd += std::string(m_segmentExtraArgs) + " ";
        }
		
		cmd += "-tas ";
    }

    WriteConfig();

#ifdef _WIN32
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    std::vector<char> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back('\0');

    if (CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, FALSE, 0, NULL, segDir.c_str(), &si, &pi)) {
        CloseHandle(pi.hThread);
        if (withDemo) {
            HANDLE hProc = pi.hProcess;
            uint64_t launchGen = ++m_segmentLaunchGen;
            std::string watchDir = GetSegmentDemoOutputDirectory();
            std::thread([this, hProc, watchDir, launchGen]() {
                WaitForSingleObject(hProc, INFINITE);
                CloseHandle(hProc);
                if (m_segmentAutoLoadResult && m_segmentLaunchGen.load() == launchGen) {
                    std::string latest = FindLatestSegmentOutputDemo(watchDir);
                    if (!latest.empty() && FileExists(latest)) {
                        m_segmentPendingReloadPath = latest;
                        m_segmentPendingReload = true;
                    }
                }
            }).detach();
        } else {
            CloseHandle(pi.hProcess);
        }
        m_segmentStatusMessage = withDemo
            ? "Launched isolated dsda-doom instance."
            : "Launched isolated dsda-doom instance (no demo).";
    } else {
        m_segmentStatusMessage = "Failed to launch dsda-doom process.";
    }
#else
    std::vector<std::string> argStrings = SplitCommandLinePosix(cmd);
    if (!argStrings.empty()) {
        pid_t pid = fork();
        if (pid == 0) {
            if (chdir(segDir.c_str()) != 0) {
                _exit(127);
            }
            std::vector<char*> argv;
            argv.reserve(argStrings.size() + 1);
            for (auto& s : argStrings) {
                argv.push_back(&s[0]);
            }
            argv.push_back(nullptr);
            execvp(argv[0], argv.data());
            _exit(127);
        } else if (pid > 0) {
            if (withDemo) {
                uint64_t launchGen = ++m_segmentLaunchGen;
                std::string watchDir = GetSegmentDemoOutputDirectory();
                std::thread([this, pid, watchDir, launchGen]() {
                    int status = 0;
                    waitpid(pid, &status, 0);
                    if (m_segmentAutoLoadResult && m_segmentLaunchGen.load() == launchGen) {
                        std::string latest = FindLatestSegmentOutputDemo(watchDir);
                        if (!latest.empty() && FileExists(latest)) {
                            m_segmentPendingReloadPath = latest;
                            m_segmentPendingReload = true;
                        }
                    }
                }).detach();
            }
            m_segmentStatusMessage = withDemo
                ? "Launched isolated dsda-doom instance."
                : "Launched isolated dsda-doom instance (no demo).";
        } else {
            m_segmentStatusMessage = "Failed to fork dsda-doom process.";
        }
    }
#endif
}

void MainWindow::RenderSegmentDialog() {
    if (m_showSegmentDialog) {
        ImGui::OpenPopup("Slowmotion/Segment");
        m_showSegmentDialog = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Slowmotion/Segment", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        const float labelWidth = 130.0f;
        const float fieldWidth = 300.0f;

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Instance Folder:");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(fieldWidth);
        ImGui::InputText("##SegmentFolder", m_segmentDirPath, sizeof(m_segmentDirPath));
        ImGui::SameLine();
        if (ImGui::Button("Browse...##SegDir")) {
            std::string chosenDir = OpenSegmentFolderDialog();
            if (!chosenDir.empty()) {
                std::strncpy(m_segmentDirPath, chosenDir.c_str(), sizeof(m_segmentDirPath) - 1);
                m_segmentDirPath[sizeof(m_segmentDirPath) - 1] = '\0';
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset to Default##SegDir")) {
            std::strncpy(m_segmentDirPath, "dsda_segment", sizeof(m_segmentDirPath) - 1);
            m_segmentDirPath[sizeof(m_segmentDirPath) - 1] = '\0';
        }

        ImGui::Checkbox("Custom Config:##UseCustomCfg", &m_segmentUseCustomConfig);
        ImGui::SameLine(labelWidth);
        if (!m_segmentUseCustomConfig) ImGui::BeginDisabled();
        ImGui::SetNextItemWidth(fieldWidth);
        ImGui::InputText("##SegmentConfigPath", m_segmentConfigPath, sizeof(m_segmentConfigPath));
        ImGui::SameLine();
        if (ImGui::Button("Browse...##SegCfg")) {
            std::string chosenCfg = OpenSegmentConfigDialog();
            if (!chosenCfg.empty()) {
                std::strncpy(m_segmentConfigPath, chosenCfg.c_str(), sizeof(m_segmentConfigPath) - 1);
                m_segmentConfigPath[sizeof(m_segmentConfigPath) - 1] = '\0';
            }
        }
        if (!m_segmentUseCustomConfig) ImGui::EndDisabled();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Skip Tic:");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::InputInt("##SegmentSkipTic", &m_segmentSkipTic)) {
            if (m_segmentSkipTic < 0) m_segmentSkipTic = 0;
        }

        // game speed [5,1000]
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Game Speed:");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::InputInt("##SegmentGameSpeed", &m_segmentGameSpeed, 0, 0, ImGuiInputTextFlags_CharsDecimal)) {
            if (m_segmentGameSpeed < 5) m_segmentGameSpeed = 5;
            if (m_segmentGameSpeed > 1000) m_segmentGameSpeed = 1000;
        }

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Extra Params:");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(fieldWidth);
        ImGui::InputText("##SegmentExtraArgs", m_segmentExtraArgs, sizeof(m_segmentExtraArgs));

        ImGui::Separator();

        if (ImGui::Button("Launch Game", ImVec2(130, 0))) {
            LaunchSegmentInstance(true);
        }
        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(100, 0))) {
            WriteConfig();
            ImGui::CloseCurrentPopup();
        }

        if (!m_segmentStatusMessage.empty()) {
            ImGui::Spacing();
            ImGui::TextWrapped("%s", m_segmentStatusMessage.c_str());
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Launch DSDA instance without demo")) {
            LaunchSegmentInstance(false);
        }

        const char* loadLatestLabel = "Load Latest Segment Demo";
        float loadBtnWidth = ImGui::CalcTextSize(loadLatestLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        float rightEdge = ImGui::GetWindowContentRegionMax().x;
        ImGui::SameLine();
        ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(), rightEdge - loadBtnWidth));

        if (ImGui::Button(loadLatestLabel)) {
            std::string watchDir = GetSegmentDemoOutputDirectory();
            std::string latest = FindLatestSegmentOutputDemo(watchDir);
            if (!latest.empty() && FileExists(latest)) {
                m_segmentPendingReloadPath = latest;
                m_segmentPendingReload = true;
                m_segmentStatusMessage = "Loaded latest segment demo.";
            } else {
                m_segmentStatusMessage = "No segment output demo found.";
            }
        }

        ImGui::EndPopup();
    }
}
