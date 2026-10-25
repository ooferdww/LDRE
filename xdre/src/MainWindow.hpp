/*
 * This file is part of XDRE.
 *
 * XDRE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * XDRE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with XDRE.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Этот файл — часть XDRE.
 *
 * XDRE — свободная программа: вы можете перераспространять её и/или
 * изменять её на условиях Стандартной общественной лицензии GNU в том виде,
 * в каком она была опубликована Фондом свободного программного обеспечения;
 * либо версии 2 лицензии, либо (по вашему выбору) любой более поздней
 * версии.
 *
 * XDRE распространяется в надежде, что она будет полезной,
 * но БЕЗО ВСЯКИХ ГАРАНТИЙ; даже без неявной гарантии ТОВАРНОГО ВИДА
 * или ПРИГОДНОСТИ ДЛЯ ОПРЕДЕЛЕННЫХ ЦЕЛЕЙ. Подробнее см. в Стандартной
 * общественной лицензии GNU.
 *
 * Вы должны были получить копию Стандартной общественной лицензии GNU
 * вместе с этой программой. Если это не так, см.
 * <http://www.gnu.org/licenses/>.
 *
 * Description: main window declarations.
*/

#ifndef __MAINWINDOW_HPP
#define __MAINWINDOW_HPP

#include <functional>

#include <wx/stattext.h>
#include <wx/statbox.h>
#include <wx/menu.h>
#include <wx/textctrl.h>
#include <wx/checkbox.h>
#include <wx/panel.h>
#include <wx/choice.h>
#include <wx/frame.h>
#include "TiclistCtrl.hpp"
#include <wx/combobox.h>
#include <wx/button.h>
#include <wx/fileconf.h>

enum class XCmd {
    //commands without value, let exit be last of these
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
    //commands with one digit value, let weapon be last of these
    ChangePlayer,
    TransposeLeft,
    TransposeRight,
    Weapon,
    //commands with char
    RunBwd,
    RunFwd,
    StrafeLeft,
    StrafeRight,
    TurnStep,
    //commands with bigger value
    CopyTicX,
    DeleteTicX,
    TurnLeft,
    TurnRight,
    BackwardDemo,
    ForwardDemo
};


WX_DECLARE_STRING_HASH_MAP(XCmd, StringToCommandMap);

class MainWindow : public wxFrame {
public:
    MainWindow(wxWindow* parent = NULL, wxWindowID id = wxID_ANY, wxPoint const& pos = wxDefaultPosition, wxSize const& size = wxDefaultSize);
    virtual ~MainWindow();
    void SetKeys();
#ifdef _WIN32
    wxFileConfig config {"xdre", "wxEmptyString", "xdre.ini"};
#else
    wxFileConfig config {"xdre", "wxEmptyString", ".xdre/xdre"};
#endif // _WIN32

    wxPanel* panel;
    wxMenuBar *menuBar;
    wxMenu *menuFile, *menuDemo, *menuTools, *menuOptions;
    wxMenuItem *menuFileLoad, *menuFileSave, *menuFileExit, *menuToolsBrute, *menuToolsHeader, *menuOptionsKeys, *menuOptionsClearTraces, *menuOptionsPalette;
    TiclistCtrl *ticlist;

    //Linedef Tracking
    wxStaticBox *linedefBox;
    wxStaticText *linedefLinedefText, *linedefHitSpecialText, *linedefXText, *linedefYText, *linedefV1Text, *linedefV2Text, *linedefDistFromLineText;
    wxComboBox *linedefInputField;
    wxButton *linedefAddButton;
    wxTextCtrl *linedefCrossedTextCtrl, *linedefXV1TextCtrl, *linedefYV1TextCtrl, *linedefXV2TextCtrl, *linedefYV2TextCtrl, *linedefDistanceFromLineTextCtrl; 

    //Sector Tracking
    wxStaticBox *sectorBox;
    wxStaticText *sectorSectorTrackingText, *sectorSectorText, *sectorFloorText, *sectorCeilingText, *sectorEffectText; 
    wxComboBox *sectorInputField;
    wxButton *sectorAddButton;
    wxTextCtrl *sectorFloorHeightTextCtrl, *sectorCeilingHeightTextCtrl, *sectorSpecialTextCtrl;

    //Thing Tracking
    wxStaticBox *thingBox;
    wxStaticText *thingThingText, *thingTicsText, *thingXText, *thingHealthText, *thingYText, *thingReactionTimeText, *thingZText, *thingThresholdText, *thingRadiusText; 
    wxComboBox *thingInputField;
    wxButton *thingAddButton;
    wxTextCtrl *thingXTextCtrl, *thingYTextCtrl, *thingZTextCtrl, *thingRadiusTextCtrl, *thingTicsTextCtrl, *thingHealthTextCtrl, *thingReactionTimeTextCtrl, *thingThresholdTextCtrl;

    //Player Tracking
    wxStaticBox *playerBox;
    wxStaticText *playerXText, *playerYText, *playerZText, *playerXDistText, *playerYDistText, *playerZDistText, *playerXMomText, *playerYMomText, *playerDistMovedText, *playerDirMovedText;
    wxTextCtrl *playerXPosTextCtrl, *playerYPosTextCtrl, *playerZPosTextCtrl, *playerXMomTextCtrl, *playerYMomTextCtrl, *playerXDistTextCtrl, *playerYDistTextCtrl, *playerZDistTextCtrl, *playerDistMovedTextCtrl, *playerDirMovedTextCtrl;

    //General
    wxStaticBox *generalBox;
    wxStaticText *generalInputText, *generalRNGText, *generalAngleText, *generalTicText, *generalDamageText, *generalSaveText, *generalTimeText, *generalUsedText;
    wxTextCtrl *generalInputField, *generalRNGIndexTextCtrl, *generalPlayerAngleTextCtrl, *generalCurrentTicTextCtrl, *generalDamageDoneTextCtrl, *generalSavepointTicTextCtrl, *generalTimeTextCtrl, *generalUseSuccessTextCtrl;
    wxChoice *generalAngleTypeChoice, *generalSavepointStyleChoice;

	void RS();

protected:
    static const long IDM_LOAD, IDM_SAVE, IDM_EXIT, IDM_BRUTE, IDM_PALSTUFF, IDM_SETKEYS, IDM_HEADER, IDM_CLEARTRACES, IDM_IDLEREFRESH;

private:
    void DoInput(XCmd cmd, unsigned int val = 0);
    void RefreshStuff();
    void WriteConfig();
    void Save();
    void Load();
    void Quit(bool canVeto = true);

    wxStringToStringHashMap configToStringMap {}; // config -> textCommand
    StringToCommandMap stringToCommandMap {}; // textCommand -> XCmd

    void OnIdle(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);
    void OnInputFieldText(wxCommandEvent& event);
    void OnAngleType(wxCommandEvent& event);
    void OnLinedefInput(wxCommandEvent& event);
    void OnLineAdd(wxCommandEvent& event);
    void OnSectorInput(wxCommandEvent& event);
    void OnSectorAdd(wxCommandEvent& event);
    void OnThingInput(wxCommandEvent& event);
    void OnThingAdd(wxCommandEvent& event);
    void OnMenuExit(wxCommandEvent& event);
    void OnMenuLoad(wxCommandEvent& event);
    void OnMenuSave(wxCommandEvent& event);
    void OnMenuBrute(wxCommandEvent& event);
    void OnMenuSetKeys(wxCommandEvent& event);
    void OnMenuPalStuff(wxCommandEvent& event);
    void OnMenuHeader(wxCommandEvent& event);
    void OnMenuClearTraces(wxCommandEvent& event);
    void OnStyleChoiceSelect(wxCommandEvent& event);

    DECLARE_EVENT_TABLE()
};

#endif // __MAINWINDOW_HPP

