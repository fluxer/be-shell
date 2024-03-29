/**************************************************************************
*   Copyright (C) 2009 by Thomas Lübking                                  *
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

#ifndef SHELL_ADAPTOR_H
#define SHELL_ADAPTOR_H

#include <QtDBus/QDBusAbstractAdaptor>
#include <KDE/KMenu>
#include <KDE/KGlobalSettings>
#include "be.shell.h"

namespace BE
{
    class ScreenLockerAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.ScreenSaver")

private:
    BE::Shell *myShell;

public:
    ScreenLockerAdaptor(BE::Shell *shell) : QDBusAbstractAdaptor(shell), myShell(shell)
    {
        connect ( myShell, SIGNAL(ActiveChanged()), this, SIGNAL(ActiveChanged()) );
    }

public slots:
    void Lock() { myShell->lockScreen(); }
    void configure() { myShell->configureScreenLocker(); }
signals:
    void ActiveChanged();
};

class ShellAdaptor : public QDBusAbstractAdaptor
{
   Q_OBJECT
   Q_CLASSINFO("D-Bus Interface", "org.kde.be.shell")

private:
    BE::Shell *myShell;

public:
    ShellAdaptor(BE::Shell *shell) : QDBusAbstractAdaptor(shell), myShell(shell) { }

public slots:
    QString debug(const QString &pluginName, const QString &parameter = QString()) { return myShell->debug(pluginName, parameter); }
    void hidePanel(const QString &name) { myShell->setPanelVisible(name, 0); }
    void reconfigure() { myShell->configure(); }
    void showWindowList(int x, int y) { myShell->myWindowList->popup(QPoint(x,y)); }
    void showPanel(const QString &name) { myShell->setPanelVisible(name, 1); }
    void setTheme(const QString &theme) { myShell->setTheme(theme); }
    void togglePanel(const QString &name) { myShell->setPanelVisible(name, -1); }
    void blockGlobalShortcuts(bool yesNo) { KGlobalSettings::self()->emitChange(KGlobalSettings::BlockShortcuts, yesNo); }
};
}
#endif //BLAZER_ADAPTOR_H
