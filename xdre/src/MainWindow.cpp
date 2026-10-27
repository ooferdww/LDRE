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
 * Description: main XDRE window (command line, traces etc.).
*/

#include "MainWindow.hpp"

#include <wx/intl.h>
#include <wx/string.h>
#include <wx/valnum.h>
#include <wx/statbox.h>
#include <wx/msgdlg.h>
#include <wx/filedlg.h>
#include <wx/ffile.h>
#include <wx/tokenzr.h>
#include <wx/window.h>
#include "BruteDialog.hpp"
#include "HeaderDialog.hpp"
#include "KeysDialog.hpp"
#include "xdre.hpp"
#include "config.h"

//for wxSscanf()
#include <wx/crt.h>

//for SDL_SetWindowPosition()
#include "SDL.h"
extern SDL_Window *sdl_window;

const long MainWindow::IDM_LOAD = wxNewId();
const long MainWindow::IDM_SAVE = wxNewId();
const long MainWindow::IDM_EXIT = wxNewId();
const long MainWindow::IDM_BRUTE = wxNewId();
const long MainWindow::IDM_PALSTUFF = wxNewId();
const long MainWindow::IDM_SETKEYS = wxNewId();
const long MainWindow::IDM_HEADER = wxNewId();
const long MainWindow::IDM_CLEARTRACES = wxNewId();

BEGIN_EVENT_TABLE(MainWindow, wxFrame)
END_EVENT_TABLE()

MainWindow::MainWindow(wxWindow* parent, wxWindowID id, wxPoint const& pos, wxSize const& size) {
    SetKeys();

    Create(parent, id, PACKAGE_STRING, wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE);

    // Couldn't find a way to prevent wxWindow resizing in documentation
    SetClientSize(wxSize(720, 632));
    SetMinSize(wxSize(720, 632));
    SetMaxSize(wxSize(720, 632));

    int x, y;
    //"-1,-1" is wxDefaultPosition
    wxSscanf(config.Read("WindowPos", "-1, -1"), "%d, %d", &x, &y);
    Move(x, y);

    wxString str(config.Read("DoomWindowPos", "center"));
    if (str == "centre" || str == "center")
        SDL_SetWindowPosition(sdl_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    else {
        wxSscanf(str, "%d, %d", &x, &y);
        SDL_SetWindowPosition(sdl_window, x, y);
    }

    str = config.Read("SavepointMode", "1");
    str.ToLong((long*)&x);
    if (x < SP_NOSAVE && x > SP_AUTO)
        x = SP_START;
    xdre::setSavepointStyle(x);
    str.Clear();

    panel = new wxPanel(this);

    ticlist = new TiclistCtrl(panel, wxID_ANY, wxPoint(8, 8), wxSize(200, 557));

    //Linedef Tracking
    linedefBox              = new wxStaticBox(panel,  wxID_ANY, "Linedef Tracking",   wxPoint(216, 5), wxSize(204, 170), 0);
    linedefLinedefText      = new wxStaticText(panel, wxID_ANY, "Linedef",            wxPoint(224, 25));
    linedefHitSpecialText   = new wxStaticText(panel, wxID_ANY, "Hit special",        wxPoint(224, 51));
    linedefXText            = new wxStaticText(panel, wxID_ANY, "X",                  wxPoint(308, 72));
    linedefYText            = new wxStaticText(panel, wxID_ANY, "Y",                  wxPoint(376, 72));
    linedefV1Text           = new wxStaticText(panel, wxID_ANY, "V. 1",               wxPoint(224, 94));
    linedefV2Text           = new wxStaticText(panel, wxID_ANY, "V. 2",               wxPoint(224, 120));
    linedefDistFromLineText = new wxStaticText(panel, wxID_ANY, "Dist. from line",    wxPoint(224, 146));
    linedefInputField       = new wxComboBox(panel, wxID_ANY, wxEmptyString, wxPoint(278, 23), wxSize(100, 21), 0, 0, 0, wxIntegerValidator<unsigned int>());
    linedefInputField->SetMaxLength(10);
    linedefInputField->Bind(wxEVT_TEXT, wxCommandEventHandler(MainWindow::OnLinedefInput), this);
    linedefInputField->Bind(wxEVT_COMBOBOX, wxCommandEventHandler(MainWindow::OnLinedefInput), this);    wxStringTokenizer tok(config.Read("LinedefNums", "0"), ",");
    while (tok.HasMoreTokens())
        linedefInputField->Append(tok.GetNextToken());
    linedefInputField->SetSelection(0);
    linedefInputField->Bind(wxEVT_TEXT, wxCommandEventHandler(MainWindow::OnLinedefInput), this);
    linedefInputField->Bind(wxEVT_COMBOBOX, wxCommandEventHandler(MainWindow::OnLinedefInput), this);
    linedefAddButton                = new wxButton(panel,   wxID_ANY, "Add",   wxPoint(385, 23),  wxSize(21, 21), 0);
    linedefAddButton->Bind(wxEVT_BUTTON, wxCommandEventHandler(MainWindow::OnLineAdd), this);
    linedefCrossedTextCtrl          = new wxTextCtrl(panel, wxID_ANY, "false", wxPoint(346, 49),  wxSize(60, 21), wxTE_READONLY|wxTE_RIGHT);
    linedefXV1TextCtrl              = new wxTextCtrl(panel, wxID_ANY, "0",     wxPoint(278, 92),  wxSize(60, 21), wxTE_READONLY|wxTE_RIGHT);
    linedefYV1TextCtrl              = new wxTextCtrl(panel, wxID_ANY, "0",     wxPoint(346, 92),  wxSize(60, 21), wxTE_READONLY|wxTE_RIGHT);
    linedefXV2TextCtrl              = new wxTextCtrl(panel, wxID_ANY, "0",     wxPoint(278, 118), wxSize(60, 21), wxTE_READONLY|wxTE_RIGHT);
    linedefYV2TextCtrl              = new wxTextCtrl(panel, wxID_ANY, "0",     wxPoint(346, 118), wxSize(60, 21), wxTE_READONLY|wxTE_RIGHT);
    linedefDistanceFromLineTextCtrl = new wxTextCtrl(panel, wxID_ANY, "0",     wxPoint(320, 144), wxSize(92, 21), wxTE_READONLY|wxTE_RIGHT);

    //Sector Tracking
    sectorBox                = new wxStaticBox(panel,  wxID_ANY, "Sector Tracking", wxPoint(428, 27), wxSize(203, 148), 0);
    sectorSectorTrackingText = new wxStaticText(panel, wxID_ANY, "Sector",          wxPoint(436, 45));
    sectorSectorText         = new wxStaticText(panel, wxID_ANY, "Floor",           wxPoint(436, 71));
    sectorCeilingText        = new wxStaticText(panel, wxID_ANY, "Ceiling",         wxPoint(436, 97));
    sectorEffectText         = new wxStaticText(panel, wxID_ANY, "Effect",          wxPoint(436, 123));
    sectorInputField         = new wxComboBox(panel,   wxID_ANY, wxEmptyString, wxPoint(498, 45), wxSize(100, 21), 0, 0, 0, wxIntegerValidator<unsigned int>());
    sectorInputField->SetMaxLength(10);
    tok.SetString(config.Read("SectorNums", "0"), ",");
    while (tok.HasMoreTokens())
        sectorInputField->Append(tok.GetNextToken());
    sectorInputField->SetSelection(0);
    sectorInputField->Bind(wxEVT_TEXT, wxCommandEventHandler(MainWindow::OnSectorInput), this);
    sectorInputField->Bind(wxEVT_COMBOBOX, wxCommandEventHandler(MainWindow::OnSectorInput), this);
    sectorAddButton             = new wxButton(panel,   wxID_ANY, "Add", wxPoint(602, 45),  wxSize(21, 21), 0);
    sectorAddButton->Bind(wxEVT_BUTTON, wxCommandEventHandler(MainWindow::OnSectorAdd), this);
    sectorFloorHeightTextCtrl   = new wxTextCtrl(panel, wxID_ANY, "0",   wxPoint(498, 71),  wxSize(72, 21), wxTE_READONLY|wxTE_RIGHT);
    sectorCeilingHeightTextCtrl = new wxTextCtrl(panel, wxID_ANY, "0",   wxPoint(498, 96),  wxSize(72, 21), wxTE_READONLY|wxTE_RIGHT);
    sectorSpecialTextCtrl       = new wxTextCtrl(panel, wxID_ANY, "0",   wxPoint(498, 123), wxSize(52, 21), wxTE_READONLY|wxTE_RIGHT);

    //Thing Tracking
    thingBox              = new wxStaticBox(panel,  wxID_ANY, "Thing Tracking",   wxPoint(216, 179), wxSize(417, 152), 0);
    thingThingText        = new wxStaticText(panel, wxID_ANY, "Thing",            wxPoint(224, 197));
    thingTicsText         = new wxStaticText(panel, wxID_ANY, "Tics",             wxPoint(398, 199));
    thingXText            = new wxStaticText(panel, wxID_ANY, "X",                wxPoint(224, 225));
    thingHealthText       = new wxStaticText(panel, wxID_ANY, "Health",           wxPoint(398, 225));
    thingYText            = new wxStaticText(panel, wxID_ANY, "Y",                wxPoint(224, 251));
    thingReactionTimeText = new wxStaticText(panel, wxID_ANY, "Reaction time",    wxPoint(398, 251));
    thingZText            = new wxStaticText(panel, wxID_ANY, "Z",                wxPoint(224, 277));
    thingThresholdText    = new wxStaticText(panel, wxID_ANY, "Threshold",        wxPoint(398, 277));
    thingRadiusText       = new wxStaticText(panel, wxID_ANY, "Radius",           wxPoint(224, 303));
    thingInputField       = new wxComboBox(panel, wxID_ANY, wxEmptyString, wxPoint(282, 197), wxSize(79, 21), 0, 0, 0, wxIntegerValidator<unsigned int>());
    thingInputField->SetMaxLength(10);
    tok.SetString(config.Read("ThingNums", "0"), ",");
    while (tok.HasMoreTokens())
	    thingInputField->Append(tok.GetNextToken());
    thingInputField->SetSelection(0);
    thingInputField->Bind(wxEVT_TEXT, wxCommandEventHandler(MainWindow::OnThingInput), this);
    thingInputField->Bind(wxEVT_COMBOBOX, wxCommandEventHandler(MainWindow::OnThingInput), this);
    thingAddButton            = new wxButton(panel,   wxID_ANY, "Add", wxPoint(369, 197), wxSize(21,  21), 0);
    thingAddButton->Bind(wxEVT_BUTTON, wxCommandEventHandler(MainWindow::OnThingAdd), this);
    thingXTextCtrl            = new wxTextCtrl(panel, wxID_ANY, "0",   wxPoint(282, 223), wxSize(108, 21), wxTE_READONLY|wxTE_RIGHT);
    thingYTextCtrl            = new wxTextCtrl(panel, wxID_ANY, "0",   wxPoint(282, 251), wxSize(108, 21), wxTE_READONLY|wxTE_RIGHT);
    thingZTextCtrl            = new wxTextCtrl(panel, wxID_ANY, "0",   wxPoint(282, 277), wxSize(60,  21),  wxTE_READONLY|wxTE_RIGHT);
    thingRadiusTextCtrl       = new wxTextCtrl(panel, wxID_ANY, "0",   wxPoint(282, 301), wxSize(60,  21),  wxTE_READONLY|wxTE_RIGHT);
    thingTicsTextCtrl         = new wxTextCtrl(panel, wxID_ANY, "0",   wxPoint(498, 197), wxSize(46,  21),  wxTE_READONLY|wxTE_RIGHT);
    thingHealthTextCtrl       = new wxTextCtrl(panel, wxID_ANY, "0",   wxPoint(498, 223), wxSize(46,  21),  wxTE_READONLY|wxTE_RIGHT);
    thingReactionTimeTextCtrl = new wxTextCtrl(panel, wxID_ANY, "0",   wxPoint(498, 249), wxSize(46,  21),  wxTE_READONLY|wxTE_RIGHT);
    thingThresholdTextCtrl    = new wxTextCtrl(panel, wxID_ANY, "0",   wxPoint(498, 275), wxSize(46,  21),  wxTE_READONLY|wxTE_RIGHT);

    //Player Movement
    playerBox               = new wxStaticBox(panel, wxID_ANY, "Player Movement",  wxPoint(216, 335), wxSize(460, 126), 0);
    playerXText             = new wxStaticText(panel, wxID_ANY, "X",               wxPoint(224, 355));
    playerYText             = new wxStaticText(panel, wxID_ANY, "Y",               wxPoint(398, 355));
    playerZText             = new wxStaticText(panel, wxID_ANY, "Z",               wxPoint(566, 355));
    playerXDistText         = new wxStaticText(panel, wxID_ANY, "X dist.",         wxPoint(224, 381));
    playerYDistText         = new wxStaticText(panel, wxID_ANY, "Y dist.",         wxPoint(398, 381));
    playerZDistText         = new wxStaticText(panel, wxID_ANY, "Z dist.",         wxPoint(566, 381));
    playerXMomText          = new wxStaticText(panel, wxID_ANY, "X mom.",          wxPoint(224, 407));
    playerYMomText          = new wxStaticText(panel, wxID_ANY, "Y mom.",          wxPoint(398, 407));
    playerDistMovedText     = new wxStaticText(panel, wxID_ANY, "Distance moved",  wxPoint(224, 433));
    playerDirMovedText      = new wxStaticText(panel, wxID_ANY, "Direction moved", wxPoint(422, 433));
    playerXPosTextCtrl      = new wxTextCtrl(panel, wxID_ANY, "0", wxPoint(282, 353), wxSize(108, 21), wxTE_READONLY|wxTE_RIGHT);
    playerYPosTextCtrl      = new wxTextCtrl(panel, wxID_ANY, "0", wxPoint(450, 353), wxSize(108, 21), wxTE_READONLY|wxTE_RIGHT);
    playerZPosTextCtrl      = new wxTextCtrl(panel, wxID_ANY, "0", wxPoint(614, 353), wxSize(52,  21), wxTE_READONLY|wxTE_RIGHT);
    playerXMomTextCtrl      = new wxTextCtrl(panel, wxID_ANY, "0", wxPoint(282, 405), wxSize(108, 21), wxTE_READONLY|wxTE_RIGHT);
    playerYMomTextCtrl      = new wxTextCtrl(panel, wxID_ANY, "0", wxPoint(450, 405), wxSize(108, 21), wxTE_READONLY|wxTE_RIGHT);
    playerXDistTextCtrl     = new wxTextCtrl(panel, wxID_ANY, "0", wxPoint(282, 379), wxSize(108, 21), wxTE_READONLY|wxTE_RIGHT);
    playerYDistTextCtrl     = new wxTextCtrl(panel, wxID_ANY, "0", wxPoint(450, 379), wxSize(108, 21), wxTE_READONLY|wxTE_RIGHT);
    playerZDistTextCtrl     = new wxTextCtrl(panel, wxID_ANY, "0", wxPoint(614, 379), wxSize(52,  21), wxTE_READONLY|wxTE_RIGHT);
    playerDistMovedTextCtrl = new wxTextCtrl(panel, wxID_ANY, "0", wxPoint(334, 431), wxSize(84,  21), wxTE_READONLY|wxTE_RIGHT);
    playerDirMovedTextCtrl  = new wxTextCtrl(panel, wxID_ANY, "0", wxPoint(544, 431), wxSize(84,  21), wxTE_READONLY|wxTE_RIGHT);

    //General
    generalBox        = new wxStaticBox(panel, wxID_ANY, "General", wxPoint(216, 465), wxSize(368, 100), 0);
    generalInputText  = new wxStaticText(panel, wxID_ANY, "Input",  wxPoint(224, 485));
    generalRNGText    = new wxStaticText(panel, wxID_ANY, "RNG",    wxPoint(350, 485));
    generalAngleText  = new wxStaticText(panel, wxID_ANY, "Angle",  wxPoint(470, 485));
    generalTicText    = new wxStaticText(panel, wxID_ANY, "Tic",    wxPoint(224, 511));
    generalDamageText = new wxStaticText(panel, wxID_ANY, "Damage", wxPoint(350, 511));
    generalSaveText   = new wxStaticText(panel, wxID_ANY, "Save",   wxPoint(470, 511));
    generalTimeText   = new wxStaticText(panel, wxID_ANY, "Time",   wxPoint(224, 539));
    generalUsedText   = new wxStaticText(panel, wxID_ANY, "Used",   wxPoint(350, 539));
    generalInputField = new wxTextCtrl(panel, wxID_ANY, wxEmptyString, wxPoint(282, 483), wxSize(60, 21), wxTE_RIGHT);
    generalInputField->SetMaxLength(10);
    generalInputField->Bind(wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler(MainWindow::OnInputFieldText), this);
    generalRNGIndexTextCtrl     = new wxTextCtrl(panel, wxID_ANY, "0",     wxPoint(404, 483), wxSize(60, 21), wxTE_READONLY|wxTE_RIGHT);
    generalPlayerAngleTextCtrl  = new wxTextCtrl(panel, wxID_ANY, "0",     wxPoint(516, 483), wxSize(60, 21), wxTE_READONLY|wxTE_RIGHT);
    generalCurrentTicTextCtrl   = new wxTextCtrl(panel, wxID_ANY, "0",     wxPoint(282, 509), wxSize(60, 21), wxTE_READONLY|wxTE_RIGHT);
    generalDamageDoneTextCtrl   = new wxTextCtrl(panel, wxID_ANY, "0",     wxPoint(404, 509), wxSize(60, 21), wxTE_READONLY|wxTE_RIGHT);
    generalSavepointTicTextCtrl = new wxTextCtrl(panel, wxID_ANY, "0",     wxPoint(516, 509), wxSize(60, 21), wxTE_READONLY|wxTE_RIGHT);
    generalTimeTextCtrl         = new wxTextCtrl(panel, wxID_ANY, "0",     wxPoint(282, 537), wxSize(60, 21), wxTE_READONLY|wxTE_RIGHT);
    generalUseSuccessTextCtrl   = new wxTextCtrl(panel, wxID_ANY, "false", wxPoint(404, 537), wxSize(60, 21), wxTE_READONLY|wxTE_RIGHT);

    generalAngleTypeChoice = new wxChoice(panel, wxID_ANY, wxPoint(592, 473), wxSize(108, 28), 0, 0, 0, wxDefaultValidator);
    generalAngleTypeChoice->Append("longtics");
    generalAngleTypeChoice->Append("fine angles"); //0-8192. 1 shorttic turn = 256 longtic turns = 32 fine angles,
    generalAngleTypeChoice->Append("degrees");
    generalAngleTypeChoice->Append("byte angles"); //shorttics, 0-255
    generalAngleTypeChoice->SetSelection(config.ReadLong("AngleType", 3));
    generalAngleTypeChoice->Bind(wxEVT_CHOICE, wxCommandEventHandler(MainWindow::OnAngleType), this);

    generalSavepointStyleChoice = new wxChoice(panel, wxID_ANY, wxPoint(592, 509), wxSize(70, 28), 0, 0, 0);
    generalSavepointStyleChoice->Append("User");
    generalSavepointStyleChoice->Append("Start");
    generalSavepointStyleChoice->Append("Auto");
    generalSavepointStyleChoice->SetSelection(x);
    generalSavepointStyleChoice->Bind(wxEVT_CHOICE, wxCommandEventHandler(MainWindow::OnStyleChoiceSelect), this);

    menuBar = new wxMenuBar();

    menuFile = new wxMenu();
    menuBar->Append(menuFile, "File");
    menuFileLoad = new wxMenuItem(menuFile, IDM_LOAD, "Load", wxEmptyString, wxITEM_NORMAL);
    menuFile->Append(menuFileLoad);
    menuBar->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MainWindow::OnMenuLoad), this, IDM_LOAD);
    menuFileSave = new wxMenuItem(menuFile, IDM_SAVE, "Save", wxEmptyString, wxITEM_NORMAL);
    menuFile->Append(menuFileSave);
    menuBar->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MainWindow::OnMenuSave), this, IDM_SAVE);
    menuFileExit = new wxMenuItem(menuFile, IDM_EXIT, "Exit", wxEmptyString, wxITEM_NORMAL);
    menuFile->Append(menuFileExit);
    menuBar->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MainWindow::OnMenuExit), this, IDM_EXIT);

    menuTools = new wxMenu();
    menuBar->Append(menuTools, "Tools");
    menuToolsBrute = new wxMenuItem(menuTools, IDM_BRUTE, "Brute force - Standard", wxEmptyString, wxITEM_NORMAL);
    menuTools->Append(menuToolsBrute);
    menuBar->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MainWindow::OnMenuBrute), this, IDM_BRUTE);
    menuToolsHeader = new wxMenuItem(menuTools, IDM_HEADER, "Edit header", wxEmptyString, wxITEM_NORMAL);
    menuTools->Append(menuToolsHeader);
    menuBar->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MainWindow::OnMenuHeader), this, IDM_HEADER);

    menuOptions = new wxMenu();
    menuBar->Append(menuOptions, "Options");
    menuOptionsKeys = new wxMenuItem(menuOptions, IDM_SETKEYS, "Set keys", wxEmptyString, wxITEM_NORMAL);
    menuOptions->Append(menuOptionsKeys);
    menuBar->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MainWindow::OnMenuSetKeys), this, IDM_SETKEYS);
    menuOptionsClearTraces = new wxMenuItem(menuOptions, IDM_CLEARTRACES, "Clear all traces", wxEmptyString, wxITEM_NORMAL);
    menuOptions->Append(menuOptionsClearTraces);
    menuBar->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MainWindow::OnMenuClearTraces), this, IDM_CLEARTRACES);
    menuOptionsPalette = new wxMenuItem(menuOptions, IDM_PALSTUFF, "Palette stuff", wxEmptyString, wxITEM_CHECK);
    menuOptions->Append(menuOptionsPalette);
    menuBar->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MainWindow::OnMenuPalStuff), this, IDM_PALSTUFF);

    SetMenuBar(menuBar);

    Connect(wxID_ANY, wxEVT_CLOSE_WINDOW, wxCloseEventHandler(MainWindow::OnClose));
    Connect(wxID_ANY, wxEVT_IDLE, wxCommandEventHandler(MainWindow::OnIdle));

    xdre::setAngleType(generalAngleTypeChoice->GetSelection());
    linedefXV1TextCtrl->ChangeValue(wxString::FromCDouble(xdre::getLinedefVertex(0, 0), 0));
    linedefYV1TextCtrl->ChangeValue(wxString::FromCDouble(xdre::getLinedefVertex(0, 1), 0));
    linedefXV2TextCtrl->ChangeValue(wxString::FromCDouble(xdre::getLinedefVertex(1, 0), 0));
    linedefYV2TextCtrl->ChangeValue(wxString::FromCDouble(xdre::getLinedefVertex(1, 1), 0));

    menuOptionsPalette->Check(false);
    generalInputField->SetFocus();
    RS();
}

MainWindow::~MainWindow() {} // Destructor

void MainWindow::DoInput(XCmd cmd, unsigned int val) {
    switch (cmd) {
        case XCmd::Fire:          xdre::toggleFire();     RS(); break;
        case XCmd::Use:           xdre::toggleUse();      RS(); break;
        case XCmd::TurnStepLeft:  xdre::turnStepLeft();   RS(); break;
        case XCmd::TurnStepRight: xdre::turnStepRight();  RS(); break;
        case XCmd::TurnStep:      xdre::setTurnStep(val);       break;
        case XCmd::PrevTic:       xdre::seekDemo(-1);     RS(); break;
        case XCmd::NextTic:       xdre::seekDemo(1);      RS(); break;
        case XCmd::CopyTic:       xdre::copyTic();        RS(); break;
        case XCmd::CopyTicX:      xdre::copyTicX(val);    RS(); break;
        case XCmd::DeleteTic:     xdre::deleteTic();      RS(); break;
        case XCmd::DeleteTicX:    xdre::deleteTicX(val);  RS(); break;
        case XCmd::NewTic:        xdre::newTic();         RS(); break;
        case XCmd::ToggleBlood:   xdre::toggleBlood(); break;
        case XCmd::SetSavepoint:  xdre::setSavepoint();   RS(); break;
        case XCmd::UnsetSavepoint:xdre::unsetSavepoint(); RS(); break;
        case XCmd::Load:          Load();                       break;
        case XCmd::Save:          Save();                       break;
        case XCmd::Exit:          Quit();                       break;
        case XCmd::RunBwd:        xdre::run(-val);        RS(); break;
        case XCmd::RunFwd:        xdre::run(val);         RS(); break;
        case XCmd::StrafeLeft:    xdre::strafe(-val);     RS(); break;
        case XCmd::StrafeRight:   xdre::strafe(val);      RS(); break;
        case XCmd::TurnLeft:      xdre::turn(val);        RS(); break;
        case XCmd::TurnRight:     xdre::turn(-val);       RS(); break;
        case XCmd::Weapon:        xdre::changeWeapon(val);RS(); break;
        case XCmd::TransposeLeft: xdre::transpose(0, val);RS(); break;
        case XCmd::TransposeRight:xdre::transpose(1, val);RS(); break;
        case XCmd::BackwardDemo:  xdre::seekDemo(-val);   RS(); break;
        case XCmd::ForwardDemo:   xdre::seekDemo(val);    RS(); break;
        case XCmd::ChangePlayer:  xdre::changePlayerView(val);RS();break;
        default: break;
    }
}

void MainWindow::SetKeys() {
    configToStringMap.clear();
    configToStringMap["Fire"] = config.Read("Fire", "f");
    configToStringMap["Use"] = config.Read("Use", "u");
    configToStringMap["TurnStepLeft"] = config.Read("TurnStepLeft", "-");
    configToStringMap["TurnStepRight"] = config.Read("TurnStepRight", "+");
    configToStringMap["TurnStep"] = config.Read("TurnStep", "ts");
    configToStringMap["PrevTic"] = config.Read("PrevTic", "j");
    configToStringMap["NextTic"] = config.Read("NextTic", "i");
    configToStringMap["CopyTic"] = config.Read("CopyTic", "c");
    configToStringMap["DeleteTic"] = config.Read("DeleteTic", "x");;
    configToStringMap["NewTic"] = config.Read("NewTic", "n");
    configToStringMap["ToggleBlood"] = config.Read("ToggleBlood", "z");
    configToStringMap["SetSavepoint"] = config.Read("SetSavepoint", "ssp");
    configToStringMap["UnsetSavepoint"] = config.Read("UnsetSavepoint", "pss");
    configToStringMap["Load"] = config.Read("Load", "load");
    configToStringMap["Save"] = config.Read("Save", "save");
    configToStringMap["Exit"] = config.Read("Exit", "exit");

    configToStringMap["RunBwd"] = config.Read("RunBwd", "wq");
    configToStringMap["RunFwd"] = config.Read("RunFwd", "we");
    configToStringMap["StrafeLeft"] = config.Read("StrafeLeft", "sa");
    configToStringMap["StrafeRight"] = config.Read("StrafeRight", "sd");
    configToStringMap["TurnLeft"] = config.Read("TurnLeft", "re");
    configToStringMap["TurnRight"] = config.Read("TurnRight", "rt");
    configToStringMap["Weapon"] = config.Read("Weapon", "g");

    configToStringMap["TransposeLeft"] = config.Read("TransposeLeft", "tr");
    configToStringMap["TransposeRight"] = config.Read("TransposeRight", "ty");
    configToStringMap["CopyTicX"] = config.Read("CopyTicX", "tc");
    configToStringMap["DeleteTicX"] = config.Read("DeleteTicX", "tx");
    configToStringMap["BackwardDemo"] = config.Read("BackwardDemo", ",");
    configToStringMap["ForwardDemo"] = config.Read("ForwardDemo", ".");
    configToStringMap["ChangePlayer"] = config.Read("ChangePlayer", "v");

    stringToCommandMap.clear();
    stringToCommandMap[configToStringMap["Fire"]] = XCmd::Fire;
    stringToCommandMap[configToStringMap["Use"]] = XCmd::Use;
    stringToCommandMap[configToStringMap["TurnStepLeft"]] = XCmd::TurnStepLeft;
    stringToCommandMap[configToStringMap["TurnStepRight"]] = XCmd::TurnStepRight;
    stringToCommandMap[configToStringMap["TurnStep"]] = XCmd::TurnStep;
    stringToCommandMap[configToStringMap["PrevTic"]] = XCmd::PrevTic;
    stringToCommandMap[configToStringMap["NextTic"]] = XCmd::NextTic;
    stringToCommandMap[configToStringMap["CopyTic"]] = XCmd::CopyTic;
    stringToCommandMap[configToStringMap["DeleteTic"]] = XCmd::DeleteTic;
    stringToCommandMap[configToStringMap["NewTic"]] = XCmd::NewTic;
    stringToCommandMap[configToStringMap["ToggleBlood"]] = XCmd::ToggleBlood;
    stringToCommandMap[configToStringMap["SetSavepoint"]] = XCmd::SetSavepoint;
    stringToCommandMap[configToStringMap["UnsetSavepoint"]] = XCmd::UnsetSavepoint;
    stringToCommandMap[configToStringMap["Load"]] = XCmd::Load;
    stringToCommandMap[configToStringMap["Save"]] = XCmd::Save;
    stringToCommandMap[configToStringMap["Exit"]] = XCmd::Exit;

    stringToCommandMap[configToStringMap["RunBwd"]] = XCmd::RunBwd;
    stringToCommandMap[configToStringMap["RunFwd"]] = XCmd::RunFwd;
    stringToCommandMap[configToStringMap["StrafeLeft"]] = XCmd::StrafeLeft;
    stringToCommandMap[configToStringMap["StrafeRight"]] = XCmd::StrafeRight;
    stringToCommandMap[configToStringMap["TurnLeft"]] = XCmd::TurnLeft;
    stringToCommandMap[configToStringMap["TurnRight"]] = XCmd::TurnRight;
    stringToCommandMap[configToStringMap["Weapon"]] = XCmd::Weapon;

    stringToCommandMap[configToStringMap["TransposeLeft"]] = XCmd::TransposeLeft;
    stringToCommandMap[configToStringMap["TransposeRight"]] = XCmd::TransposeRight;
    stringToCommandMap[configToStringMap["CopyTicX"]] = XCmd::CopyTicX;
    stringToCommandMap[configToStringMap["DeleteTicX"]] = XCmd::DeleteTicX;
    stringToCommandMap[configToStringMap["BackwardDemo"]] = XCmd::BackwardDemo;
    stringToCommandMap[configToStringMap["ForwardDemo"]] = XCmd::ForwardDemo;
    stringToCommandMap[configToStringMap["ChangePlayer"]] = XCmd::ChangePlayer;
}

void MainWindow::RS() { //RefreshStuff
    linedefCrossedTextCtrl         ->ChangeValue(xdre::getLinedefCrossed() ? "True" : "False");
    linedefDistanceFromLineTextCtrl->ChangeValue(wxString::FromCDouble(xdre::getDistanceFromLine(), 6));

    sectorFloorHeightTextCtrl  ->ChangeValue(wxString::FromCDouble(xdre::getSectorInfo(0), 2));
    sectorCeilingHeightTextCtrl->ChangeValue(wxString::FromCDouble(xdre::getSectorInfo(1), 2));
    sectorSpecialTextCtrl      ->ChangeValue(wxString::FromCDouble(xdre::getSectorInfo(2)));

    thingXTextCtrl           ->ChangeValue(wxString::FromCDouble(xdre::getThingInfo(0), 6));
    thingYTextCtrl           ->ChangeValue(wxString::FromCDouble(xdre::getThingInfo(1), 6));
    thingZTextCtrl           ->ChangeValue(wxString::FromCDouble(xdre::getThingInfo(2), 2));
    thingRadiusTextCtrl      ->ChangeValue(wxString::FromCDouble(xdre::getThingInfo(3), 3));
    thingTicsTextCtrl        ->ChangeValue(wxString::FromCDouble(xdre::getThingInfo(4), 0));
    thingHealthTextCtrl      ->ChangeValue(wxString::FromCDouble(xdre::getThingInfo(5), 0));
    thingReactionTimeTextCtrl->ChangeValue(wxString::FromCDouble(xdre::getThingInfo(6), 0));
    thingThresholdTextCtrl   ->ChangeValue(wxString::FromCDouble(xdre::getThingInfo(7), 0));

    playerXPosTextCtrl     ->ChangeValue(wxString::FromCDouble(xdre::getXPos(), 6));
    playerYPosTextCtrl     ->ChangeValue(std::to_string(xdre::getYPos()));
    playerZPosTextCtrl     ->ChangeValue(wxString::FromCDouble(xdre::getZPos(), 2)); //2 for slow stairs
    playerXMomTextCtrl     ->ChangeValue(wxString::FromCDouble(xdre::getXMom(), 6));
    playerYMomTextCtrl     ->ChangeValue(wxString::FromCDouble(xdre::getYMom(), 6));
    playerXDistTextCtrl    ->ChangeValue(wxString::FromCDouble(xdre::getXDist(), 6));
    playerYDistTextCtrl    ->ChangeValue(wxString::FromCDouble(xdre::getYDist(), 6));
    playerZDistTextCtrl    ->ChangeValue(wxString::FromCDouble(xdre::getZDist(), 2));
    playerDistMovedTextCtrl->ChangeValue(wxString::FromCDouble(xdre::getDistanceMoved(), 6));
    playerDirMovedTextCtrl ->ChangeValue(wxString::FromCDouble(xdre::getDirectionMoved(), 6));

    generalRNGIndexTextCtrl         ->ChangeValue(wxString::FromCDouble(xdre::getRngIndex()));
    generalPlayerAngleTextCtrl      ->ChangeValue(wxString::FromCDouble(xdre::getAngle(0)));
    generalTimeTextCtrl             ->ChangeValue(wxString::FromCDouble(xdre::getTime(), 2));
    generalCurrentTicTextCtrl       ->ChangeValue(wxString::FromCDouble(xdre::getCurrentTic()));
    generalDamageDoneTextCtrl       ->ChangeValue(wxString::FromCDouble(xdre::getDoneDamage()));
    generalUseSuccessTextCtrl       ->ChangeValue(xdre::getUseSuccess() ? "True" : "False");

    generalSavepointTicTextCtrl     ->ChangeValue(wxString::FromCDouble(xdre::getSavepointTic()));
    generalSavepointStyleChoice      ->SetSelection(xdre::getSavepointStyle());
    ticlist          ->SetItemCount(xdre::getTiclistSize());
    ticlist          ->SetItemState(xdre::getCurrentTic(), wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    ticlist          ->EnsureVisible(xdre::getCurrentTic());

    ticlist->Refresh();
	
	xdre::doSdlEvents();
}

void MainWindow::Save() {
    wxString filename = wxFileSelector(wxFileSelectorPromptStr,
                                       wxEmptyString,
                                       wxEmptyString,
                                       ".lmp",
                                       "LMP files (*.lmp)|*.lmp",
                                       wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if(!filename.empty()) {
        wxFFile file;
        if(file.Open(filename, "wb")) {
            if(!xdre::save(file.fp())) {
                wxMessageBox("Couldn't write demo. That's really weird and shouldn't happen.",
                             "Couldn't write demo",
                             wxOK | wxCENTRE | wxICON_ERROR,
                             this);
            }
        } else {
            wxMessageBox("Couldn't open file for write: " + filename,
                         "Couldn't open file",
                         wxOK | wxCENTRE | wxICON_ERROR,
                         this);
        }
    }
}

void MainWindow::Load() {
    wxString filename = wxFileSelector(wxFileSelectorPromptStr,
                                       wxEmptyString,
                                       wxEmptyString,
                                       ".lmp",
                                       "LMP files (*.lmp)|*.lmp",
                                       wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if(!filename.empty()) {
        wxFFile file;
        if(file.Open(filename, "rb")) {
            if(!xdre::load(file.fp())) {
                wxMessageBox("Couldn't load demo. Debug it yourself to see what went wrong.",
                             "Couldn't load demo",
                             wxOK | wxCENTRE | wxICON_ERROR,
                             this);
            } else RS();
        } else {
            wxMessageBox("Couldn't open file for read: " + filename,
                         "Couldn't open file",
                         wxOK | wxCENTRE | wxICON_ERROR,
                         this);
        }
    }
}

void MainWindow::Quit(bool canVeto) {
    if(canVeto) {
        int ync = wxMessageBox("Save demo before quitting?", "Save before quit", wxYES_NO | wxCANCEL, this);
        if(ync == wxCANCEL) return;
        else if(ync == wxYES) Save();
    }
    WriteConfig();
    Destroy();
}

void MainWindow::WriteConfig() {
    for(auto& key : configToStringMap) config.Write(key.first, key.second);

    unsigned int i, j;
    wxString s(wxEmptyString);
    int x, y;
    MainWindow::GetScreenPosition(&x, &y);
    s << x; s << ","; s << y;
    config.Write("WindowPos", s);
    s.Clear();
    SDL_GetWindowPosition(sdl_window, &x, &y);
    s << x; s << ","; s << y;
    config.Write("DoomWindowPos", s);
    s.Clear();
    config.Write("AngleType", generalAngleTypeChoice->GetSelection());
    config.Write("SavepointMode", generalSavepointStyleChoice->GetSelection());

    j = linedefInputField->GetCount();
    for(i = 0; i != j; i++) {
        if(linedefInputField->GetString(i) == wxEmptyString) continue;
        s << linedefInputField->GetString(i);
        if(j-1 > i) s << ",";
    }
    if(s.IsEmpty()) s = "0";
    config.Write("LinedefNums", s);
    s.Clear();

    j = sectorInputField->GetCount();
    for(i = 0; i != j; i++) {
        if(sectorInputField->GetString(i) == wxEmptyString) continue;
        s << sectorInputField->GetString(i);
        if(j-1 > i) s << ",";
    }
    if(s.IsEmpty()) s = "0";
    config.Write("SectorNums", s);
    s.Clear();

    j = thingInputField->GetCount();
    for(i = 0; i != j; i++) {
        if(thingInputField->GetString(i) == wxEmptyString) continue;
        s << thingInputField->GetString(i);
        if(j-1 > i) s << ",";
    }
    if(s.IsEmpty()) s = "0";
    config.Write("ThingNums", s);
    s.Clear();
}

void MainWindow::OnIdle(wxCommandEvent& event) {
    xdre::doSdlEvents();
}

void MainWindow::OnClose(wxCloseEvent& event) {
    Quit(event.CanVeto());
}

void MainWindow::OnInputFieldText(wxCommandEvent& event) {
    wxString input = generalInputField->GetValue();

    if(input.length() < 1) return;

    wxString strCmd = input;
    unsigned long val = 0;
    int numDigits = 0;
    bool space = false;
    //plain cmd or with value?
    for(unsigned int i = 1; i < input.length(); ++i) {
        if(wxIsdigit(input[i])) {
            numDigits = input.Mid(i).First(" ");
            if(numDigits == wxNOT_FOUND) numDigits = 1;
            else space = true;

            //there should be digits only, is there?
            if(input.Mid(i, numDigits).ToULong(&val)) {
                strCmd = input.Mid(0, i);
                break; //yes
            } else return; //no
        }
    }

    auto keyvalue = stringToCommandMap.find(strCmd);
    if(keyvalue == stringToCommandMap.end()) return;

    // final checks D:
    if(keyvalue->second > XCmd::Exit && numDigits < 1) return;
    else if(keyvalue->second > XCmd::Weapon && !space) return;

    generalInputField->ChangeValue("");
    DoInput(keyvalue->second, val);
}

void MainWindow::OnAngleType(wxCommandEvent& event) {
    xdre::setAngleType(generalAngleTypeChoice->GetSelection());
    generalPlayerAngleTextCtrl->ChangeValue(wxString::FromCDouble(xdre::getAngle(0)));
}

void MainWindow::OnLinedefInput(wxCommandEvent& event) {
    unsigned long val;
    if(linedefInputField->GetValue().ToULong(&val)) {
        xdre::setLinedefCheck(val);
        linedefXV1TextCtrl             ->ChangeValue(wxString::FromCDouble(xdre::getLinedefVertex(0, 0), 0));
        linedefYV1TextCtrl             ->ChangeValue(wxString::FromCDouble(xdre::getLinedefVertex(0, 1), 0));
        linedefXV2TextCtrl             ->ChangeValue(wxString::FromCDouble(xdre::getLinedefVertex(1, 0), 0));
        linedefYV2TextCtrl             ->ChangeValue(wxString::FromCDouble(xdre::getLinedefVertex(1, 1), 0));
        linedefCrossedTextCtrl         ->ChangeValue(xdre::getLinedefCrossed() ? "true" : "false");
        linedefDistanceFromLineTextCtrl->ChangeValue(wxString::FromCDouble(xdre::getDistanceFromLine(), 6));
    }
}

void MainWindow::OnLineAdd(wxCommandEvent& event) {
    if(linedefInputField->GetValue() != wxEmptyString)
        linedefInputField->Append(linedefInputField->GetValue());
}

void MainWindow::OnSectorInput(wxCommandEvent& event) {
    unsigned long val;
    if(sectorInputField->GetValue().ToULong(&val)) {
        xdre::setSectorCheck(val);
        sectorFloorHeightTextCtrl  ->ChangeValue(wxString::FromCDouble(xdre::getSectorInfo(0), 2));
        sectorCeilingHeightTextCtrl->ChangeValue(wxString::FromCDouble(xdre::getSectorInfo(1), 2));
        sectorSpecialTextCtrl      ->ChangeValue(wxString::FromCDouble(xdre::getSectorInfo(2)));
    }
}

void MainWindow::OnSectorAdd(wxCommandEvent& event) {
    if(sectorInputField->GetValue() != wxEmptyString) {
        sectorInputField->Append(sectorInputField->GetValue());
    }
}

void MainWindow::OnThingInput(wxCommandEvent& event) {
    unsigned long val;
    if(thingInputField->GetValue().ToULong(&val)) {
        xdre::setThingCheck(val);
        thingXTextCtrl           ->ChangeValue(wxString::FromCDouble(xdre::getThingInfo(0), 6));
        thingYTextCtrl           ->ChangeValue(wxString::FromCDouble(xdre::getThingInfo(1), 6));
        thingZTextCtrl           ->ChangeValue(wxString::FromCDouble(xdre::getThingInfo(2), 2));
        thingRadiusTextCtrl      ->ChangeValue(wxString::FromCDouble(xdre::getThingInfo(3), 3));
        thingTicsTextCtrl        ->ChangeValue(wxString::FromCDouble(xdre::getThingInfo(4), 0));
        thingHealthTextCtrl      ->ChangeValue(wxString::FromCDouble(xdre::getThingInfo(5), 0));
        thingReactionTimeTextCtrl->ChangeValue(wxString::FromCDouble(xdre::getThingInfo(6), 0));
        thingThresholdTextCtrl   ->ChangeValue(wxString::FromCDouble(xdre::getThingInfo(7), 0));
    }
}

void MainWindow::OnThingAdd(wxCommandEvent& event) {
    if(thingInputField->GetValue() != wxEmptyString)
        thingInputField->Append(thingInputField->GetValue());
}

void MainWindow::OnMenuPalStuff(wxCommandEvent& event) {
    xdre::toggleBlood();
    menuOptionsPalette->Check(xdre::getBlood());
}

void MainWindow::OnMenuExit(wxCommandEvent& event) {
    Quit();
}

void MainWindow::OnMenuLoad(wxCommandEvent& event) {
    Load();
}

void MainWindow::OnMenuSave(wxCommandEvent& event) {
    Save();
}

void MainWindow::OnMenuBrute(wxCommandEvent& event) {
    BruteDialog bruteDialog(this);
    bruteDialog.ShowModal();
    RS();
}

void MainWindow::OnMenuSetKeys(wxCommandEvent& event) {
    KeysDialog *keysDialog = new KeysDialog(this);
    keysDialog->Show(true);
}

void MainWindow::OnMenuHeader(wxCommandEvent& event) {
    HeaderDialog *headerDialog = new HeaderDialog(this);
    headerDialog->Show(true);
}

void MainWindow::OnMenuClearTraces(wxCommandEvent& event) {
    linedefInputField->Clear();
    sectorInputField->Clear();
    thingInputField->Clear();
}

void MainWindow::OnStyleChoiceSelect(wxCommandEvent& event) {
    xdre::setSavepointStyle(generalSavepointStyleChoice->GetSelection());
    RS();
}
