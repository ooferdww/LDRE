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
 * Description: change main window keys w/o editing xdre/xdre.ini. Might be
 *              useless for people who use a text editor, though :P
 *              Too bad this is half-arsed, I just use a pointer to the parent
 *              window.
*/

#include "KeysDialog.hpp"

#include <wx/intl.h>
#include <wx/string.h>
#include <wx/valnum.h>
#include <wx/valtext.h>

#include "MainWindow.hpp"

const long KeysDialog::ID_SET = wxNewId();
const long KeysDialog::ID_CLOSE = wxNewId();

BEGIN_EVENT_TABLE(KeysDialog,wxDialog)
END_EVENT_TABLE()

MainWindow *window;

KeysDialog::KeysDialog(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size) {
    wxDialog::Create(parent, wxID_ANY, "Keys");
    SetClientSize(wxSize(472, 320));
    SetMinSize(wxSize(472, 320));
    SetMaxSize(wxSize(472, -1));
    window = (MainWindow*)parent;

    panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(400, 440), wxTAB_TRAVERSAL);
    setButton = new wxButton(panel, ID_SET, "Set", wxPoint(32, 288), wxDefaultSize, 0, wxDefaultValidator);
    closeButton = new wxButton(panel, ID_CLOSE, "Close", wxPoint(380, 288), wxDefaultSize, 0, wxDefaultValidator);

    fireText           = new wxStaticText(panel, wxID_ANY, "Fire",           wxPoint(8,   4));
    useText            = new wxStaticText(panel, wxID_ANY, "Use",            wxPoint(8,   32));
    turnStepLeftText   = new wxStaticText(panel, wxID_ANY, "TurnStepLeft",   wxPoint(8,   60));
    turnStepRightText  = new wxStaticText(panel, wxID_ANY, "TurnStepRight",  wxPoint(8,   88));
    turnStepText       = new wxStaticText(panel, wxID_ANY, "TurnStep",       wxPoint(8,   116));
    prevTicText        = new wxStaticText(panel, wxID_ANY, "PrevTic",        wxPoint(8,   144));
    nextTicText        = new wxStaticText(panel, wxID_ANY, "NextTic",        wxPoint(8,   172));
    copyTicText        = new wxStaticText(panel, wxID_ANY, "CopyTic",        wxPoint(8,   200));
    deleteTicText      = new wxStaticText(panel, wxID_ANY, "DeleteTic",      wxPoint(8,   228));
    newTicText         = new wxStaticText(panel, wxID_ANY, "NewTic",         wxPoint(8,   256));
    toggleBloodText    = new wxStaticText(panel, wxID_ANY, "ToggleBlood",    wxPoint(152, 4));
    setSavepointText   = new wxStaticText(panel, wxID_ANY, "SetSavepoint",   wxPoint(152, 32));
    unsetSavepointText = new wxStaticText(panel, wxID_ANY, "UnsetSavepoint", wxPoint(152, 60));
    loadText           = new wxStaticText(panel, wxID_ANY, "Load",           wxPoint(152, 88));
    saveText           = new wxStaticText(panel, wxID_ANY, "Save",           wxPoint(152, 116));
    exitText           = new wxStaticText(panel, wxID_ANY, "Exit",           wxPoint(152, 144));
    runBwdText         = new wxStaticText(panel, wxID_ANY, "RunBwd",         wxPoint(152, 172));
    runFwdText         = new wxStaticText(panel, wxID_ANY, "RunFwd",         wxPoint(152, 200));
    strafeLeftText     = new wxStaticText(panel, wxID_ANY, "StrafeLeft",     wxPoint(152, 228));
    strafeRightText    = new wxStaticText(panel, wxID_ANY, "StrafeRight",    wxPoint(152, 256));
    turnLeftText       = new wxStaticText(panel, wxID_ANY, "TurnLeft",       wxPoint(308, 4));
    turnRightText      = new wxStaticText(panel, wxID_ANY, "TurnRight",      wxPoint(308, 32));
    weaponText         = new wxStaticText(panel, wxID_ANY, "Weapon",         wxPoint(308, 60));
    transposeLeftText  = new wxStaticText(panel, wxID_ANY, "TransposeLeft",  wxPoint(308, 88));
    transposeRightText = new wxStaticText(panel, wxID_ANY, "TransposeRight", wxPoint(308, 116));
    copyTicXText       = new wxStaticText(panel, wxID_ANY, "CopyTicX",       wxPoint(308, 144));
    deleteTicXText     = new wxStaticText(panel, wxID_ANY, "DeleteTicX",     wxPoint(308, 172));
    backwardDemoText   = new wxStaticText(panel, wxID_ANY, "BackwardDemo",   wxPoint(308, 200));
    forwardDemoText    = new wxStaticText(panel, wxID_ANY, "ForwardDemo",    wxPoint(308, 228));
    changePlayerText   = new wxStaticText(panel, wxID_ANY, "ChangePlayer",   wxPoint(308, 256));

    Fire           = new wxTextCtrl(panel, wxID_ANY, window->config.Read("Fire", "f"),            wxPoint(100, 0),   wxSize(44, 20), wxTE_RIGHT);
    Use            = new wxTextCtrl(panel, wxID_ANY, window->config.Read("Use", "u"),             wxPoint(100, 28),  wxSize(44, 20), wxTE_RIGHT);
    TurnStepLeft   = new wxTextCtrl(panel, wxID_ANY, window->config.Read("TurnStepLeft", "-"),    wxPoint(100, 56),  wxSize(44, 20), wxTE_RIGHT);
    TurnStepRight  = new wxTextCtrl(panel, wxID_ANY, window->config.Read("TurnStepRight", "+"),   wxPoint(100, 84),  wxSize(44, 20), wxTE_RIGHT);
    TurnStep       = new wxTextCtrl(panel, wxID_ANY, window->config.Read("TurnStep", "ts"),       wxPoint(100, 112), wxSize(44, 20), wxTE_RIGHT);
    PrevTic        = new wxTextCtrl(panel, wxID_ANY, window->config.Read("PrevTic", "j"),         wxPoint(100, 140), wxSize(44, 20), wxTE_RIGHT);
    NextTic        = new wxTextCtrl(panel, wxID_ANY, window->config.Read("NextTic", "i"),         wxPoint(100, 168), wxSize(44, 20), wxTE_RIGHT);
    CopyTic        = new wxTextCtrl(panel, wxID_ANY, window->config.Read("CopyTic", "c"),         wxPoint(100, 196), wxSize(44, 20), wxTE_RIGHT);
    DeleteTic      = new wxTextCtrl(panel, wxID_ANY, window->config.Read("DeleteTic", "x"),       wxPoint(100, 224), wxSize(44, 20), wxTE_RIGHT);
    NewTic         = new wxTextCtrl(panel, wxID_ANY, window->config.Read("NewTic", "n"),          wxPoint(100, 252), wxSize(44, 20), wxTE_RIGHT);
    ToggleBlood    = new wxTextCtrl(panel, wxID_ANY, window->config.Read("ToggleBlood", "z"),     wxPoint(260, 0),   wxSize(44, 20), wxTE_RIGHT);
    SetSavepoint   = new wxTextCtrl(panel, wxID_ANY, window->config.Read("SetSavepoint", "ssp"),  wxPoint(260, 28),  wxSize(44, 20), wxTE_RIGHT);
    UnsetSavepoint = new wxTextCtrl(panel, wxID_ANY, window->config.Read("UnsetSavepoint", "pss"),wxPoint(260, 56),  wxSize(44, 20), wxTE_RIGHT);
    Load           = new wxTextCtrl(panel, wxID_ANY, window->config.Read("Load", "load"),         wxPoint(260, 84),  wxSize(44, 20), wxTE_RIGHT);
    Save           = new wxTextCtrl(panel, wxID_ANY, window->config.Read("Save", "save"),         wxPoint(260, 112), wxSize(44, 20), wxTE_RIGHT);
    Exit           = new wxTextCtrl(panel, wxID_ANY, window->config.Read("Exit", "exit"),         wxPoint(260, 140), wxSize(44, 20), wxTE_RIGHT);
    RunBwd         = new wxTextCtrl(panel, wxID_ANY, window->config.Read("RunBwd", "wq"),         wxPoint(260, 168), wxSize(44, 20), wxTE_RIGHT);
    RunFwd         = new wxTextCtrl(panel, wxID_ANY, window->config.Read("RunFwd", "we"),         wxPoint(260, 196), wxSize(44, 20), wxTE_RIGHT);
    StrafeLeft     = new wxTextCtrl(panel, wxID_ANY, window->config.Read("StrafeLeft", "sa"),     wxPoint(260, 224), wxSize(44, 20), wxTE_RIGHT);
    StrafeRight    = new wxTextCtrl(panel, wxID_ANY, window->config.Read("StrafeRight", "sd"),    wxPoint(260, 252), wxSize(44, 20), wxTE_RIGHT);
    TurnLeft       = new wxTextCtrl(panel, wxID_ANY, window->config.Read("TurnLeft", "re"),       wxPoint(416, 0),   wxSize(44, 20), wxTE_RIGHT);
    TurnRight      = new wxTextCtrl(panel, wxID_ANY, window->config.Read("TurnRight", "rt"),      wxPoint(416, 28),  wxSize(44, 20), wxTE_RIGHT);
    Weapon         = new wxTextCtrl(panel, wxID_ANY, window->config.Read("Weapon", "g"),          wxPoint(416, 56),  wxSize(44, 20), wxTE_RIGHT);
    TransposeLeft  = new wxTextCtrl(panel, wxID_ANY, window->config.Read("TransposeLeft", "tr"),  wxPoint(416, 84),  wxSize(44, 20), wxTE_RIGHT);
    TransposeRight = new wxTextCtrl(panel, wxID_ANY, window->config.Read("TransposeRight", "ty"), wxPoint(416, 112), wxSize(44, 20), wxTE_RIGHT);
    CopyTicX       = new wxTextCtrl(panel, wxID_ANY, window->config.Read("CopyTicX", "tc"),       wxPoint(416, 140), wxSize(44, 20), wxTE_RIGHT);
    DeleteTicX     = new wxTextCtrl(panel, wxID_ANY, window->config.Read("DeleteTicX", "tx"),     wxPoint(416, 168), wxSize(44, 20), wxTE_RIGHT);
    BackwardDemo   = new wxTextCtrl(panel, wxID_ANY, window->config.Read("BackwardDemo", ","),    wxPoint(416, 196), wxSize(44, 20), wxTE_RIGHT);
    ForwardDemo    = new wxTextCtrl(panel, wxID_ANY, window->config.Read("ForwardDemo", "."),     wxPoint(416, 224), wxSize(44, 20), wxTE_RIGHT);
    ChangePlayer   = new wxTextCtrl(panel, wxID_ANY, window->config.Read("ChangePlayer", "v"),    wxPoint(416, 252), wxSize(44, 20), wxTE_RIGHT);

    Connect(wxID_ANY, wxEVT_CLOSE_WINDOW, wxCloseEventHandler(KeysDialog::OnClose));
    Connect(ID_CLOSE, wxEVT_BUTTON, wxCloseEventHandler(KeysDialog::OnClose));
    Connect(ID_SET, wxEVT_BUTTON, wxCommandEventHandler(KeysDialog::OnSet));
}

KeysDialog::~KeysDialog() {} //Destructor

void KeysDialog::OnClose(wxCloseEvent& event) {
    Destroy();
}

void KeysDialog::OnSet(wxCommandEvent& event) {
    window->config.Write("Fire", Fire->GetValue());
    window->config.Write("Use", Use->GetValue());
    window->config.Write("TurnStepLeft", TurnStepLeft->GetValue());
    window->config.Write("TurnStepRight", TurnStepRight->GetValue());
    window->config.Write("TurnStep", TurnStep->GetValue());
    window->config.Write("PrevTic", PrevTic->GetValue());
    window->config.Write("NextTic", NextTic->GetValue());
    window->config.Write("CopyTic", CopyTic->GetValue());
    window->config.Write("DeleteTic", DeleteTic->GetValue());
    window->config.Write("NewTic", NewTic->GetValue());
    window->config.Write("ToggleBlood", ToggleBlood->GetValue());
    window->config.Write("SetSavepoint", SetSavepoint->GetValue());
    window->config.Write("UnsetSavepoint", UnsetSavepoint->GetValue());
    window->config.Write("Load", Load->GetValue());
    window->config.Write("Save", Save->GetValue());
    window->config.Write("Exit", Exit->GetValue());
    window->config.Write("RunBwd", RunBwd->GetValue());
    window->config.Write("RunFwd", RunFwd->GetValue());
    window->config.Write("StrafeLeft", StrafeLeft->GetValue());
    window->config.Write("StrafeRight", StrafeRight->GetValue());
    window->config.Write("TurnLeft", TurnLeft->GetValue());
    window->config.Write("TurnRight", TurnRight->GetValue());
    window->config.Write("Weapon", Weapon->GetValue());
    window->config.Write("TransposeLeft", TransposeLeft->GetValue());
    window->config.Write("TransposeRight", TransposeRight->GetValue());
    window->config.Write("CopyTicX", CopyTicX->GetValue());
    window->config.Write("DeleteTicX", DeleteTicX->GetValue());
    window->config.Write("BackwardDemo", BackwardDemo->GetValue());
    window->config.Write("ForwardDemo", ForwardDemo->GetValue());
    window->config.Write("ChangePlayer", ChangePlayer->GetValue());
    window->SetKeys();
}

