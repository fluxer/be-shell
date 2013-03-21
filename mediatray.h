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

#ifndef MEDIATRAY_H
#define MEDIATRAY_H

#include <QFrame>
#include <QMap>
#include <QPointer>
#include <QToolButton>
#include <solid/device.h>
#include <solid/opticaldisc.h>
#include <solid/solidnamespace.h>
#include <solid/storagedrive.h>

#include "be.plugged.h"

namespace BE {

class Device : public QToolButton
{
    Q_OBJECT
public:
    enum DefaultAction { Mount = 0, Open, UMount, Eject };
    Device( QWidget *parent, const Solid::Device &dev, Solid::StorageDrive *drv = 0);
    void setEmpty();
    void setMounted( bool mounted, const QString &path = QString() );

    inline bool isEjectable() { return ejectable; }
    inline const Solid::StorageDrive *drive() { return myDrv; }
    inline const QString &udi() { return myUdi; }
    inline const QString &iconString() { return myIcon; }
    static void updateSolidActions();

public slots:
    void setVolume(bool, const QString &udi);

protected:
    void resizeEvent(QResizeEvent *re);

private slots:
    void lostActions();
    void mount();
    void open(); void finishOpen(Solid::ErrorType error);
    void run(QAction*);
    void toggleEject(); void ejected(Solid::ErrorType error);
    void umount();
    void setCheckState();
private:
    void extendMenu( const QMap<QString, QString> &map, QAction **defAction );
    void setMediaDisc(Solid::OpticalDisc::ContentTypes type, QString label);
private:
    static QList<QAction*> ourSolidActions;
    QPointer<Solid::StorageDrive> myDrv;
    QString myUdi, myDriveUdi, myProduct, myLabel, myPath, myIcon;
    bool ejectable;
    qulonglong myCapacity;
};

// stuff like KHBox is evil... ok: inflexible (if we ever wanted this vertical...)
class MediaTray : public QFrame, public Plugged
{
    Q_OBJECT
public:
    MediaTray( QWidget *parent );
    void configure( KConfigGroup *grp );
    void saveSettings( KConfigGroup *grp );
    void themeChanged();
    static QString ejectCommand() { return ourEjectCommand; };
protected:
    void mousePressEvent(QMouseEvent *ev);
private slots:
    void addDevice( const Solid::Device &device );
    void addDevice( const QString &udi );
    void removeDevice( const QString &udi );
    void collectDevices();
    void updateSolidActions();
private:
    static bool ourSolidActionsAreDirty;
    static QString ourEjectCommand;
};

}

#endif // MEDIATRAY_H
