/**************************************************************************
*   Copyright (C) 2013 by Thomas Luebking                                 *
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

#ifndef BE_WATCHED
#define BE_WATCHED

#include <QFileSystemWatcher>
#include <QSettings>

#include <QtDBus/QDBusContext>
#include <QHash>
#include <QObject>
#include <QStringList>

namespace BE {
class Watcher : public QObject, QDBusContext {
    Q_OBJECT
public:
    Watcher();
private slots:
    void fileChanged(const QString &path);
    void dbusCalled();
private:
    void callDBus(const QString &instruction);
    QString readExec(QSettings &settings, const QString &group);
    void reconfigure();
    void run(QString cmd);
private:
    QHash<QString, QString> m_files;
    typedef QHash<QString, QPair<QString, QStringList> > DBusMap;
    DBusMap m_dbus;
    QString m_settings;
    QFileSystemWatcher *m_fsWatcher;
};
}

#endif // BE_WATCHED