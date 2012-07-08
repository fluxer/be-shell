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

#ifndef TRASH_H
#define TRASH_H

#include "be.plugged.h"

class KDirLister;
class QMenu;
class QProcess;


#include <QLabel>

namespace BE {

class Trash : public QLabel, public Plugged
{
  Q_OBJECT
public:
    Trash(QWidget *parent = 0);
    bool isEmpty();
protected:
    void dragEnterEvent(QDragEnterEvent *dee);
    void dropEvent(QDropEvent *de);
    void mouseMoveEvent(QMouseEvent * ev);
    void mousePressEvent(QMouseEvent *me);
    void mouseReleaseEvent(QMouseEvent *ev);
    void resizeEvent( QResizeEvent *re );
signals:
    void moved();
private slots:
    void emptyTrash();
    void open();
    void tryEmptyTrash();
    void updateStatus();
private:
    void themeChanged();
    static KDirLister *ourTrashcan;
    static QMenu *ourTaskMenu;
};

}
#endif // TRASH_H
