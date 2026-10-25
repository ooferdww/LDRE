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
 * Description: demo header editor.
*/

#include "HeaderDialog.hpp"

#include <wx/intl.h>
#include <wx/string.h>
#include <wx/valnum.h>
#include <wx/valtext.h>
#include <wx/msgdlg.h>

class HeaderException : public std::exception {
public:
    HeaderException(const char* str) : str{str} {}
    virtual const char* what() const noexcept {return str;}
    const char* str;
};

int options[28];
HeaderDialog::HeaderDialog(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size) {
    wxDialog::Create(parent, wxID_ANY, "Header editor", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE & ~wxRESIZE_BORDER);
    SetClientSize(wxSize(448, 400));
    xdre::getOptions(options); // In cstuff.c

    panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(444, 400), wxTAB_TRAVERSAL);

    applyButton = new wxButton(panel, wxID_ANY, "Apply", wxPoint(16,  140), wxSize(82, 22), 0, wxDefaultValidator);

    boxAroundTypedValues = new wxStaticBox(panel, wxID_ANY, "", wxPoint(8, 33), wxSize(98, 137), 0);

    // Sorted by type, then in order of appearance in x_getOptions() in cstuff.c
    complevelText        = new wxStaticText(panel, wxID_ANY, "Complevel",          wxPoint(8,   14),  wxDefaultSize, 0); //options[0]
    skillText            = new wxStaticText(panel, wxID_ANY, "Skill",              wxPoint(16,  53),  wxDefaultSize, 0); //options[1]
    episodeText          = new wxStaticText(panel, wxID_ANY, "Episode",            wxPoint(16,  83),  wxDefaultSize, 0); //options[2]
    mapText              = new wxStaticText(panel, wxID_ANY, "Map",                wxPoint(16,  113), wxDefaultSize, 0); //options[3]
    deathmatchText       = new wxStaticText(panel, wxID_ANY, "Deathmatch",         wxPoint(228, 14),  wxDefaultSize, 0); //options[4]
    consolePlayerText    = new wxStaticText(panel, wxID_ANY, "Console\nplayer",    wxPoint(16,  192), wxDefaultSize, 0); //options[8]
    variableFrictionText = new wxStaticText(panel, wxID_ANY, "Variable\nfriction", wxPoint(16,  232), wxDefaultSize, 0); //options[11]
    rngText              = new wxStaticText(panel, wxID_ANY, "(P)RNG seed",        wxPoint(222, 112), wxDefaultSize, 0); //options[16]
    dogsText             = new wxStaticText(panel, wxID_ANY, "Dogs\n(0-3)",        wxPoint(16,  272), wxDefaultSize, 0); //options[18]
    friendDistanceText   = new wxStaticText(panel, wxID_ANY, "Friend\ndistance",   wxPoint(16,  312), wxDefaultSize, 0); //options[19]
    compatFlagsText      = new wxStaticText(panel, wxID_ANY, "Compat\nflags",      wxPoint(16,  352), wxDefaultSize, 0); //options[26]

    complevelChoice = new wxChoice(panel, wxID_ANY, wxPoint(80, 8), wxSize(140, 28), wxArrayString{
        "0 - Doom V1.2",
        "1 - Doom v1.666",
        "2 - Doom v1.9",
        "3 - Ultimate Doom & Doom95",
        "4 - Final Doom",
        "5 - DOSDoom",
        "6 - TASDoom",
        "7 - Boom's inaccurate vanilla compatibility mode",
        "8 - Boom v2.01",
        "9 - Boom V2.02",
        "10 - LxDoom",
        "11 - MBF",
        "12 - PrBoom v2.03beta",
        "13 - PrBoom v2.1.0",
        "14 - PrBoom v2.1.1 - 2.2.6",
        "15 - PrBoom v2.3.x",
        "16 - PrBoom v2.4.0",
        "17 - Engine defaults (\"current version\")"}, 0, wxDefaultValidator); //options[0]
    deathmatchChoice = new wxChoice(panel, wxID_ANY, wxPoint(300, 8), wxSize(140, 28), wxArrayString{
        "0 - Single-player/Co-op",
        "1 - deathmatch",
        "2 - altdeath"}, 0, wxDefaultValidator); //options[4]
    complevelChoice->SetSelection(options[0]);
    deathmatchChoice->SetSelection(0);

    skillInput            = new wxTextCtrl(panel, wxID_ANY, "", wxPoint(68, 50),  wxSize(30, 22), wxTE_RIGHT, wxIntegerValidator<unsigned int>()); //options[1]
    episodeInput          = new wxTextCtrl(panel, wxID_ANY, "", wxPoint(68, 80),  wxSize(30, 22), wxTE_RIGHT, wxIntegerValidator<unsigned int>()); //options[2]
    mapInput              = new wxTextCtrl(panel, wxID_ANY, "", wxPoint(68, 110), wxSize(30, 22), wxTE_RIGHT, wxIntegerValidator<unsigned int>()); //options[3]
    consolePlayerInput    = new wxTextCtrl(panel, wxID_ANY, "", wxPoint(68, 200), wxSize(30, 22), wxTE_RIGHT, wxIntegerValidator<unsigned int>()); //options[8]
    variableFrictionInput = new wxTextCtrl(panel, wxID_ANY, "", wxPoint(68, 240), wxSize(30, 22), wxTE_RIGHT, wxIntegerValidator<unsigned int>()); //options[11]
    dogsInput             = new wxTextCtrl(panel, wxID_ANY, "", wxPoint(68, 280), wxSize(30, 22), wxTE_RIGHT, wxIntegerValidator<unsigned int>()); //options[18]
    distFriendInput       = new wxTextCtrl(panel, wxID_ANY, "", wxPoint(68, 320), wxSize(40, 22), wxTE_RIGHT, wxIntegerValidator<unsigned int>()); //options[19]
    compatFlagsInput      = new wxTextCtrl(panel, wxID_ANY, "", wxPoint(68, 360), wxSize(64, 22), wxTE_RIGHT, wxIntegerValidator<unsigned int>()); //options[26]
    skillInput           ->ChangeValue(wxString::FromCDouble(options[1], 0));
    episodeInput         ->ChangeValue(wxString::FromCDouble(options[2], 0));
    mapInput             ->ChangeValue(wxString::FromCDouble(options[3], 0));
    consolePlayerInput   ->ChangeValue(wxString::FromCDouble(options[8], 0));
    variableFrictionInput->ChangeValue(wxString::FromCDouble(options[11], 0));
    dogsInput            ->ChangeValue(wxString::FromCDouble(options[18], 0));
    distFriendInput      ->ChangeValue(wxString::FromCDouble(options[19], 0));
    compatFlagsInput     ->ChangeValue(wxString::FromCDouble(options[26], 0));

    respawnToggle                   = new wxToggleButton(panel, wxID_ANY, "Respawn",                      wxPoint(114, 42),  wxSize(100, 22)); //options[5]
    fastToggle                      = new wxToggleButton(panel, wxID_ANY, "Fast",                         wxPoint(114, 76),  wxSize(100, 22)); //options[6]
    noMonstersToggle                = new wxToggleButton(panel, wxID_ANY, "No Monsters",                  wxPoint(114, 110), wxSize(100, 22)); //options[7]
    player1Toggle                   = new wxToggleButton(panel, wxID_ANY, "Player 1",                     wxPoint(226, 42),  wxSize(100, 22)); //options[9]
    player2Toggle                   = new wxToggleButton(panel, wxID_ANY, "Player 2",                     wxPoint(338, 42),  wxSize(100, 22)); //options[9]
    player3Toggle                   = new wxToggleButton(panel, wxID_ANY, "Player 3",                     wxPoint(226, 76),  wxSize(100, 22)); //options[9]
    player4Toggle                   = new wxToggleButton(panel, wxID_ANY, "Player 4",                     wxPoint(338, 76),  wxSize(100, 22)); //options[9]
    monstersRememberTargetsToggle   = new wxToggleButton(panel, wxID_ANY, "Monsters remember target",     wxPoint(114, 260), wxSize(156, 22)); //options[10]
    weaponRecoilToggle              = new wxToggleButton(panel, wxID_ANY, "Weapon recoil",                wxPoint(114, 200), wxSize(100, 22)); //options[12]
    allowPushersToggle              = new wxToggleButton(panel, wxID_ANY, "Allow pushers",                wxPoint(226, 200), wxSize(100, 22)); //options[13]
    playerBobbingToggle             = new wxToggleButton(panel, wxID_ANY, "Player bobbing",               wxPoint(338, 200), wxSize(100, 22)); //options[14]
    demoInsuranceToggle             = new wxToggleButton(panel, wxID_ANY, "Demo insurance",               wxPoint(114, 230), wxSize(100, 22)); //options[15]
    monsterInfightingToggle         = new wxToggleButton(panel, wxID_ANY, "Monster infighting",           wxPoint(282, 260), wxSize(156, 22)); //options[17]
    monstersCanGoBackwardsToggle    = new wxToggleButton(panel, wxID_ANY, "Monsters can go backwards",    wxPoint(114, 290), wxSize(156, 22)); //options[20]
    monstersAvoidHazardsToggle      = new wxToggleButton(panel, wxID_ANY, "Monsters avoid hazards",       wxPoint(282, 290), wxSize(156, 22)); //options[21]
    frictionAppliesToMonstersToggle = new wxToggleButton(panel, wxID_ANY, "Friction applies to monsters", wxPoint(226, 230), wxSize(212, 22)); //options[22]
    monstersHelpFriendsToggle       = new wxToggleButton(panel, wxID_ANY, "Monsters help friends",        wxPoint(114, 320), wxSize(156, 22)); //options[23]
    dogsCanJumpDownToggle           = new wxToggleButton(panel, wxID_ANY, "Dogs can jump down",           wxPoint(282, 320), wxSize(156, 22)); //options[24]
    climbSteepStairsToggle          = new wxToggleButton(panel, wxID_ANY, "Climb steep stairs",           wxPoint(140, 350), wxSize(156, 22)); //options[25]
    longticsToggle                  = new wxToggleButton(panel, wxID_ANY, "Longtics",                     wxPoint(114, 144), wxSize(100, 22)); //options[27]

    rngSeedSpinCtrl = new wxSpinCtrl(panel, wxID_ANY, wxEmptyString, wxPoint(302, 110), wxSize(136, 22), wxSP_ARROW_KEYS, 1, 1000000, 1); //options[16]
    rngSeedSpinCtrl ->SetValue(wxString::FromCDouble(options[16], 0));

    //if ((wxString::FromCDouble(options[4],  0)) == "1") deathmatchCheckbox->SetValue(true); else deathmatchCheckbox->SetValue(false);
    if ((wxString::FromCDouble(options[5],  0)) == "1")    respawnToggle->SetValue(true); else    respawnToggle->SetValue(false);
    if ((wxString::FromCDouble(options[6],  0)) == "1")       fastToggle->SetValue(true); else       fastToggle->SetValue(false);
    if ((wxString::FromCDouble(options[7],  0)) == "1") noMonstersToggle->SetValue(true); else noMonstersToggle->SetValue(false);
    if (options[9] & (1 << 0)) player1Toggle->SetValue(1);
    if (options[9] & (1 << 1)) player2Toggle->SetValue(1);
    if (options[9] & (1 << 2)) player3Toggle->SetValue(1);
    if (options[9] & (1 << 3)) player4Toggle->SetValue(1);
    if ((wxString::FromCDouble(options[10], 0)) == "1")   monstersRememberTargetsToggle->SetValue(true); else   monstersRememberTargetsToggle->SetValue(false);
    if ((wxString::FromCDouble(options[12], 0)) == "1")              weaponRecoilToggle->SetValue(true); else              weaponRecoilToggle->SetValue(false);
    if ((wxString::FromCDouble(options[13], 0)) == "1")              allowPushersToggle->SetValue(true); else              allowPushersToggle->SetValue(false);
    if ((wxString::FromCDouble(options[14], 0)) == "1")             playerBobbingToggle->SetValue(true); else             playerBobbingToggle->SetValue(false);
    if ((wxString::FromCDouble(options[15], 0)) == "1")             demoInsuranceToggle->SetValue(true); else             demoInsuranceToggle->SetValue(false);
    if ((wxString::FromCDouble(options[17], 0)) == "1")         monsterInfightingToggle->SetValue(true); else         monsterInfightingToggle->SetValue(false);
    if ((wxString::FromCDouble(options[20], 0)) == "1")    monstersCanGoBackwardsToggle->SetValue(true); else    monstersCanGoBackwardsToggle->SetValue(false);
    if ((wxString::FromCDouble(options[21], 0)) == "1")      monstersAvoidHazardsToggle->SetValue(true); else      monstersAvoidHazardsToggle->SetValue(false);
    if ((wxString::FromCDouble(options[22], 0)) == "1") frictionAppliesToMonstersToggle->SetValue(true); else frictionAppliesToMonstersToggle->SetValue(false);
    if ((wxString::FromCDouble(options[23], 0)) == "1")       monstersHelpFriendsToggle->SetValue(true); else       monstersHelpFriendsToggle->SetValue(false);
    if ((wxString::FromCDouble(options[24], 0)) == "1")           dogsCanJumpDownToggle->SetValue(true); else           dogsCanJumpDownToggle->SetValue(false);
    if ((wxString::FromCDouble(options[25], 0)) == "1")          climbSteepStairsToggle->SetValue(true); else          climbSteepStairsToggle->SetValue(false);
    if ((wxString::FromCDouble(options[27], 0)) == "1")                  longticsToggle->SetValue(true); else                  longticsToggle->SetValue(false);

    applyButton                    ->Bind(wxEVT_BUTTON,       &HeaderDialog::OnApply, this);
    complevelChoice                ->Bind(wxEVT_CHOICE,       &HeaderDialog::OnComplevelChoice, this);
    deathmatchChoice               ->Bind(wxEVT_CHOICE,       &HeaderDialog::OnDeathmatchChoice, this);
    respawnToggle                  ->Bind(wxEVT_TOGGLEBUTTON, &HeaderDialog::OnGeneralToggle, this);
    fastToggle                     ->Bind(wxEVT_TOGGLEBUTTON, &HeaderDialog::OnGeneralToggle, this);
    noMonstersToggle               ->Bind(wxEVT_TOGGLEBUTTON, &HeaderDialog::OnGeneralToggle, this);
    player1Toggle                  ->Bind(wxEVT_TOGGLEBUTTON, &HeaderDialog::OnPlayerToggle, this);
    player2Toggle                  ->Bind(wxEVT_TOGGLEBUTTON, &HeaderDialog::OnPlayerToggle, this);
    player3Toggle                  ->Bind(wxEVT_TOGGLEBUTTON, &HeaderDialog::OnPlayerToggle, this);
    player4Toggle                  ->Bind(wxEVT_TOGGLEBUTTON, &HeaderDialog::OnPlayerToggle, this);
    longticsToggle                 ->Bind(wxEVT_TOGGLEBUTTON, &HeaderDialog::OnGeneralToggle, this);
    monstersRememberTargetsToggle  ->Bind(wxEVT_TOGGLEBUTTON, &HeaderDialog::OnGeneralToggle, this);
    weaponRecoilToggle             ->Bind(wxEVT_TOGGLEBUTTON, &HeaderDialog::OnGeneralToggle, this);
    allowPushersToggle             ->Bind(wxEVT_TOGGLEBUTTON, &HeaderDialog::OnGeneralToggle, this);
    playerBobbingToggle            ->Bind(wxEVT_TOGGLEBUTTON, &HeaderDialog::OnGeneralToggle, this);
    demoInsuranceToggle            ->Bind(wxEVT_TOGGLEBUTTON, &HeaderDialog::OnGeneralToggle, this);
    monsterInfightingToggle        ->Bind(wxEVT_TOGGLEBUTTON, &HeaderDialog::OnGeneralToggle, this);
    monstersCanGoBackwardsToggle   ->Bind(wxEVT_TOGGLEBUTTON, &HeaderDialog::OnGeneralToggle, this);
    monstersAvoidHazardsToggle     ->Bind(wxEVT_TOGGLEBUTTON, &HeaderDialog::OnGeneralToggle, this);
    frictionAppliesToMonstersToggle->Bind(wxEVT_TOGGLEBUTTON, &HeaderDialog::OnGeneralToggle, this);
    monstersHelpFriendsToggle      ->Bind(wxEVT_TOGGLEBUTTON, &HeaderDialog::OnGeneralToggle, this);
    dogsCanJumpDownToggle          ->Bind(wxEVT_TOGGLEBUTTON, &HeaderDialog::OnGeneralToggle, this);
    climbSteepStairsToggle         ->Bind(wxEVT_TOGGLEBUTTON, &HeaderDialog::OnGeneralToggle, this);
    rngSeedSpinCtrl                ->Bind(wxEVT_SPINCTRL,     &HeaderDialog::OnGeneralToggle, this);
}

HeaderDialog::~HeaderDialog() {} // Destructor, compilation fails if this is absent despite doing nothing

void HeaderDialog::OnApply(wxCommandEvent& event) {
    try {
        signed long i;
        if (skillInput          ->GetValue().ToCLong(&i)) options[1] = i;
        if (episodeInput        ->GetValue().ToCLong(&i)) options[2] = i;
        if (mapInput            ->GetValue().ToCLong(&i)) options[3] = i;
        if (consolePlayerInput  ->GetValue().ToCLong(&i)) options[8] = i;
        if (options[8] > 3)
            throw HeaderException("Invalid consoleplayer.");
        if(variableFrictionInput->GetValue().ToCLong(&i)) options[11] = i;
        if(dogsInput            ->GetValue().ToCLong(&i)) options[18] = i;
        if(distFriendInput      ->GetValue().ToCLong(&i)) options[19] = i;
        if(compatFlagsInput     ->GetValue().ToCLong(&i)) options[26] = i;

        /* Force-read the toggle button */
        options[7] = noMonstersToggle->GetValue() ? 1 : 0;
        options[5] = respawnToggle->GetValue() ? 1 : 0;
        options[6] = fastToggle->GetValue() ? 1 : 0;

        xdre::changeOptions(options);
        MainWindow* XDREWindow = dynamic_cast<MainWindow*>(GetParent());
        XDREWindow->MainWindow::RS();
    }
    catch(HeaderException& e) {
        wxMessageBox(e.what(), "Invalid value(s)", wxOK|wxCENTRE|wxICON_ERROR, this);
    }
}

void HeaderDialog::OnComplevelChoice(wxCommandEvent& event) {
    if (complevelChoice->GetSelection() <= 17)
        options[0] = complevelChoice->GetSelection();
    else if (complevelChoice->GetSelection() == 18) // No complevel 18, 19 or 20
        options[0] = 21;

    // Changes applied now immediately change the game screen AND the information in the XDRE screen
    xdre::changeOptions(options);
    MainWindow* XDREWindow = dynamic_cast<MainWindow*>(GetParent());
    XDREWindow->MainWindow::RS();
}

void HeaderDialog::OnDeathmatchChoice(wxCommandEvent& event) {
    options[4] = deathmatchChoice->GetSelection();
}

void HeaderDialog::OnGeneralToggle(wxCommandEvent& event) {
    // if (deathmatchCheckbox                ->GetValue()) options[4]  = 1; else options[4]  = 0;
    if (respawnToggle                   ->GetValue()) options[5]  = 1; else options[5]  = 0;
    if (fastToggle                      ->GetValue()) options[6]  = 1; else options[6]  = 0;
    if (noMonstersToggle                ->GetValue()) options[7]  = 1; else options[7]  = 0;
    if (monstersRememberTargetsToggle   ->GetValue()) options[10] = 1; else options[10] = 0;
    if (weaponRecoilToggle              ->GetValue()) options[12] = 1; else options[12] = 0;
    if (allowPushersToggle              ->GetValue()) options[13] = 1; else options[13] = 0;
    if (playerBobbingToggle             ->GetValue()) options[14] = 1; else options[14] = 0;
    if (demoInsuranceToggle             ->GetValue()) options[15] = 1; else options[15] = 0;
    options[16] = rngSeedSpinCtrl->GetValue(); // Okay fine, this isn't a toggle, but it's conveniently dealt with here
    if (monsterInfightingToggle         ->GetValue()) options[17] = 1; else options[17] = 0;
    if (monstersCanGoBackwardsToggle    ->GetValue()) options[20] = 1; else options[20] = 0;
    if (monstersAvoidHazardsToggle      ->GetValue()) options[21] = 1; else options[21] = 0;
    if (frictionAppliesToMonstersToggle ->GetValue()) options[22] = 1; else options[22] = 0;
    if (monstersHelpFriendsToggle       ->GetValue()) options[23] = 1; else options[23] = 0;
    if (dogsCanJumpDownToggle           ->GetValue()) options[24] = 1; else options[24] = 0;
    if (climbSteepStairsToggle          ->GetValue()) options[25] = 1; else options[25] = 0;
    if (longticsToggle                  ->GetValue()) options[27] = 1; else options[27] = 0;

    xdre::changeOptions(options);
    MainWindow* XDREWindow = dynamic_cast<MainWindow*>(GetParent());
    XDREWindow->MainWindow::RS();
}

void HeaderDialog::OnPlayerToggle(wxCommandEvent& event) {
    // Players present are set with bit flags, any combination of four players is possible

    // Ensure at least one player present
    if (!player1Toggle->GetValue() && !player2Toggle->GetValue() && !player3Toggle->GetValue() && !player4Toggle->GetValue())
        player1Toggle->SetValue(1);

    int players = 0;
    if (player1Toggle->GetValue()) players += 1; // 0001
    if (player2Toggle->GetValue()) players += 2; // 0010
    if (player3Toggle->GetValue()) players += 4; // 0100
    if (player4Toggle->GetValue()) players += 8; // 1000
    options[9] = players;

    xdre::changeOptions(options);
    MainWindow* XDREWindow = dynamic_cast<MainWindow*>(GetParent());
    XDREWindow->MainWindow::RS();
}
