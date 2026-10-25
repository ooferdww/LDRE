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
 * Description: demo header editor declarations.
*/

#ifndef __HEADERDIALOG_HPP
#define __HEADERDIALOG_HPP

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/panel.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/tglbtn.h>
#include <wx/wx.h> // TODO, find out where Layout() and Fit() are from
#include "MainWindow.hpp" // For being able to access RS()
#include "xdre.hpp"

class HeaderDialog: public wxDialog {
    public:
        HeaderDialog(wxWindow* parent, wxWindowID id=wxID_ANY, const wxPoint& pos=wxDefaultPosition, const wxSize& size=wxDefaultSize);
        virtual ~HeaderDialog();

        wxPanel *panel;
        wxButton *applyButton;
        wxStaticBox *boxAroundTypedValues;
        wxStaticText *complevelText, *skillText, *episodeText, *mapText, *deathmatchText, *consolePlayerText, *variableFrictionText,
            *rngText, *dogsText, *friendDistanceText, *compatFlagsText;;
        wxChoice *complevelChoice, *deathmatchChoice;
        wxTextCtrl *skillInput, *episodeInput, *mapInput, *consolePlayerInput, *variableFrictionInput, *dogsInput, *distFriendInput,
            *compatFlagsInput;
        wxToggleButton *respawnToggle, *fastToggle, *noMonstersToggle, *player1Toggle, *player2Toggle, *player3Toggle, *player4Toggle,
            *monstersRememberTargetsToggle, *weaponRecoilToggle, *allowPushersToggle, *playerBobbingToggle, *demoInsuranceToggle,
            *monsterInfightingToggle, *monstersCanGoBackwardsToggle, *monstersAvoidHazardsToggle, *frictionAppliesToMonstersToggle,
            *monstersHelpFriendsToggle, *dogsCanJumpDownToggle, *climbSteepStairsToggle, *longticsToggle;
        // I thought about wxSpinCtrl for Map/Episode/Skill, but setting no limits is a problem, and
        // I didn't want to set a limit & risk conflicting with emerging formats and possibilities
        wxSpinCtrl *rngSeedSpinCtrl;

    private:
        void OnApply(wxCommandEvent& event);
        void OnComplevelChoice(wxCommandEvent& event);
        void OnDeathmatchChoice(wxCommandEvent& event);
        void OnGeneralToggle(wxCommandEvent& event);
        void OnPlayerToggle(wxCommandEvent& event);
};

#endif //__HEADERDIALOG_HPP
