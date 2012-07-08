#ifndef PAGER_H
#define PAGER_H
/**************************************************************************
*   Copyright (C) 2009 by Ian Reinhart Geiser                             *
*   geiseri@kde.org                                                       *
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

#include <QFrame>

#include "be.plugged.h"

class QButtonGroup;
class QHBoxLayout;
class QAbstractButton;

namespace BE {

class Pager : public QFrame, public Plugged
{
    Q_OBJECT
public:
    Pager( QWidget *parent = 0);

private slots:
    void currentDesktopChanged (int desktop);
    void desktopNamesChanged ();
    void numberOfDesktopsChanged (int num);
    void setCurrentDesktop( QAbstractButton *button );

private:
    QButtonGroup *myDesktops;
    QHBoxLayout *myLayout;
};

}

#endif // PAGER_H
