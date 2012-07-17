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

#include <QCoreApplication>
#include <QProcess>
#include <QSettings>
#include <QStringList>
#include <QThread>
#include <QTimer>

#include "be.idle.imap.h"

#include <QtDebug>

int main (int argc, char **argv)
{
    QCoreApplication a(argc, argv);
    new IdleManager(&a);
    return a.exec();
}

class IdleThread : public QThread {
public:
IdleThread(IdleManager *parent, const Account &account)
    : QThread(parent)
    , m_account(account)
    , m_manager(parent) {}

~IdleThread()
{
    m_idler->deleteLater();
}

void run() {
    m_idler = new Idler(0, m_account);
    connect(m_idler, SIGNAL(newMail()), m_manager, SLOT(gotMail()));
    connect(m_idler, SIGNAL(destroyed(QObject*)), SLOT(quit()));
    m_account.id.clear();
    m_account.server.clear();
    m_account.login.clear();
    m_account.pass.clear();
    exec();
}

private:
    Account m_account;
    IdleManager *m_manager;
    Idler *m_idler;
};

#define READ(_S_, _s_) \
account._s_ = settings.value(#_S_).toString();\
if (account._s_.isEmpty()) { \
    qDebug() << "** WARNING ** account" << group << "lacks config for" << #_S_ << "- skipping..."; \
    settings.endGroup(); \
    continue;\
} //

IdleManager::IdleManager(QObject *parent) : QObject(parent)
{
    QSettings settings("be.idle.imap");
    m_exec = settings.value("Exec", QString()).toString();
    if (m_exec.isEmpty()) {
        qWarning("There is no \"Exec\" key for what to do on new mails.\nMakes this daemon pointless.\nBye.");
        exit(1);
    }
    QStringList accounts = settings.value("Accounts", QStringList()).toStringList();
    if (accounts.isEmpty()) {
        qWarning("There are no \"Accounts\" configured. Don't know what to do.\nBye.");
        exit(1);
    }
    Account account;
    bool valid = false;
    foreach (const QString &group, accounts) {
        account.id = group;
        settings.beginGroup(group);
        READ(Server, server);
        READ(Login, login);
        READ(Password, pass);
        valid = true;
        account.dir = settings.value("Directory", "INBOX").toString();
        account.port = settings.value("Port", 993).toUInt();
        settings.endGroup();
        IdleThread *it = new IdleThread(this, account);
        it->start();
    }
    if (!valid) {
        qWarning("There are no VALID \"Accounts\" configured. I'm worth nothing.\nBye.");
        exit(1);
    }
}

void
IdleManager::gotMail()
{
    if (const Idler *idler = qobject_cast<Idler*>(sender()))
        QProcess::startDetached(m_exec.arg('"' + idler->account() + '"').arg('"' + QString::number(idler->recentMails()) + '"'));
}


inline bool compare(const QString &s, const char *c) {
    return !qstricmp(s.toLatin1().data(), c);
}

Idler::Idler(QObject *parent, const Account &account) :
QSslSocket(parent)
, m_canIdle(false)
, m_loggedIn(false)
, m_idling(false)
{
    m_account = account;
    connect (this, SIGNAL(encrypted()), SLOT(checkCaps()));
    connect (this, SIGNAL(readyRead()), SLOT(listen()));
    connect (this, SIGNAL(peerVerifyError(QSslError)), SLOT(handleVerificationError(QSslError)));
    connect (this, SIGNAL(sslErrors(QList<QSslError>)), SLOT(handleSslErrors(QList<QSslError>)));
    m_signalTimer = new QTimer(this);
    m_signalTimer->setSingleShot(true);
    connect(m_signalTimer, SIGNAL(timeout()), SIGNAL(newMail()));
    connectToHostEncrypted(m_account.server, m_account.port);
}

Idler::~Idler() {
    request("t_logout LOGOUT");
}

void Idler::handleSslErrors(const QList<QSslError> &errors)
{
    foreach (const QSslError &error, errors)
        qDebug() << "BE::Idle.Imap" << m_account.server << error.error() << error.errorString();
}

void Idler::handleVerificationError(const QSslError &error)
{
    qDebug() << "BE::Idle.Imap" << m_account.server << "VERIFICATION ERROR" << error.error() << error.errorString();
}


void Idler::listen()
{
    QStringList msg(QString(readAll()).split("\r\n"));
    foreach (const QString &reply, msg) {
//         qDebug() << m_account.id << reply;
        QStringList tokens(reply.split(" "));
        if (tokens.isEmpty())
            continue;

        if (!m_canIdle && tokens.count() > 1 && compare(tokens.at(1), "CAPABILITY")) {
            foreach (const QString &token, tokens) {
                if (compare(token, "IDLE")) {
                    m_canIdle = true;
                    request(QString("t_login LOGIN %1 %2").arg(m_account.login).arg(m_account.pass));
                    // wipe private data
                    m_account.login.clear();
                    m_account.pass.clear();
                    break;
                }
            }
            if (!m_canIdle) {
                qDebug() << m_account.server << "does not support IMAP IDLE, sorry";
                deleteLater();
                return;
            }
            return; // we didn't ask anything before the capability test
        }

        if (m_canIdle && tokens.at(0) == "t_login") {
            Q_ASSERT(tokens.count() > 1);
            if ((m_loggedIn = compare(tokens.at(1), "OK"))) {  // we're in!
                request(QString("t_exa EXAMINE %1").arg(m_account.dir));
                return; // not interested in anything else atm.
            }
        }

        if (m_canIdle && tokens.at(0) == "t_exa") {
            Q_ASSERT(tokens.count() > 1);
            if ((m_loggedIn = compare(tokens.at(1), "OK"))) {  // we're in!
                reIdle();
                return; // not interested in anything else atm.
            }
        }

        if (m_idling && tokens.at(0) == "*") {
            if (compare(tokens.at(tokens.count()-1), "EXPUNGE")) // this is not spontaneous, user interacts
                break; // ... with some other client and has updated mails and we get notified -> skip
            if (compare(tokens.at(tokens.count()-1), "RECENT"))
                updateMails(tokens.at(1).toInt());
            else if (compare(tokens.at(tokens.count()-1), "EXISTS")) // google can't recent mails
                updateMails(0);
            continue;
        }
    }
}

void Idler::checkCaps()
{
    request("t_caps CAPABILITY");
}

void
Idler::reIdle()
{
    m_idling = !m_idling;
    int timeout = 0;
    if (m_idling) {
        request("t_idle IDLE");
        // servers don't like permanent idling, so we re-idle every 9 minutes to prevent a timeout
        timeout = 9*60*1000;
    }
    else {
        request("done");
        // we wait a second to not confuse the server
        timeout = 1000;
    }
    QTimer::singleShot(timeout, this, SLOT(reIdle()));
}

void
Idler::request(const QString &s)
{
    QSslSocket::write(s.toLatin1());
    QSslSocket::write("\r\n");
}

void
Idler::updateMails(int recent) {
    m_recent = qMax(1, recent);
    m_signalTimer->start(150);
}

