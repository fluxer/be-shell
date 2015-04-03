/**************************************************************************
*   Copyright (C) 2014 by Thomas Lübking                                  *
*   thomas.luebking@gmail.com                                             *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/

#ifndef WMCTRL_H
#define WMCTRL_H

class WmButton;
#include <QFrame>

#include "be.plugged.h"

namespace BE {

class WmCtrl : public QFrame, public Plugged
{
    Q_OBJECT
public:
    WmCtrl(QWidget *parent);
    void configure(KConfigGroup *grp);
    void saveSettings(KConfigGroup *grp);
    void themeChanged();
protected:
    void mouseMoveEvent(QMouseEvent *ev);
    void mousePressEvent(QMouseEvent *ev);
private:
    void enableButtons(bool);
private slots:
    void buttonClicked();
    void setManagedWindow(WId id);
    void windowChanged(WId, const unsigned long *props);
private:
    WId myManagedWindow;
    bool iOnlyManageMaximized, iHideDisabled;
    QString myButtons;
};

}

#endif // MEDIATRAY_H