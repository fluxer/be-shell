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

#ifndef VOLUME_H
#define VOLUME_H

#include <QProcess>
#include <QTimer>

class KConfigGroup;

#include "pixlabel.h"
#include "be.plugged.h"

namespace BE {

class Volume : public PixLabel, public Plugged
{
    class OSD : public QWidget
    {
    public:
        OSD(QWidget *parent = 0);
        void show(int total, int done);
    protected:
        void paintEvent(QPaintEvent *ev);
    private:
        int myProgress, myTarget;
        QTimer *hideTimer;
    };

    Q_OBJECT
public:
    Volume(QWidget *parent = 0);
    void configure( KConfigGroup *grp );
    void themeChanged();
protected:
    void mousePressEvent(QMouseEvent *ev);
    void wheelEvent(QWheelEvent *ev);
private slots:
    void down();
    void launchMixer();
    void toggleMute();
    void sync();
    void up();
private:
    void read(QProcess &amixer);
    void updateValue();
    void set(int v, QChar dir = QChar());
    QString myChannel, myMixerCommand, myDevice;
    bool iAmMuted;
    int myStep, myValue, myUnmutedValue, myPollInterval, myMaxVolume;
    OSD *myOSD;
    QTimer *syncTimer;
};
}
#endif // VOLUME_H
