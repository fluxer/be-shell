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
#include <QPointer>
#include <QProcess>
#include <QSettings>
#include <QStringList>
#include <QThread>
#include <QTimer>

#include <signal.h>
#include <unistd.h>

#include "be.idle.imap.h"

#include <QtDebug>

static QList<Idler*> s_idlers;

void signalHandler(int signal)
{
    if (signal == SIGHUP) {
        foreach(Idler *i, s_idlers)
            i->restart();
    }
}

int main (int argc, char **argv)
{
    signal(SIGHUP, signalHandler);
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
    if (m_idler)
        m_idler->deleteLater();
}

void run() {
    m_idler = new Idler(0, m_account);
    s_idlers << m_idler;
    connect(m_idler, SIGNAL(newMail()), m_manager, SLOT(gotMail()));
    connect(m_idler, SIGNAL(destroyed(QObject*)), SLOT(quit()));
    connect(this, SIGNAL(finished()), SLOT(deleteLater()));
    m_account.id.clear();
    m_account.server.clear();
    m_account.login.clear();
    m_account.pass.clear();
    exec();
}

private:
    Account m_account;
    IdleManager *m_manager;
    QPointer<Idler> m_idler;
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
        account.ignoredErrors = settings.value("IgnoredErrors", QString()).toString();
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
    const Idler *idler = qobject_cast<Idler*>(sender());
    if (!idler)
        return;
    int recent = 0;
    foreach (const Idler *i, s_idlers)
        recent += i->recentMails();
    QProcess::startDetached(m_exec.arg('"' + idler->account() + '"')
                                   .arg('"' + QString::number(idler->recentMails()) + '"')
                                   .arg('"' + QString::number(recent) + '"'));
}


inline bool compare(const QString &s, const char *c) {
    return !qstricmp(s.toLatin1().data(), c);
}

#define RETURN_ERROR(_ERR_) if (string == #_ERR_) return QSslError::_ERR_
QSslError::SslError sslError(QString string) {
    RETURN_ERROR(UnableToGetIssuerCertificate);
    RETURN_ERROR(UnableToDecryptCertificateSignature);
    RETURN_ERROR(UnableToDecodeIssuerPublicKey);
    RETURN_ERROR(CertificateSignatureFailed);
    RETURN_ERROR(CertificateNotYetValid);
    RETURN_ERROR(CertificateExpired);
    RETURN_ERROR(InvalidNotBeforeField);
    RETURN_ERROR(InvalidNotAfterField);
    RETURN_ERROR(SelfSignedCertificate);
    RETURN_ERROR(SelfSignedCertificateInChain);
    RETURN_ERROR(UnableToGetLocalIssuerCertificate);
    RETURN_ERROR(UnableToVerifyFirstCertificate);
    RETURN_ERROR(CertificateRevoked);
    RETURN_ERROR(InvalidCaCertificate);
    RETURN_ERROR(PathLengthExceeded);
    RETURN_ERROR(InvalidPurpose);
    RETURN_ERROR(CertificateUntrusted);
    RETURN_ERROR(CertificateRejected);
    RETURN_ERROR(SubjectIssuerMismatch);
    RETURN_ERROR(AuthorityIssuerSerialNumberMismatch);
    RETURN_ERROR(NoPeerCertificate);
    RETURN_ERROR(HostNameMismatch);
    RETURN_ERROR(UnspecifiedError);
    RETURN_ERROR(NoSslSupport);
    RETURN_ERROR(CertificateBlacklisted);
    return QSslError::NoError;
}
#undef RETURN_ERROR

Idler::Idler(QObject *parent, const Account &account) :
QSslSocket(parent)
, m_recent(0)
, m_canIdle(false)
, m_loggedIn(false)
, m_idling(false)
{
    m_account = account;
    connect (this, SIGNAL(encrypted()), SLOT(checkCaps()));
    connect (this, SIGNAL(readyRead()), SLOT(listen()));
    connect (this, SIGNAL(peerVerifyError(QSslError)), SLOT(handleVerificationError(QSslError)));
    connect (this, SIGNAL(sslErrors(QList<QSslError>)), SLOT(handleSslErrors(QList<QSslError>)));
    connect (this, SIGNAL(disconnected()), SLOT(reconnectLater()));
    m_signalTimer = new QTimer(this);
    m_signalTimer->setSingleShot(true);
    m_reIdleTimer = new QTimer(this);
    m_reIdleTimer->setSingleShot(true);
    connect(m_signalTimer, SIGNAL(timeout()), SIGNAL(newMail()));
    connect(m_reIdleTimer, SIGNAL(timeout()), SLOT(reIdle()));
    reconnect();
}

Idler::~Idler() {
    s_idlers.removeAll(this);
    disconnect (this, SIGNAL(readyRead()), this, SLOT(listen())); // ignore "BYE"
    disconnect (this, SIGNAL(disconnected()), this, SLOT(reconnectLater()));
    request("t_logout LOGOUT");
}

void Idler::handleSslErrors(const QList<QSslError> &errors)
{
    if (errors.isEmpty())
        return;
    foreach (const QSslError &error, errors)
        qDebug() << "BE::Idle.Imap" << m_account.server << error.error() << error.errorString();
    if (m_account.ignoredErrors.isEmpty())
        return;

    // WARNING! SSL Error ignoring code below!
    qDebug() << "*** WARNING ***" << m_account.server << "will by IgnoredErrors configuration ignore:\n" <<
                m_account.ignoredErrors << "\nIgnoring errors is a MAJOR security risk! Use with greatest care only!";
    if (m_account.ignoredErrors == "ALL") {
        ignoreSslErrors();
    } else {
        QStringList sl = m_account.ignoredErrors.split(',');
        QList<QSslError> sslel;
        foreach (const QString &s, sl)
            sslel << QSslError(sslError(s.trimmed()));
        ignoreSslErrors(sslel);
    }
}

void Idler::handleVerificationError(const QSslError &error)
{
    qDebug() << "BE::Idle.Imap" << m_account.server << "VERIFICATION ERROR" << error.error() << error.errorString();
}

void Idler::reconnectLater()
{
    QTimer::singleShot(1000, this, SLOT(reconnect()));
}

void Idler::reconnect()
{
    m_canIdle = false;
    m_loggedIn = false;
    m_idling = false;
    connectToHostEncrypted(m_account.server, m_account.port);
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
                    // wipe private data - would orevent required reconnection...
//                     m_account.login.clear();
//                     m_account.pass.clear();
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
            } else
                qDebug() << "WARNING could not log into" << m_account.server << "\n" << reply;
        }

        if (m_canIdle && tokens.at(0) == "t_exa") {
            Q_ASSERT(tokens.count() > 1);
            if ((m_loggedIn = compare(tokens.at(1), "OK"))) {  // we're in!
                // check what we have, that will also trigger IDLE
                checkUnseen();
                return; // not interested in anything else atm.
            }
        }

        if (tokens.at(0) == "*") {
            if (tokens.count() > 1 && compare(tokens.at(1), "BYE")) {
                // we've been disconnected
                // -> reconnect!
                qDebug() << "we were logged out, try to reconnect" << m_account.id;
                reconnectLater();
                return;
            }
            if (m_idling) {
                if (compare(tokens.last(), "EXPUNGE") || // this is not spontaneous, user interacts with some other client and has updated mails
                    compare(tokens.last(), "RECENT") ||
                    compare(tokens.last(), "EXISTS")) { // google can't recent mails :-(
                    request("done");
                    m_idling = false;
                    checkUnseen();
                    break;
                }
                continue;
            } else if (tokens.count() > 4) {
                if (compare(tokens.at(tokens.count()-2), "(UNSEEN")) {
                    QString s = tokens.at(tokens.count()-1);
                    s = s.left(s.count()-1); // " n)"
                    if (int unseen = s.toInt())
                        updateMails(unseen);
                    reIdle();
                    break; // it's all we wanted to know
                }
            }
        }
    }
}

void Idler::checkCaps()
{
    request("t_caps CAPABILITY");
}

void Idler::checkUnseen()
{
    request(QString("t_uns STATUS %1 (UNSEEN)").arg(m_account.dir));
}

void
Idler::reIdle()
{

    m_idling = !m_idling;
    if (m_idling) {
//         qDebug() << "idle another 4.5 minutes" << m_account.id;
        request("t_idle IDLE");
        // servers don't like permanent idling, so we re-idle every 4:30 minutes to prevent a timeout or spontaneous notifications (gmail does that)
        m_reIdleTimer->start((4*60 + 30)*1000);
    }
    else {
        request("done");
//         qDebug() << "stopped idling" << m_account.id;
        // sync data - this will cause another re-idle, getting us into above branch
        // we wait a second to not confuse the server
        usleep(500000);
        checkUnseen();
        // QTimer::singleShot(500, this, SLOT(checkUnseen())); doesn't work on SIGHUP invocation
        // it's threaded and the system should buffer the socket, so this is "fine"
    }
}

void
Idler::request(const QString &s)
{
    QSslSocket::write(s.toLatin1());
    if (QSslSocket::write("\r\n") < 0) {
        qDebug() << "somehow lost connection" << m_account.id;
        reconnectLater();
    }
}

void
Idler::updateMails(int recent) {
    if (m_recent == recent)
        return;
    m_recent = recent;
    m_signalTimer->start(150);
}

