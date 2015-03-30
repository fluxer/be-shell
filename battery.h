/**************************************************************************
*   Copyright (C) 2011 by Thomas Luebking                                 *
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

#include "button.h"
#include <solid/acadapter.h>
#include <solid/battery.h>
#include <QMap>

namespace BE {
class Battery : public Button {
    Q_OBJECT
public:
    Battery(QWidget *parent = 0);
//     virtual void configure( KConfigGroup *grp );
    QSize sizeHint() const;
protected:
//     void mousePressEvent(QMouseEvent *me);
//     void mouseReleaseEvent(QMouseEvent *me);
    bool event(QEvent *e);
    void paintEvent(QPaintEvent *pe);
private slots:
    void collectDevices();
    void addDevice( const QString &udi );
    void removeDevice( const QString &udi );
    void _repolish();
    void setCharge(int value, const QString &udi);
    void setState(int newState, const QString &udi);
    void setAcPlugged(bool newState, const QString &udi);
    void setBatteryPlugged(bool newState, const QString &udi);
private:
    void countCharge();
    void repolish();
private:
    bool myACisPlugged, iAmCharging, iAmDirty;
    int myCharge;
    QSize myPadding;
    QMap<QString, int> myBatteries;
    QMap<QString, bool> myACs;
};
}
