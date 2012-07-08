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

#ifndef CLAWS_H
#define CLAWS_H

#include "be.plugged.h"

#include <QToolButton>

namespace BE {

class Claws : public QToolButton, public Plugged
{
  Q_OBJECT
public:
    Claws(QWidget *parent = 0);
//     void setNewMails(int n);
protected:
    void resizeEvent(QResizeEvent *re);
    void themeChanged();
//     void wheelEvent(QWheelEvent *ev);
private slots:
    void toggleClaws();
    void updateNewMails();
};

}
#endif // CLAWS_H
