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

#ifndef SYSTEMTRAY_H
#define SYSTEMTRAY_H

#include <QFrame>
#include <QPointer>
#include <QList>

#include "be.plugged.h"

class QMenu;
class QTreeWidgetItem;
class QX11EmbedWidget;
class KConfigGroup;

namespace BE {
class SysTrayIcon;
class SysTray : public QFrame, public Plugged
{
    Q_OBJECT
public:
    SysTray( QWidget *parent = 0);
    ~SysTray();
    void configure( KConfigGroup *grp );
    void saveSettings( KConfigGroup *grp );
protected:
//     void resizeEvent(QResizeEvent *ev);
    void mousePressEvent(QMouseEvent *);
    bool x11Event(XEvent *event);
private:
    void themeChanged();
    void updateUnthemed();
    friend bool x11EventFilter(void *message, long int *result);
    void damageEvent(void *);
private slots:
    void configureIcons();
    void init();
    void selfCheck();
    void toggleNastyOnes(bool);
    void toggleItem( QTreeWidgetItem*, int);
    void requestShowIcon();
private:
    QMenu *myConfigMenu;
    QTimer *healthTimer;
    QList< QPointer<SysTrayIcon> > myIcons;
    QStringList nastyOnes, unthemedOnes;
    bool nastyOnesAreVisible;
};
}

#endif // SYSTEMTRAY_H
