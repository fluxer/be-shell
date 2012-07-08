/**************************************************************************
*   Copyright (C) 2009 by Thomas Luebking                                  *
*   thomas.luebking@web.de                                                *
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

#ifndef SESSION_H
#define SESSION_H

class QAction;
class KConfigGroup;
class QMenu;

#include <QToolButton>
#include "be.plugged.h"

namespace BE {

class Session : public QToolButton, public Plugged
{
    Q_OBJECT
public:
    enum Termination { PowerOff = 1, Logout, Reboot, Suspend };
    Session(QWidget *parent = 0);
    void configure( KConfigGroup *grp );
    void saveSettings( KConfigGroup *grp );
protected:
    void mousePressEvent(QMouseEvent *ev);
private slots:
    void activateSession();
    void rescueDialogFinished(int);
    void lockscreen();
    void logout();
    void login();
    void reboot();
//     void sessionsUpdated();
    void suspend();
    void shutdown();
    void updateSessions();
    void updateSettings();
private:
    QAction *showIcon, *useFullName, *mySessionAction;
    QMenu *myConfigMenu, *mySessionMenu;
};
}
#endif // SESSION_H
