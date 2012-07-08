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

#include <KDE/KUniqueApplication>
#include <KDE/KAboutData>
#include <KDE/KCmdLineArgs>

// #include <QtDBus>
// #include <QTimer>

#include "runner.h"
// #include "be.run_dbus.h"

int main (int argc, char *argv[])
{
    KAboutData aboutData(
    // The program name used internally.
    "be.run",
    // The message catalog name
    // If null, program name is used instead.
    0,
    // A displayable program name string.
    ki18n("BE::run"),
    // The program version string.
    "1.0",
    // Short description of what the app does.
    ki18n("Runner for BE::shell"),
    // The license this code is released under
    KAboutData::License_GPL,
    // Copyright Statement
    ki18n("(c) 2009"),
    // Optional text shown in the About box.
    // Can contain any information desired.
    ki18n("Some text..."),
    // The program homepage string.
    "http://www.kde.org",
    // The bug report email address
    "submit@bugs.kde.org");

    KCmdLineArgs::init( argc, argv, &aboutData );
    KUniqueApplication app;
    app.disableSessionManagement();

//     QDBusInterface ksmserver( "org.kde.ksmserver", "/KSMServer", "org.kde.KSMServerInterface", QDBusConnection::sessionBus() );
//     const QString startupID("workspace desktop");
//     ksmserver.call(QLatin1String("suspendStartup"), startupID);

    BE::Run *r = new BE::Run;
    r->resize(300,600);
//     DesktopWidget localDesktop;
//     new BE::RunAdaptor(&localDesktop);
//     localDesktop.show();

//     ksmserver.call(QLatin1String("resumeStartup"), startupID);

//     QTimer::singleShot(0, &localDesktop, SLOT(slotUpdateStyleSheet()));

    return app.exec();
}

