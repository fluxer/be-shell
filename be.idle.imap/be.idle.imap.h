/**************************************************************************
 *   Copyright (C) 2012 by Thomas Lübking                                  *
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

class QTimer;

#include <QSslSocket>
#include <QStringList>

class IdleManager : public QObject {
    Q_OBJECT
public:
    IdleManager(QObject *parent);
private slots:
    void gotMail();
private:
    QString m_exec;
};

class Account {
public:
    Account() { port = 993; }
    Account(const Account &other) {
        id = other.id;
        server = other.server;
        login = other.login;
        pass = other.pass;
        dir = other.dir;
        port = other.port;
    }
    QString id, server, login, pass, dir;
    int port;
};

class Idler : public QSslSocket {
    Q_OBJECT
public:
    Idler(QObject *parent, const Account &account);
    ~Idler();
    inline const QString &account() const { return m_account.id; }
    inline int recentMails() const { return m_recent; }
signals:
    void newMail();
    void lostConnection(QString);
private slots:
    void checkUnseen();
    void checkCaps();
    void reconnect();
    void reconnectLater();
    void listen();
    void handleSslErrors(const QList<QSslError> &errors);
    void handleVerificationError(const QSslError &error);
    void reIdle();
private:
    void request(const QString &s);
    void updateMails(int recent);
private:
    Account m_account;
    int m_recent;
    QTimer *m_signalTimer, *m_reIdleTimer;
    bool m_canIdle, m_loggedIn, m_idling;
};