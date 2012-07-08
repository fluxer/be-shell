/**************************************************************************
*   Copyright (C) 2009 by Thomas Luebking                                  *
*   thomas.luebking@web.de                                                *
*   Copyright (C) 2009 by Ian Reinhart Geiser                             *
*   geiseri@kde.org                                                       *
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

#ifndef CLOCK_H
#define CLOCK_H

#include <QLabel>
#include "be.plugged.h"

class QMenu;

namespace BE {

class Clock : public QLabel, public Plugged
{
    Q_OBJECT
public:
    Clock(QWidget *parent = 0, const QString &pattern = "ddd/hh:mm");
    void configure( KConfigGroup *grp );
    void saveSettings( KConfigGroup *grp );
    const QString &pattern() { return myPattern; }
protected:
    bool event(QEvent *ev);
    void mousePressEvent(QMouseEvent *ev);
private slots:
    void configTime();
    void configPattern();
private:
    QString myPattern;
    int myTimer;
    QMenu *myConfigMenu;
};

}
#endif // CLOCK_H
