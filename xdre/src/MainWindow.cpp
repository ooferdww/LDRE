/* LDRE: User interface
 * Authors: ooferdww */

#include "MainWindow.hpp"
#include "../imgui/imgui.h"
#include "SDL.h"

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cctype>

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

// bruteforce parser
static std::vector<signed char> ParseCmds(const std::string& str, bool parseAsUnsigned = false) {
    long limitMax = parseAsUnsigned ? 255 : 127;
    long limitMin = parseAsUnsigned ? 0 : -128;
    std::vector<signed char> ret;

    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (item.empty()) continue;
        auto colon = item.find(':');
        if (colon == std::string::npos) {
            long val = std::stol(item);
            if (val >= limitMin && val <= limitMax) ret.push_back((signed char)val);
        } else {
            long start = std::stol(item.substr(0, colon));
            long end = std::stol(item.substr(colon + 1));
            if (start <= end) {
                for (long v = start; v <= end; ++v) ret.push_back((signed char)v);
            } else {
                for (long v = start; v >= end; --v) ret.push_back((signed char)v);
            }
        }
    }
    return ret;
}

// window core

MainWindow::MainWindow() {
    Init();
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
    LoadIni("xdre.ini");

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
	
	IniWrite("GuiWindowX", std::to_string(m_guiPosX));
    IniWrite("GuiWindowY", std::to_string(m_guiPosY));
    IniWrite("DoomWindowX", std::to_string(m_doomPosX));
    IniWrite("DoomWindowY", std::to_string(m_doomPosY));

    SaveIni("xdre.ini");
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
        case XCmd::RunBwd:        xdre::run(-static_cast<signed char>(std::abs((long)(int)val))); break;
        case XCmd::RunFwd:        xdre::run(static_cast<signed char>((long)(int)val));            break;
        case XCmd::StrafeLeft:    xdre::strafe(-static_cast<signed char>(std::abs((long)(int)val))); break;
        case XCmd::StrafeRight:   xdre::strafe(static_cast<signed char>((long)(int)val));         break;
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
    RenderBruteForceDialog();
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
            if (ImGui::MenuItem("Brute force - Standard...")) {
                m_checkTic = xdre::getCurrentTic();
                m_showBruteDialog = true;
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

        ImGui::Separator();
        if (ImGui::Button("Save & Apply", ImVec2(120, 0))) {
            m_bloodEnabled = m_bloodEditBuffer;
            m_invertTicScrub = m_invertTicScrubEditBuffer;

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

void MainWindow::RenderBruteForceDialog() {
    if (m_showBruteDialog) {
        ImGui::OpenPopup("Brute Force - Standard");
        m_showBruteDialog = false;
    }

    if (m_bruteFinishedNotification) {
        m_bruteFinishedNotification = false;
        ImGui::OpenPopup("Brute Result");
    }

    if (ImGui::BeginPopupModal("Brute Result", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (m_bruteSuccess) {
            ImGui::Text("Found it! Replay positioned at desired tic.");
        } else {
            ImGui::Text("Whole range examined. No cigar.");
        }
        if (ImGui::Button("OK", ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Brute Force - Standard", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Columns(2, "BruteCols", false);
        ImGui::SetColumnWidth(0, 240.0f);

        ImGui::Text("Tics to Brute Force:");
        if (ImGui::Button("Add Tic")) {
            BruteItem item;
            int nextTic = xdre::getCurrentTic() + static_cast<int>(m_bruteItems.size());
            snprintf(item.ticStr, sizeof(item.ticStr), "%d", nextTic);
            m_bruteItems.push_back(item);
            m_selectedBruteItem = static_cast<int>(m_bruteItems.size()) - 1;
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove") && m_selectedBruteItem >= 0 && m_selectedBruteItem < (int)m_bruteItems.size()) {
            m_bruteItems.erase(m_bruteItems.begin() + m_selectedBruteItem);
            if (m_selectedBruteItem >= (int)m_bruteItems.size()) m_selectedBruteItem = (int)m_bruteItems.size() - 1;
        }

        // tic item list
        ImGui::BeginChild("BruteItemList", ImVec2(0, 140), true);
        for (int i = 0; i < (int)m_bruteItems.size(); ++i) {
            char lbl[32];
            snprintf(lbl, sizeof(lbl), "Tic %s", m_bruteItems[i].ticStr);
            if (ImGui::Selectable(lbl, m_selectedBruteItem == i)) {
                m_selectedBruteItem = i;
            }
        }
        ImGui::EndChild();

        // selected tic config
        if (m_selectedBruteItem >= 0 && m_selectedBruteItem < (int)m_bruteItems.size()) {
            auto& item = m_bruteItems[m_selectedBruteItem];
            ImGui::InputText("Tic", item.ticStr, sizeof(item.ticStr));
            ImGui::InputText("Run (MB:MF)", item.runStr, sizeof(item.runStr));
            ImGui::InputText("Strafe (SL:SR)", item.strafeStr, sizeof(item.strafeStr));
            ImGui::InputText("Turn", item.turnStr, sizeof(item.turnStr));

            const char* turnModes[] = { "Short", "Long" };
            ImGui::Combo("Turn Mode", &item.turnOrAngle, turnModes, 2);

            const char* radioChoices[] = { "No", "Both", "Yes" };
            ImGui::Combo("Fire", &item.fire, radioChoices, 3);
            ImGui::Combo("Use", &item.use, radioChoices, 3);
        }

        ImGui::Separator();
        const char* rangeSizes[] = { "1", "2", "3", "4", "5", "Max Range" };
        ImGui::Combo("Range Size", &m_autoRangeChoice, rangeSizes, 6);
        ImGui::Checkbox("Avoid Turbo (>50)", &m_avoidTurbo);

        if (ImGui::Button("Auto Range Current Tics")) {
            int rangeSize = m_autoRangeChoice + 1;
            for (size_t i = 0; i < m_bruteItems.size(); ++i) {
                int run = xdre::getCurrentRun(i, m_bruteItems.size());
                int strafe = xdre::getCurrentStrafe(i, m_bruteItems.size());
                int turn = xdre::getCurrentTurn(i, m_bruteItems.size());

                snprintf(m_bruteItems[i].runStr, sizeof(m_bruteItems[i].runStr), "%d:%d", run - rangeSize, run + rangeSize);
                snprintf(m_bruteItems[i].strafeStr, sizeof(m_bruteItems[i].strafeStr), "%d:%d", strafe - rangeSize, strafe + rangeSize);
                snprintf(m_bruteItems[i].turnStr, sizeof(m_bruteItems[i].turnStr), "%d:%d", turn - rangeSize - 1, turn + rangeSize + 1);
            }
        }

        ImGui::NextColumn();

        // bruteforce target checks, bruteforce is currently awful and will be replaced with new system in next update
        ImGui::Text("Conditions to Match:");
        ImGui::InputInt("On Tic", &m_checkTic);
        ImGui::Checkbox("Require Use Success", &m_checkUse);

        const char* compOps[] = { "=", ">=", ">", "<=", "<" };

        ImGui::Checkbox("RNG", &m_rngActive); ImGui::SameLine(100);
        ImGui::Combo("##RngOp", &m_rngOp, "= \0!= \0"); ImGui::SameLine(180);
        ImGui::InputText("##RngVal", m_rngVal, sizeof(m_rngVal));

        ImGui::Checkbox("Damage", &m_damageActive); ImGui::SameLine(100);
        ImGui::Combo("##DmgOp", &m_damageOp, compOps, 5); ImGui::SameLine(180);
        ImGui::InputInt("##DmgVal", &m_damageVal);

        ImGui::Checkbox("X Pos", &m_xPosActive); ImGui::SameLine(100);
        ImGui::Combo("##XOp", &m_xPosOp, compOps, 5); ImGui::SameLine(180);
        ImGui::InputDouble("##XVal", &m_xPosVal);

        ImGui::Checkbox("Y Pos", &m_yPosActive); ImGui::SameLine(100);
        ImGui::Combo("##YOp", &m_yPosOp, compOps, 5); ImGui::SameLine(180);
        ImGui::InputDouble("##YVal", &m_yPosVal);

        ImGui::Checkbox("Z Pos", &m_zPosActive); ImGui::SameLine(100);
        ImGui::Combo("##ZOp", &m_zPosOp, compOps, 5); ImGui::SameLine(180);
        ImGui::InputDouble("##ZVal", &m_zPosVal);

        ImGui::Checkbox("X Mom", &m_xMomActive); ImGui::SameLine(100);
        ImGui::Combo("##XMOp", &m_xMomOp, compOps, 5); ImGui::SameLine(180);
        ImGui::InputDouble("##XMVal", &m_xMomVal);

        ImGui::Checkbox("Y Mom", &m_yMomActive); ImGui::SameLine(100);
        ImGui::Combo("##YMOp", &m_yMomOp, compOps, 5); ImGui::SameLine(180);
        ImGui::InputDouble("##YMVal", &m_yMomVal);

        ImGui::Checkbox("Speed", &m_speedActive); ImGui::SameLine(100);
        ImGui::Combo("##SpdOp", &m_speedOp, compOps, 5); ImGui::SameLine(180);
        ImGui::InputDouble("##SpdVal", &m_speedVal);

        ImGui::Columns(1);
        ImGui::Separator();

        // control execution
        if (m_bruteRunning) {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Brute force running...");
            if (ImGui::Button("Abort Search")) {
                m_abortBrute = true;
            }
        } else {
            if (ImGui::Button("Start Search", ImVec2(120, 0))) {
                if (!m_bruteItems.empty()) {
                    if (m_bruteThread.joinable()) m_bruteThread.join();

                    m_abortBrute = false;
                    m_bruteRunning = true;
                    m_bruteSuccess = false;

                    // check structure
                    BruteCheck check;
                    check.tic = m_checkTic;
                    check.use = m_checkUse;

                    check.rngFunc = [this](int val) {
                        if (!m_rngActive) return true;
                        auto indexes = ParseCmds(m_rngVal, true);
                        bool matches = false;
                        for (auto idx : indexes) if (idx == val) { matches = true; break; }
                        return (m_rngOp == 0) ? matches : !matches;
                    };

                    auto MakeCmp = [](bool active, int op, double target) {
                        return [active, op, target](double val) {
                            if (!active) return true;
                            switch (op) {
                                case 0: return val == target;
                                case 1: return val >= target;
                                case 2: return val > target;
                                case 3: return val <= target;
                                case 4: return val < target;
                            }
                            return true;
                        };
                    };

                    check.damageFunc = [this](int val) {
                        if (!m_damageActive) return true;
                        switch (m_damageOp) {
                            case 0: return val == m_damageVal;
                            case 1: return val >= m_damageVal;
                            case 2: return val > m_damageVal;
                            case 3: return val <= m_damageVal;
                            case 4: return val < m_damageVal;
                        }
                        return true;
                    };

                    check.xPosFunc  = MakeCmp(m_xPosActive,  m_xPosOp,  m_xPosVal);
                    check.yPosFunc  = MakeCmp(m_yPosActive,  m_yPosOp,  m_yPosVal);
                    check.zPosFunc  = MakeCmp(m_zPosActive,  m_zPosOp,  m_zPosVal);
                    check.xMomFunc  = MakeCmp(m_xMomActive,  m_xMomOp,  m_xMomVal);
                    check.yMomFunc  = MakeCmp(m_yMomActive,  m_yMomOp,  m_yMomVal);
                    check.speedFunc = MakeCmp(m_speedActive, m_speedOp, m_speedVal);

                    // build tics
                    std::vector<BruteTic> tics;
                    for (auto const& it : m_bruteItems) {
                        BruteTic b;
                        b.tic = std::stoul(it.ticStr);
                        b.fire = it.fire - 1;
                        b.use = it.use - 1;
                        b.turnsAreAngles = (it.turnOrAngle == 1);
                        b.runCmds = ParseCmds(it.runStr);
                        b.strafeCmds = ParseCmds(it.strafeStr);
                        b.turnCmds = ParseCmds(it.turnStr, b.turnsAreAngles);
                        tics.push_back(b);
                    }

                    // Launch async worker
                    m_bruteThread = std::thread([this, tics, check]() {
                        bool res = xdre::bruteForce(tics, check, m_abortBrute);
                        m_bruteSuccess = res;
                        m_bruteRunning = false;
                        m_bruteFinishedNotification = true;
                    });
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Close", ImVec2(100, 0))) {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }
}