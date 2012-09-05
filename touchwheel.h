/**************************************************************************
*   Copyright (C) 2012 by Thomas Luebking                                 *
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

#ifndef TOUCHWHEEL_H
#define TOUCHWHEEL_H

class QToolButton;

#include <QFrame>

namespace BE {

class TouchWheel : public QFrame {
    Q_OBJECT
public:
    /**
     * Allows preventing the window from being auto-closed in reaction to certain actions
     * (like another in-process window is opened, what makes Qt close all unrelated popup windows)
     * DO NOT FORGET TO UNSET!
     */
    static void blockClose(bool b);
    /**
     * binds signal/slot toggle to QObject o; returns whether it worked (or the wheel is atm. taken)
     * do only consider forcing in reaction to a direct user input (mouseclick etc.)
     */
    static bool claimFor(QObject *o, const char *toggle, bool force = false);
    /**
     * Customize icons. If toggle is empty (the default) the button is not shown
     */
    static void setIcons(QString down, QString toggle, QString up);
    static QSize size();
    static void show(QPoint center = QPoint());
protected:
    void closeEvent(QCloseEvent *ce);
signals:
    void up();
    void down();
    void toggled();
private slots:
    void wheelDown();
    void wheelUp();
private:
    QToolButton *myUp, *myDown, *myToggle;
    bool myIconsAreDirty, iCanClose;
    QWeakPointer<QObject> myClaimer;
    TouchWheel();
    Q_DISABLE_COPY(TouchWheel)
};
}
#endif // TOUCHWHEEL_H
