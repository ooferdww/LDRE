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
 * Description: brute force dialog.
*/

#include "BruteDialog.hpp"

#include <wx/intl.h>
#include <wx/string.h>
#include <wx/valnum.h>
#include <wx/valtext.h>
#include <wx/msgdlg.h>
#include <wx/tokenzr.h>

class BruteException : public std::exception {
public:
    BruteException(const char* str) : str{str} {}
    virtual const char* what() const noexcept {return str;}
    const char* str;
};

const long BruteDialog::ID_TURNCMDINPUT = wxNewId();
const long BruteDialog::ID_TURNANGLEBUTTON = wxNewId();
const long BruteDialog::ID_FIRERADIOBOX = wxNewId();
const long BruteDialog::ID_USERADIOBOX = wxNewId();

wxDEFINE_EVENT(EVT_BRUTE_DONE, wxThreadEvent);
BEGIN_EVENT_TABLE(BruteDialog,wxDialog)
END_EVENT_TABLE()

BruteDialog::BruteDialog(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size) {
    wxDialog::Create(parent, wxID_ANY, "Brute Force");
    SetClientSize(wxSize(426, 400));
    SetMinSize(wxSize(426, 396));
    SetMaxSize(wxSize(426, -1));

    panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(460,440), wxTAB_TRAVERSAL);

    onTicText = new wxStaticText(panel, wxID_ANY, "On tic", wxPoint(180, 26), wxDefaultSize, 0);
    checkTicInput = new wxTextCtrl(panel, wxID_ANY, "0", wxPoint(236, 22),  wxSize(96, 21), wxTE_RIGHT, wxIntegerValidator<unsigned int>());

    rngToggle = new wxToggleButton(panel, wxID_ANY, "RNG", wxPoint(180, 48),  wxSize(72, 21));
    rngChoice = new wxChoice(panel, wxID_ANY, wxPoint(258, 48), wxSize(64, 21), wxArrayString{"=", "!="}, 0, wxDefaultValidator);
    rngChoice->SetSelection(0);
    rngInput  = new wxTextCtrl(panel, wxID_ANY, "0", wxPoint(330, 48),  wxSize(80, 21), wxTE_RIGHT);

    damageToggle = new wxToggleButton(panel, wxID_ANY, "Damage", wxPoint(180, 74),  wxSize(72, 21));
    damageChoice = new wxChoice(panel, wxID_ANY, wxPoint(258, 74), wxSize(64, 21), wxArrayString{"=", ">=", ">", "<=", "<"}, 0, wxDefaultValidator);
    damageChoice->SetSelection(0);
    damageInput  = new wxTextCtrl(panel, wxID_ANY, "0", wxPoint(330, 74),  wxSize(80, 21), wxTE_RIGHT, wxIntegerValidator<unsigned int>());

    xPosToggle = new wxToggleButton(panel, wxID_ANY, "X", wxPoint(180, 100),  wxSize(72, 21));
    xPosChoice = new wxChoice(panel, wxID_ANY, wxPoint(258, 100), wxSize(64, 21), wxArrayString{"=", ">=", ">", "<=", "<"}, 0, wxDefaultValidator);
    xPosChoice->SetSelection(0);
    xPosInput  = new wxTextCtrl(panel, wxID_ANY, "0", wxPoint(330, 100), wxSize(80, 21), wxTE_RIGHT, wxFloatingPointValidator<double>());

    yPosToggle = new wxToggleButton(panel, wxID_ANY, "Y", wxPoint(180, 126),  wxSize(72, 21));
    yPosChoice = new wxChoice(panel, wxID_ANY, wxPoint(258, 126), wxSize(64, 21), wxArrayString{"=", ">=", ">", "<=", "<"}, 0, wxDefaultValidator);
    yPosChoice->SetSelection(0);
    yPosInput  = new wxTextCtrl(panel, wxID_ANY, "0", wxPoint(330, 126), wxSize(80, 21), wxTE_RIGHT, wxFloatingPointValidator<double>());

    zPosToggle = new wxToggleButton(panel, wxID_ANY, "Z", wxPoint(180, 152),  wxSize(72, 21));
    zPosChoice = new wxChoice(panel, wxID_ANY, wxPoint(258, 152), wxSize(64, 21), wxArrayString{"=", ">=", ">", "<=", "<"}, 0, wxDefaultValidator);
    zPosChoice->SetSelection(0);
    zPosInput     = new wxTextCtrl(panel, wxID_ANY, "0", wxPoint(330, 152), wxSize(80, 21), wxTE_RIGHT, wxIntegerValidator<int>());

    xMomToggle = new wxToggleButton(panel, wxID_ANY, "X mom.", wxPoint(180, 178),  wxSize(72, 21));
    xMomChoice = new wxChoice(panel, wxID_ANY, wxPoint(258, 178), wxSize(64, 21), wxArrayString{"=", ">=", ">", "<=", "<"}, 0, wxDefaultValidator);
    xMomChoice->SetSelection(0);
    xMomInput  = new wxTextCtrl(panel, wxID_ANY, "0", wxPoint(330, 178), wxSize(80, 21), wxTE_RIGHT, wxFloatingPointValidator<double>());

    yMomToggle = new wxToggleButton(panel, wxID_ANY, "Y mom.", wxPoint(180, 204),  wxSize(72, 21));
    yMomChoice = new wxChoice(panel, wxID_ANY, wxPoint(258, 204), wxSize(64, 21), wxArrayString{"=", ">=", ">", "<=", "<"}, 0, wxDefaultValidator);
    yMomChoice->SetSelection(0);
    yMomInput  = new wxTextCtrl(panel, wxID_ANY, "0", wxPoint(330, 204), wxSize(80, 21), wxTE_RIGHT, wxFloatingPointValidator<double>());

    speedToggle = new wxToggleButton(panel, wxID_ANY, "Speed", wxPoint(180, 230),  wxSize(72, 21));
    speedChoice = new wxChoice(panel, wxID_ANY, wxPoint(258, 230), wxSize(64, 21), wxArrayString{"=", ">=", ">", "<=", "<"}, 0, wxDefaultValidator);
    speedChoice->SetSelection(0);
    speedInput  = new wxTextCtrl(panel, wxID_ANY, "0", wxPoint(330, 230), wxSize(80, 21), wxTE_RIGHT, wxFloatingPointValidator<double>());

    useText = new wxStaticText(panel, wxID_ANY, "Use", wxPoint(180, 284));
    useCheckBox = new wxCheckBox(panel, wxID_ANY, "", wxPoint(232, 284), wxDefaultSize, wxALIGN_RIGHT);
    useCheckBox->SetValue(false);

    bruteTicsBox = new wxListBox(panel, wxID_ANY, wxPoint(16, 16), wxSize(140, 100), 0, 0, 0);
    bruteTicsBox->Bind(wxEVT_COMMAND_LISTBOX_SELECTED, &BruteDialog::OnBruteTicsBoxSelect, this);

    addButton = new wxButton(panel, wxID_ANY, "Add", wxPoint(16, 120), wxSize(68, -1), 0);
    addButton->Bind(wxEVT_BUTTON, &BruteDialog::OnAddButtonClick, this);

    removeButton = new wxButton(panel, wxID_ANY, "Remove", wxPoint(88, 120), wxSize(68, -1), 0);
    removeButton->Bind(wxEVT_BUTTON, &BruteDialog::OnRemoveButtonClick, this);
    removeButton->Disable();

    bruteTicInput = new wxTextCtrl(panel, wxID_ANY, "", wxPoint(64, 152), wxSize(92, 21), wxTE_RIGHT, wxIntegerValidator<unsigned int>());
    bruteTicInput->Bind(wxEVT_COMMAND_TEXT_UPDATED, &BruteDialog::OnBruteTicInputText, this);
    bruteTicInput->Disable();

    strafeCmdInput = new wxTextCtrl(panel, wxID_ANY, "", wxPoint(64, 176), wxSize(92, 21), wxTE_RIGHT, wxTextValidator());
    strafeCmdInput->Bind(wxEVT_COMMAND_TEXT_UPDATED, &BruteDialog::OnStrafeCmdInputText, this);
    strafeCmdInput->Disable();

    runCmdInput = new wxTextCtrl(panel, wxID_ANY, "", wxPoint(64, 200), wxSize(92, 21), wxTE_RIGHT, wxTextValidator());
    runCmdInput->Bind(wxEVT_COMMAND_TEXT_UPDATED, &BruteDialog::OnRunCmdInputText, this);
    runCmdInput->Disable();

    turnCmdInput = new wxTextCtrl(panel, wxID_ANY, "", wxPoint(64, 224), wxSize(92, 21), wxTE_RIGHT, wxTextValidator());
    turnCmdInput ->Bind(wxEVT_COMMAND_TEXT_UPDATED, &BruteDialog::OnTurnCmdInputText, this);
    turnCmdInput->Disable();

    turnAngleButton = new wxButton(panel, ID_TURNANGLEBUTTON, "Turn", wxPoint(16, 224), wxSize(40, 21), 0);
    turnAngleButton->Disable();

    cancelButton = new wxButton(panel, wxID_ANY, "Cancel", wxPoint(100, 360), wxDefaultSize, 0, wxDefaultValidator);
    cancelButton->Bind(wxEVT_BUTTON, &BruteDialog::OnCancelClick, this);

    startButton = new wxButton(panel, wxID_ANY, "Start", wxPoint(190, 360), wxDefaultSize, 0, wxDefaultValidator);
    startButton->Bind(wxEVT_BUTTON, &BruteDialog::OnStartClick, this);

    helpButton = new wxButton(panel, wxID_ANY, "Help", wxPoint(8, 360), wxDefaultSize, 0, wxDefaultValidator);
    helpButton->Bind(wxEVT_BUTTON, &BruteDialog::OnBruteHelp, this);

    autoBruteRangeButton = new wxButton(panel, wxID_ANY, "Auto Range", wxPoint(172, 316), wxDefaultSize, 0, wxDefaultValidator);
    autoBruteRangeButton->Bind(wxEVT_BUTTON, &BruteDialog::OnAutoRange, this);

    wxString __wxRadioBoxChoices_1[3] = {"No", "Both", "Yes"};
    fireRadioBox = new wxRadioBox(panel, ID_FIRERADIOBOX, "Fire", wxPoint(16, 246), wxSize(56, 100), 3, __wxRadioBoxChoices_1, 3, wxRA_VERTICAL);
    wxString __wxRadioBoxChoices_2[3] = {"No", "Both", "Yes"};
    useRadioBox = new wxRadioBox(panel, ID_USERADIOBOX, "Use", wxPoint(80, 246), wxSize(56, 100), 3, __wxRadioBoxChoices_2, 3, wxRA_VERTICAL);
    fireRadioBox->SetSelection(0);
    useRadioBox->SetSelection(0);
    fireRadioBox->Disable();
    useRadioBox->Disable();

    workingText = new wxStaticText(panel, wxID_ANY, "Working...", wxPoint(278, 284), wxDefaultSize, 0);
    workingText->Hide();
    ticsToBruteForceBox = new wxStaticBox(panel, wxID_ANY, "Tics to brute force", wxPoint(8, 0), wxSize(156, 354), 0);
    checksBox  = new wxStaticBox( panel, wxID_ANY, "Checks", wxPoint(172, 8),  wxSize(246, 299), 0);
    ticText    = new wxStaticText(panel, wxID_ANY, "Tic",    wxPoint(16, 156), wxDefaultSize, 0);
    runText    = new wxStaticText(panel, wxID_ANY, "Run",    wxPoint(16, 204), wxDefaultSize, 0);
    strafeText = new wxStaticText(panel, wxID_ANY, "Strafe", wxPoint(16, 180), wxDefaultSize, 0);

    automaticRangeSizeText = new wxStaticText(panel, wxID_ANY, "Automatic\nrange size:", wxPoint(280, 340), wxDefaultSize, 0);
    rangeSizeChoice = new wxChoice(panel, wxID_ANY, wxPoint(352, 340), wxSize(68, 21), wxArrayString{"1", "2", "3", "4", "5", "Max Range"}, 0, wxDefaultValidator);
    rangeSizeChoice->SetSelection(2);
    avoidTurboText = new wxStaticText(panel, wxID_ANY, "Avoid Turbo:", wxPoint(280, 316), wxDefaultSize, 0);
    avoidTurboCheckbox = new wxCheckBox(panel, wxID_ANY, "", wxPoint(356, 316), wxDefaultSize, wxALIGN_RIGHT);
    avoidTurboCheckbox->SetValue(1);

    Connect(ID_FIRERADIOBOX, wxEVT_COMMAND_RADIOBOX_SELECTED, wxCommandEventHandler(BruteDialog::OnFireRadioBoxSelect));
    Connect(ID_USERADIOBOX, wxEVT_COMMAND_RADIOBOX_SELECTED, wxCommandEventHandler(BruteDialog::OnUseRadioBoxSelect));
    Connect(ID_TURNANGLEBUTTON, wxEVT_BUTTON, wxCommandEventHandler(BruteDialog::OnTurnAngleButtonClick));

    Connect(wxID_ANY, wxEVT_CLOSE_WINDOW, wxCloseEventHandler(BruteDialog::OnClose));

    (*checkTicInput) << static_cast<long>(xdre::getCurrentTic());
}

BruteDialog::~BruteDialog() {}

std::vector<signed char> BruteDialog::ParseCmds(wxString str, bool parseAsUnsigned) {
    long limitMax = 127;
    long limitMin = -128;
    if (parseAsUnsigned) {
        limitMax = 255;
        limitMin = 0;
    }

    std::vector<signed char> ret;

    auto cmds = wxStringTokenize(str, ",", wxTOKEN_STRTOK);

    for (auto& cmd : cmds) {
        auto range = wxStringTokenize(cmd, ":", wxTOKEN_STRTOK);

        if (range.GetCount() == 1) {
            long num;
            if (cmd.ToLong(&num)) {
                if (num > limitMax || num < limitMin) {
                    throw BruteException("Too big input value given.");
                }
                ret.push_back(num);
            } else {
                throw BruteException("Invalid value.");
            }
        } else if (range.GetCount() == 2) {
            long start, end;
            if (!range[0].ToLong(&start) || !range[1].ToLong(&end)) {
                throw BruteException("Invalid range given.");
            }

            if (start > limitMax || start < limitMin || end > limitMax || end < limitMin) {
                throw BruteException("Too big input value given.");
            }

            if (start <= end) {
                for (; start <= end; ++start) {
                    ret.push_back(start);
                }
            } else for (; start >= end; --start) {
                ret.push_back(start);
            }
        } else {
            throw BruteException("Invalid range given.");
        }
    }
    return ret;
}

std::vector<BruteTic> BruteDialog::CreateBruteTics() {
    std::vector<BruteTic> ret;
    for (auto& item : items) {
        BruteTic tic;
        long num;

        if (!item.ticStr.ToLong(&num)) {
            throw BruteException("Tic value not a number.");
        } else if (num < 0) {
            throw BruteException("Tic value < 0.");
        }

        tic.tic = num;
        tic.fire = item.fire - 1;
        tic.use = item.use - 1;

        if (tic.fire < -1 || tic.fire > 1 || tic.use < -1 || tic.use > 1) {
            throw BruteException("Invalid fire/use choices.");
        }

        bool turnsUnsigned;
        if (item.turnOrAngle == 0) {
            tic.turnsAreAngles = false;
            turnsUnsigned = false;
        } else {
            tic.turnsAreAngles = true;
            turnsUnsigned = true;
        }

        tic.runCmds = ParseCmds(item.runStr);
        tic.strafeCmds = ParseCmds(item.strafeStr);
        tic.turnCmds = ParseCmds(item.turnStr, turnsUnsigned);

        if (tic.runCmds.empty() || tic.strafeCmds.empty() || tic.turnCmds.empty()) {
            throw BruteException("Empty field.");
        }
        ret.push_back(tic);
    }
    return ret;
}

BruteCheck BruteDialog::CreateBruteCheck() {
    BruteCheck check;

    long tic;
    if (!checkTicInput->GetValue().ToLong(&tic)) {
        throw BruteException("Checktic not a number.");
    } else if(tic < 0) {
        throw BruteException("Checktic < 0.");
    }

    check.tic = tic;

    //TODO: refactor this shit
    if (rngToggle->GetValue()) {
        auto indexes = ParseCmds(rngInput->GetValue(), true);
        switch (rngChoice->GetSelection()) {
            case 0: // "="
            check.rngFunc = [indexes](int val) {
                for (auto index : indexes) {
                    if (val == index) {
                        return true;
                    }
                }
                return false;
            };
            break;

            case 1: // "!="
            check.rngFunc = [indexes](int val) {
                for (auto index : indexes) {
                    if (val == index) {
                        return false;
                    }
                }
                return true;
            };
            break;
        }
    } else {
        check.rngFunc = [](int val) {
            return true;
        };
    }

    if (damageToggle->GetValue()) {
        long num;
        if (!damageInput->GetValue().ToLong(&num)) {
            throw BruteException("Invalid damage.");
        }
        switch (damageChoice->GetSelection()) {
        case 0: check.damageFunc = [num](int val){return val == num;}; break;
        case 1: check.damageFunc = [num](int val){return val >= num;}; break;
        case 2: check.damageFunc = [num](int val){return val >  num;}; break;
        case 3: check.damageFunc = [num](int val){return val <= num;}; break;
        case 4: check.damageFunc = [num](int val){return val <  num;}; break;
        }
    } else {check.damageFunc = [](int val){return true;};}

    if (xPosToggle->GetValue()) {
        double dbl;
        if (!xPosInput->GetValue().ToDouble(&dbl)) {
            throw BruteException("Invalid X position.");
        }
        switch (xPosChoice->GetSelection()) {
        case 0: check.xPosFunc = [dbl](double val){return val == dbl;}; break;
        case 1: check.xPosFunc = [dbl](double val){return val >= dbl;}; break;
        case 2: check.xPosFunc = [dbl](double val){return val >  dbl;}; break;
        case 3: check.xPosFunc = [dbl](double val){return val <= dbl;}; break;
        case 4: check.xPosFunc = [dbl](double val){return val <  dbl;}; break;
        }
    } else {check.xPosFunc = [](double val){return true;};}

    if (yPosToggle->GetValue()) {
        double dbl;
        if (!yPosInput->GetValue().ToDouble(&dbl)) {
            throw BruteException("Invalid Y position.");
        }
        switch (yPosChoice->GetSelection()) {
        case 0: check.yPosFunc = [dbl](double val){return val == dbl;}; break;
        case 1: check.yPosFunc = [dbl](double val){return val >= dbl;}; break;
        case 2: check.yPosFunc = [dbl](double val){return val >  dbl;}; break;
        case 3: check.yPosFunc = [dbl](double val){return val <= dbl;}; break;
        case 4: check.yPosFunc = [dbl](double val){return val <  dbl;}; break;
        }
    } else {check.yPosFunc = [](double val){return true;};}

    if (zPosToggle->GetValue()) {
        double dbl;
        if (!zPosInput->GetValue().ToDouble(&dbl)) {
            throw BruteException("Invalid Z position.");
        }
        switch (zPosChoice->GetSelection()) {
        case 0: check.zPosFunc = [dbl](double val){return val == dbl;}; break;
        case 1: check.zPosFunc = [dbl](double val){return val >= dbl;}; break;
        case 2: check.zPosFunc = [dbl](double val){return val >  dbl;}; break;
        case 3: check.zPosFunc = [dbl](double val){return val <= dbl;}; break;
        case 4: check.zPosFunc = [dbl](double val){return val <  dbl;}; break;
        }
    } else {check.zPosFunc = [](double val){return true;};}

    if (xMomToggle->GetValue()) {
        double dbl;
        if (!xMomInput->GetValue().ToDouble(&dbl)) {
            throw BruteException("Invalid X momentum.");
        }
        switch (xMomChoice->GetSelection()) {
        case 0: check.xMomFunc = [dbl](double val){return val == dbl;}; break;
        case 1: check.xMomFunc = [dbl](double val){return val >= dbl;}; break;
        case 2: check.xMomFunc = [dbl](double val){return val >  dbl;}; break;
        case 3: check.xMomFunc = [dbl](double val){return val <= dbl;}; break;
        case 4: check.xMomFunc = [dbl](double val){return val <  dbl;}; break;
        }
    } else {check.xMomFunc = [](double val){return true;};}

    if (yMomToggle->GetValue()) {
        double dbl;
        if (!yMomInput->GetValue().ToDouble(&dbl)) {
            throw BruteException("Invalid Y momentum.");
        }
        switch (yMomChoice->GetSelection()) {
        case 0: check.yMomFunc = [dbl](double val){return val == dbl;}; break;
        case 1: check.yMomFunc = [dbl](double val){return val >= dbl;}; break;
        case 2: check.yMomFunc = [dbl](double val){return val >  dbl;}; break;
        case 3: check.yMomFunc = [dbl](double val){return val <= dbl;}; break;
        case 4: check.yMomFunc = [dbl](double val){return val <  dbl;}; break;
        }
    } else {check.yMomFunc = [](double val){return true;};}

    if (speedToggle->GetValue()) {
        double dbl;
        if (!speedInput->GetValue().ToDouble(&dbl)) {
            throw BruteException("Invalid speed.");
        }
        switch (speedChoice->GetSelection()) {
        case 0: check.speedFunc = [dbl](double val){return val == dbl;}; break;
        case 1: check.speedFunc = [dbl](double val){return val >= dbl;}; break;
        case 2: check.speedFunc = [dbl](double val){return val >  dbl;}; break;
        case 3: check.speedFunc = [dbl](double val){return val <= dbl;}; break;
        case 4: check.speedFunc = [dbl](double val){return val <  dbl;}; break;
        }
    } else {check.speedFunc = [](double val){return true;};}

    check.use = useCheckBox->GetValue();
    return check;
}

void BruteDialog::ShowData(int n) {
    ListItem const& data = items.at(n);
    bruteTicInput->ChangeValue(data.ticStr);
    runCmdInput->ChangeValue(data.runStr);
    strafeCmdInput->ChangeValue(data.strafeStr);
    turnCmdInput->ChangeValue(data.turnStr);
    fireRadioBox->SetSelection(data.fire);
    useRadioBox->SetSelection(data.use);
    turnAngleButton->SetLabel(turnAngleTexts[data.turnOrAngle]);
}

void BruteDialog::EnableStuff() {
    bruteTicInput->Enable();
    runCmdInput->Enable();
    strafeCmdInput->Enable();
    turnCmdInput->Enable();
    fireRadioBox->Enable();
    useRadioBox->Enable();
    turnAngleButton->Enable();
    removeButton->Enable();
}

void BruteDialog::DisableStuff() {
    bruteTicInput->Disable();
    runCmdInput->Disable();
    strafeCmdInput->Disable();
    turnCmdInput->Disable();
    fireRadioBox->Disable();
    useRadioBox->Disable();
    turnAngleButton->Disable();
    removeButton->Disable();
}

void BruteDialog::OnClose(wxCloseEvent& event) {
    if (GetThread() && GetThread()->IsRunning()) {
        abortBrute = true;
        GetThread()->Wait();
        Destroy();
        return;
    }
    Destroy();
}

void BruteDialog::OnCancelClick(wxCommandEvent& event) {
    if (GetThread() && GetThread()->IsRunning()) {
        abortBrute = true;
        GetThread()->Wait();
        return;
    }
    Close();
}

void BruteDialog::OnTurnAngleButtonClick(wxCommandEvent& event) {
    auto item = bruteTicsBox->GetSelection();
    if (items[item].turnOrAngle) {
        items[item].turnOrAngle = 0;
    } else {
        items[item].turnOrAngle = 1;
    }
    turnAngleButton->SetLabel(turnAngleTexts[items[item].turnOrAngle]);
}

void BruteDialog::OnAddButtonClick(wxCommandEvent& event) {
    /* ORIGINAL CODE FROM XDRE 2.22
    items.push_back({});
    int last = items.size() - 1;
    bruteTicsBox->Append(items.at(last).ticStr);
    bruteTicsBox->SetSelection(last);
    items.at(last).ticStr = wxString::Format(wxT("%i"), xdre::getCurrentTic());
    bruteTicsBox->SetString(last, items.at(last).ticStr);
    EnableStuff();
    ShowData(last); */

    items.push_back({});
    bruteTicsBox->Append(items.at(items.size()-1).ticStr);
    bruteTicsBox->SetSelection(items.size()-1);

    /* This creates a list with higher values at the top. Code below creates a list with higher values at bottom.
    for(int i=0; i<(bruteTicsBox->GetCount()); i++) {
        items.at(i).ticStr = wxString::Format(wxT("%i"), xdre::getCurrentTic()-i);
        bruteTicsBox->SetString(i, items.at(i).ticStr);
    } */

    for (int i=bruteTicsBox->GetCount()-1; i>=0; i--) {
        items.at(i).ticStr = wxString::Format(wxT("%i"), xdre::getCurrentTic() - bruteTicsBox->GetCount()+i+1);
        bruteTicsBox->SetString(i, items.at(i).ticStr);
    }
    EnableStuff();
    ShowData(items.size()-1);
}


void BruteDialog::OnAutoRange(wxCommandEvent& event) {
    if (bruteTicsBox->GetCount() < 1) {
        return;
    }

    int autoRangeSize = 0;
    bool maxRange = false;
    switch (rangeSizeChoice->GetSelection()) {
    case 0: autoRangeSize = 1; break;
    case 1: autoRangeSize = 2; break;
    case 2: autoRangeSize = 3; break;
    case 3: autoRangeSize = 4; break;
    case 4: autoRangeSize = 5; break;
    case 5: maxRange = true; break;
    }

    if (autoRangeSize != 0) {
        for (int i=bruteTicsBox->GetCount()-1; i>=0; i--) {
            if (avoidTurboCheckbox->GetValue()) {
                if (xdre::getCurrentStrafe(i, bruteTicsBox->GetCount()) + autoRangeSize > 50) {
                    items.at(i).strafeStr = wxString::Format(wxT("%i" ":" "50"), 50-(2*autoRangeSize));
                } else if (xdre::getCurrentStrafe(i, bruteTicsBox->GetCount()) - autoRangeSize < -50) {
                    items.at(i).strafeStr = wxString::Format(wxT("%i" ":" "-50"), -50+(2*autoRangeSize));
                } else {
                    items.at(i).strafeStr = wxString::Format(wxT("%i" ":" "%i"), xdre::getCurrentStrafe(i, bruteTicsBox->GetCount())-autoRangeSize, xdre::getCurrentStrafe(i, bruteTicsBox->GetCount())+autoRangeSize);
                }
            } else {
                items.at(i).strafeStr = wxString::Format(wxT("%i" ":" "%i"), xdre::getCurrentStrafe(i, bruteTicsBox->GetCount())-autoRangeSize, xdre::getCurrentStrafe(i, bruteTicsBox->GetCount())+autoRangeSize);
            }

            if(avoidTurboCheckbox->GetValue()){
                if (xdre::getCurrentRun(i, bruteTicsBox->GetCount()) + autoRangeSize > 50) {
                    items.at(i).runStr = wxString::Format(wxT("%i" ":" "50"), 50-(2*autoRangeSize));
                } else if (xdre::getCurrentRun(i, bruteTicsBox->GetCount()) - autoRangeSize < -50) {
                    items.at(i).runStr = wxString::Format(wxT("%i" ":" "-50"), -50+(2*autoRangeSize));
                } else {
                    items.at(i).runStr = wxString::Format(wxT("%i" ":" "%i"), xdre::getCurrentRun(i, bruteTicsBox->GetCount())-autoRangeSize, xdre::getCurrentRun(i, bruteTicsBox->GetCount())+autoRangeSize);
                }
            } else {
                items.at(i).runStr = wxString::Format(wxT("%i" ":" "%i"), xdre::getCurrentRun(i, bruteTicsBox->GetCount())-autoRangeSize, xdre::getCurrentRun(i, bruteTicsBox->GetCount())+autoRangeSize);
            }
            // Automatic turn range produced is longtic value, %255 converts it to shorttic equivalent. Turn range deliberately 1 value wider each side than movement values
            items.at(i).turnStr = wxString::Format(wxT("%i" ":" "%i"), (xdre::getCurrentTurn(i, bruteTicsBox->GetCount()))%255-autoRangeSize-1, (xdre::getCurrentTurn(i, bruteTicsBox->GetCount()))%255+autoRangeSize+1);
        }
    }

    // Covers same movements as 50:-50 50:-50 127:-128 (except for bad transposition angles) in 1/4 the time 
    // Good for pure movement requirements, probably fine for firing/using requirements
    if (maxRange == 1) {
        for (int i=bruteTicsBox->GetCount()-1; i>=0; i--) {
            items.at(i).strafeStr = wxString::Format(wxT("50" ":" "-50"));
            items.at(i).runStr = wxString::Format(wxT("50" ":" "-50"));
            items.at(i).turnStr = wxString::Format(wxT("0" ":" "63"));
        }
    }

    ShowData(bruteTicsBox->GetSelection());
}

void BruteDialog::OnRemoveButtonClick(wxCommandEvent& event) {
    int item = bruteTicsBox->GetSelection();
    if (item == wxNOT_FOUND) {
        return;
    }

    bruteTicsBox->Delete(item);
    items.erase(std::begin(items) + item);
    int sel = (item >= 0 && static_cast<size_t>(item) < items.size()) ? item : static_cast<int>(items.size()) - 1;
    bruteTicsBox->SetSelection(sel);

    if (sel >= 0) {
        ShowData(sel);
    }

    if (bruteTicsBox->GetCount() < 1) {
        DisableStuff();
    }
}

void BruteDialog::OnBruteTicsBoxSelect(wxCommandEvent& event) {
    ShowData(bruteTicsBox->GetSelection());
}

void BruteDialog::OnBruteTicInputText(wxCommandEvent& event) {
    int item = bruteTicsBox->GetSelection();

    if (item == wxNOT_FOUND) {
        return;
    }

    items.at(item).ticStr = bruteTicInput->GetValue();
    if(items.at(item).ticStr.IsSameAs("")) {
        items.at(item).ticStr = "_";
    }
    bruteTicsBox->SetString(item, items.at(item).ticStr);
}

void BruteDialog::OnRunCmdInputText(wxCommandEvent& event) {
    int item = bruteTicsBox->GetSelection();
    if (item == wxNOT_FOUND) {
        return;
    }
    items.at(item).runStr = runCmdInput->GetValue();
}

void BruteDialog::OnStrafeCmdInputText(wxCommandEvent& event) {
    int item = bruteTicsBox->GetSelection();
    if (item == wxNOT_FOUND) {
        return;
    }
    items.at(item).strafeStr = strafeCmdInput->GetValue();
}

void BruteDialog::OnTurnCmdInputText(wxCommandEvent& event) {
    int item = bruteTicsBox->GetSelection();
    if (item == wxNOT_FOUND) {
        return;
    }
    items.at(item).turnStr = turnCmdInput->GetValue();
}

void BruteDialog::OnFireRadioBoxSelect(wxCommandEvent& event) {
    int item = bruteTicsBox->GetSelection();
    if (item == wxNOT_FOUND) {
        return;
    }
    items.at(item).fire = fireRadioBox->GetSelection();
}

void BruteDialog::OnUseRadioBoxSelect(wxCommandEvent& event) {
    int item = bruteTicsBox->GetSelection();
    if (item == wxNOT_FOUND) {
        return;
    }
    items.at(item).use = useRadioBox->GetSelection();
}

wxThread::ExitCode BruteDialog::Entry() {
    bruteSuccess = xdre::bruteForce(bruteTics, bruteCheck, abortBrute);
    wxQueueEvent(this, new wxThreadEvent(EVT_BRUTE_DONE));
    return static_cast<wxThread::ExitCode>(0);
}

void BruteDialog::OnBruteDone(wxThreadEvent& event) {
    Disconnect(wxID_ANY, EVT_BRUTE_DONE, wxThreadEventHandler(BruteDialog::OnBruteDone));
    startButton->Enable();
    workingText->Hide();

    if (bruteSuccess) {
        wxMessageBox("Found it!", "Success", wxOK | wxCENTRE, this);
        if (GetThread() && GetThread()->IsRunning())
            GetThread()->Wait();
//        Close();
    } else if (!bruteSuccess && !abortBrute) {
        wxMessageBox("Whole range examined. No cigar.", "No success", wxOK | wxCENTRE,this);
    }
}

void BruteDialog::OnStartClick(wxCommandEvent& event) {
    if (GetThread() && GetThread()->IsRunning())
        return;

    try {
        bruteTics = CreateBruteTics();
        if (bruteTics.empty()) return;
        bruteCheck = CreateBruteCheck();

        if (CreateThread(wxTHREAD_JOINABLE) == wxTHREAD_NO_ERROR) {
            Connect(wxID_ANY, EVT_BRUTE_DONE, wxThreadEventHandler(BruteDialog::OnBruteDone));
            startButton->Disable();
            abortBrute = false;
            bruteSuccess = false;
            if (GetThread()->Run() == wxTHREAD_NO_ERROR) {
                workingText->Show();
            } else {
                wxMessageBox("Couldn't start brute force thread.", "Brute force error", wxOK | wxCENTRE | wxICON_ERROR, this);
                Disconnect(wxID_ANY, EVT_BRUTE_DONE, wxThreadEventHandler(BruteDialog::OnBruteDone));
                startButton->Enable();
                workingText->Hide();
            }
        } else {
            wxMessageBox("Couldn't create brute force thread.", "Brute force error", wxOK | wxCENTRE | wxICON_ERROR, this);
        }
    } catch (BruteException& e) {
        wxMessageBox(e.what(), "Brute force error", wxOK | wxCENTRE | wxICON_ERROR, this);
    }
}

void BruteDialog::OnBruteHelp(wxCommandEvent& event) {
    wxMessageBox(
        "\'Brute force\' tests a range of movement inputs & finds the ones necessary "
        "for a desired outcome, if that outcome is possible.\n"
        "On the left, add the tic numbers you would like to brute force and enter the "
        "movement value range. The range must be entered for each tic individually.\n\n"

        "Select each tic in the list and alter its values individually.\n"
        "For strafing, negative values mean SL and positive values mean SR.\n"
        "For running, negative values are MB and positive values mean MF.\n"
        "For turning, negative values mean TR and positive values mean TL.\n\n"
        
        "x:y will test every value between, and including, x and y.\n"
        "   - Run 10:15 will test MF10, MF11, MF12, MF13, MF14 & MF15.\n"
        "x,y,z will test every value separated by the comma, and not values between.\n"
        "   - Run 10,15 will test MF10 & MF15.\n\n"

        "On the right, choose the tic you would like to search for an outcome on, and then determine "
        "that outcome. This part is as simple as it looks; 'On tic 100' searches for the outcome on "
        "that tic, and searching for Y = 100 will test every combination of values searching for "
        "Y = 100 on tic 100.\n\n"

        "'Auto Range' considers the inputs already present on a given tic before opening the brute "
        "force window and automatically generates strafing, running & turning values in the range to "
        "brute force. The number selected in 'Automatic range size' defines the size of the brute force "
        "range.\n"
        "Wider ranges have greater success chances, but will take longer to calculate.\n"
        "Turning angles are deliberately 1 value wider, as angles affect the chance of success "
        "enough for it to be worth it.\n"
        "'Avoid Turbo' affects only the auto range button, and will ensure the running and strafing range "
        "does not exceed values of 50, while keeping the range the same size.\n"
        "   - If movement before entering brute force is MF49 SR49, and an auto range of size 3 is "
        "automatically generated, the range will be 46:52, 46:52, -4:4.\n"
        "   - If the 'Avoid Turbo' button is checked, the range will be 44:50, 44:50, -4:4.\n\n"
        "Currently a little bugged - bottom tic in list must be selected when clicking the Auto Range button, "
        "& it wont properly work if the tic list isn't in order. For example, trying to set an auto range for 2, "
        "4 & 5 will create an auto range based on existing inputs on 3, 4 & 5.",
        "Brute Force Help",
        wxOK|wxICON_INFORMATION, this);
}

