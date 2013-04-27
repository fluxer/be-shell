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

#include "be.watched.h"

#include <QCoreApplication>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCall>
#include <QFileSystemWatcher>
#include <QProcess>
#include <QSettings>

#include <QtDebug>

using namespace BE;

Watcher::Watcher() : QObject(), QDBusContext()
{
    m_fsWatcher = 0;
    reconfigure();
}

QString Watcher::readExec(QSettings &settings, const QString &group)
{
    QString exec = settings.value("Exec").toString();
    if (exec.isEmpty()) {
        exec = settings.value("DBus").toStringList().join(";");
        if (!exec.isEmpty())
            exec.prepend(":dbus:");
    }
    if (exec.isEmpty()) {
        qDebug() << group << ": Setting either \"Exec\" or \"DBus\" is required, skipping";
        settings.endGroup();
    }
    return exec;
}

void Watcher::reconfigure()
{
    delete m_fsWatcher;
    m_fsWatcher = new QFileSystemWatcher(this);
    m_files.clear();

    QSettings settings("be.watched");
    m_settings = settings.fileName(); 
    m_fsWatcher->addPath(m_settings);

    QStringList files = settings.value("Files").toStringList();
    foreach (const QString &file, files) {
        settings.beginGroup(file);
        QString path(settings.value("File").toString());
        QString exec = readExec(settings, file);
        if (exec.isEmpty())
            continue;
        m_files.insert(path, exec);
        m_fsWatcher->addPath(path);
        settings.endGroup();
    }
    connect(m_fsWatcher, SIGNAL(fileChanged(QString)), SLOT(fileChanged(QString)));



    for (DBusMap::const_iterator it = m_dbus.constBegin(), end = m_dbus.constEnd(); it != end; ++it) {
        const QStringList &l = it->second;
        if (l.at(0) == "0")
            QDBusConnection::systemBus().disconnect(l.at(1), l.at(2), l.at(3), l.at(4), l.at(5), this, SLOT(dbusCalled()));
        else
            QDBusConnection::sessionBus().disconnect(l.at(1), l.at(2), l.at(3), l.at(4), l.at(5), this, SLOT(dbusCalled()));
    }
    m_dbus.clear();

    QStringList dbusSignals = settings.value("DBusSignals").toStringList();
    foreach (const QString &dbusSignal, dbusSignals) {
        settings.beginGroup(dbusSignal);

        const QString service(settings.value("Service").toString());
        const QString path(settings.value("Path").toString());
        const QString interface(settings.value("Interface").toString());
        const QString signal(settings.value("Signal").toString());
        const QString signature(settings.value("Signature", "").toString());
        if (service.isEmpty() || path.isEmpty() || interface.isEmpty() || signal.isEmpty()) {
            qDebug() << dbusSignal << ": \"Service\", \"Path\", \"Interface\" and \"Signal\" are mandatory";
            settings.endGroup();
            continue;
        }
        QString exec = readExec(settings, dbusSignal);
        if (exec.isEmpty())
            continue;
        QString bus = settings.value("Bus", "Session").toString();
        if (!bus.compare("system", Qt::CaseInsensitive)) {
            QDBusConnection::systemBus().connect(service, path, interface, signal, signature, this, SLOT(dbusCalled()));
            bus = "0";
        } else {
            QDBusConnection::sessionBus().connect(service, path, interface, signal, signature, this, SLOT(dbusCalled()));
            bus = "1";
        }
        QStringList l; l << bus << service << path << interface << signal << signature;
        m_dbus.insert(path+interface+signal+signature, QPair<QString, QStringList>(exec,l));
        settings.endGroup();
    }
}

void Watcher::fileChanged(const QString &path)
{
    if (path == m_settings) {
        reconfigure();
        return;
    }
    m_fsWatcher->removePath(path); // the inode probably changed
    m_fsWatcher->addPath(path);
    run(m_files.value(path));
}

void Watcher::dbusCalled()
{
    const QString key = message().path() + message().interface() + message().member() + message().signature();
    DBusMap::const_iterator it = m_dbus.constFind(key);
    if (it == m_dbus.constEnd())
        return;
    QString exec = it->first;
    QList<QVariant> args = message().arguments();
    for (int i = 0; i < args.count(); ++i) {
        const QString pattern = '%' + QString::number(i+1);
        exec.replace(pattern, args.at(i).toString());
    }
    run(exec);
}

void Watcher::run(QString cmd)
{
    if (cmd.startsWith(":dbus:"))
        callDBus(cmd.mid(6));
    else
        QProcess::startDetached(cmd);
}

void Watcher::callDBus(const QString &instruction)
{
    QStringList list = instruction.split(';');
    if (list.count() < 5)
    {
        qWarning("invalid dbus chain, must be: \"bus;service;path;interface;method[;arg1;arg2;...]\", bus is \"session\" or \"system\"");
        return;
    }

    QDBusInterface *caller = 0;
    if (list.at(0) == "session")
        caller = new QDBusInterface( list.at(1), list.at(2), list.at(3), QDBusConnection::sessionBus() );
    else if (list.at(0) == "system")
        caller = new QDBusInterface( list.at(1), list.at(2), list.at(3), QDBusConnection::systemBus() );
    else
    {
        qWarning("unknown bus, must be \"session\" or \"system\"");
        return;
    }

    QList<QVariant> args;
    if (list.count() > 5)
    {
        for (int i = 5; i < list.count(); ++i)
        {
            bool ok = false;
            short Short = list.at(i).toShort(&ok);
            if (ok) { args << Short; continue; }
            unsigned short UShort = list.at(i).toUShort(&ok);
            if (ok) { args << UShort; continue; }
            int Int = list.at(i).toInt(&ok);
            if (ok) { args << Int; continue; }
            uint UInt = list.at(i).toUInt(&ok);
            if (ok) { args << UInt; continue; }
            double Double = list.at(i).toDouble(&ok);
            if (ok) { args << Double; continue; }
            if (!list.at(i).compare("true", Qt::CaseInsensitive)) {
                args << true; continue;
            }
            if (!list.at(i).compare("false", Qt::CaseInsensitive)) {
                args << false; continue;
            }

            args << list.at(i);
        }
    }
    caller->asyncCallWithArgumentList(list.at(4), args);
    delete caller;
}


int main(int argc, char **argv)
{
    if (argc > 1 && !qstricmp(argv[1], "edit")) {
        QProcess::startDetached("xdg-open", QStringList() << QSettings("be.watched").fileName());
        return 0;
    }
    QCoreApplication a(argc, argv);
    Watcher w;
    return a.exec();
}