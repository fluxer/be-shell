/**************************************************************************
 *   Copyright (C) 2011 by Thomas Lübking                                  *
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

#ifndef LABEL_H
#define LABEL_H

#include <QDBusInterface>
#include <QFile>
#include <QProcess>
#include <QTimer>

#include "be.plugged.h"

#include <QLabel>

namespace BE {

class Label : public QLabel, public Plugged
{
    Q_OBJECT
public:
    Label(QWidget *parent = 0);
    void configure( KConfigGroup *grp );
protected:
    void enterEvent(QEvent *e);
    void hideEvent(QHideEvent *he);
    void leaveEvent(QEvent *e);
    void mousePressEvent(QMouseEvent *me);
    void showEvent(QShowEvent *se);
    void timerEvent(QTimerEvent *te);
    void wheelEvent(QWheelEvent *we);
private slots:
    void protectedExec(const QString &cmd);
    void updateContents();
    void readFiFo();
    void startMovie();
    void stopMovie();
private:
    void poll();
    int myTimer, myPollInterval, myLines, myMovieLoops;
    QString myCommand;
    QProcess *myProcess;
    QDBusInterface *myDBus;
    QList<QVariant> *myDBusArgs;
    QStringList myPermittedCommands;
    bool myReplyIsPending;
    bool iStartMovieOnShow;
    QFile *myFiFo;
    Label *myToolTip;
    QTimer *myToolTipTimer;
};

}
#endif // BUTTON_H