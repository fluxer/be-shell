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

#include "be.shell.h"
#include "mediatray.h"
#include "flowlayout.h"


#include <KDE/KAction>
#include <KDE/KConfigGroup>
#include <KDE/KLocale>
#include <KDE/KRun>
#include <KDE/KStandardDirs>
#include <KDE/KToolInvocation>
#include <KDE/KUriFilter>
#include <KDE/KUriFilterData>

#include <solid/block.h>
#include <solid/devicenotifier.h>
#include <solid/deviceinterface.h>
#include <solid/opticaldisc.h>
#include <solid/opticaldrive.h>
#include <solid/storageaccess.h>
#include <solid/storagevolume.h>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDialog>
#include <QIcon>
#include <QMenu>
#include <QProcess>
#include <QTime>
#include <QToolButton>

#include <kdebug.h>

static QTime ejectMonitor;
QMap<QString, QString> BE::Device::ourTypeMap[BE::Device::NumTypes];

static QString sizeString(qulonglong n)
{
    QString suffix = " bytes";
    float f;
    if (n > 999)
        { n /= 1024; suffix = " Kb"; }
    if (n > 999)
        { f = n/1024.0; suffix = " Mb"; }
    else
        return QString::number(n) + suffix;
    if (f > 999.0)
        { f /= 1024.0; suffix = " Gb"; }
    if (f > 999.0)
        { f /= 1024.0; suffix = " Tb"; }
    if (f > 999.0)
        { f /= 1024.0; suffix = " Pb"; }// that should do for the moment ;-P
    return QString::number(f, 'f', 2) + suffix;
}

static Solid::StorageDrive *drive4(const Solid::Device &device)
{
    Solid::StorageDrive *drv = 0;
    Solid::Device parentDevice = device;
    while (parentDevice.isValid() && !drv)
    {
        drv = parentDevice.as<Solid::StorageDrive>();
        parentDevice = parentDevice.parent();
    }
    return drv;
}

BE::Device::Device( QWidget *parent, const Solid::Device &dev, Solid::StorageDrive *drv) :
QToolButton(parent)
{
    if (!dev.isValid())
    {
        deleteLater();
        return;
    }

//         setPopupMode( QToolButton::MenuButtonPopup );
    setMinimumSize(16,16);

    myProduct = dev.product();
    myDrv = drv ? drv : drive4(dev);
    ejectable = dev.as<Solid::OpticalDrive>();

    if (const Solid::StorageAccess *acc = dev.as<Solid::StorageAccess>())
        connect( acc, SIGNAL(accessibilityChanged(bool, const QString&)), this, SLOT(setVolume(bool, const QString&)) );

    if (dev.as<Solid::StorageVolume>())
        setVolume(false, dev.udi());
    else if (ejectable)
    {
        myDriveUdi = dev.udi();
        setEmpty();
    }
    else
        deleteLater();

    show();
}

void
BE::Device::resizeEvent(QResizeEvent *re)
{
    const int s = qMin(contentsRect().width(), contentsRect().height()) - 1;
    setIconSize(QSize(s,s));
}

void
BE::Device::setEmpty()
{
    disconnect(SIGNAL( clicked(bool) ));
    setToolTip(myProduct);
    myUdi = QString();
    myIcon = "media-eject";
    setIcon( BE::Plugged::themeIcon(myIcon) );
    delete menu(); setMenu( 0L );
    connect(this, SIGNAL( clicked(bool) ), this, SLOT(toggleEject()) );
}

void
BE::Device::extendMenu( const QMap<QString, QString> &map, QAction **defAction )
{
    QMap<QString, QString>::const_iterator i = map.constBegin();
    while (i != map.constEnd())
    {
        if (*defAction)
            menu()->addAction( i.key(), this, SLOT(runCommand()) )->setData(i.value());
        else
        {
            *defAction = menu()->addAction( i.key(), this, SLOT(runCommand()) );
            (*defAction)->setData(i.value());
        }
        ++i;
    }
    menu()->addSeparator();
}

void
BE::Device::setVolume(bool, const QString &udi)
{
    const Solid::Device dev(udi);

    disconnect(SIGNAL( clicked(bool) ));
    const Solid::StorageVolume *vol = dev.as<Solid::StorageVolume>();
    if (!vol)
    {
        ejectable ? setEmpty() : deleteLater();
        return;
    }

    myUdi = dev.udi();
    myLabel = vol->label();
    myCapacity = vol->size();
    myIcon = dev.icon();
    setIcon( BE::Plugged::themeIcon(myIcon) );
    if (!menu())
    {
        QMenu *menu = new QMenu("Actions", this);
        setMenu(menu);
    }
    else
        menu()->clear();

    QAction *defAction = 0;
    const Solid::StorageAccess *acc = dev.as<Solid::StorageAccess>();
    if (const Solid::OpticalDisc *disc = dev.as<Solid::OpticalDisc>())
    {   // maybe cdda, dvd, vcd, blank,...
        if (disc->isAppendable() || disc->isBlank() || disc->isRewritable())
            extendMenu(ourTypeMap[WritableDisc], &defAction);
        if (disc->isBlank())
        {
            menu()->addAction( "Eject", this, SLOT(toggleEject()) );
            setToolTip("Blank Disc");
            if (defAction)
                connect(this, SIGNAL( clicked(bool) ), defAction, SIGNAL(triggered()) );
            return;
        }

        if (disc->availableContent() & Solid::OpticalDisc::Audio)
            extendMenu(ourTypeMap[AudioDisc], &defAction);

        if (disc->availableContent() & (Solid::OpticalDisc::VideoCd|Solid::OpticalDisc::SuperVideoCd|Solid::OpticalDisc::VideoDvd))
            extendMenu(ourTypeMap[VideoDisc], &defAction);

        if (defAction)
            connect(this, SIGNAL( clicked(bool) ), defAction, SIGNAL(triggered()) );

        if (acc && disc->availableContent() & Solid::OpticalDisc::Data)
            setMounted(acc->isAccessible(), acc->filePath());
    }
    else if (acc)
        setMounted(acc->isAccessible(), acc->filePath());
    else
        setEmpty();

    if (!defAction)
        connect(this, SIGNAL( clicked(bool) ), this, SLOT(open()) );

    connect(this, SIGNAL( clicked(bool) ), this, SLOT(setCheckState()) );
}

void
BE::Device::runCommand()
{
    QAction *act = qobject_cast<QAction*>(sender());
    if (!act)
        return;

    QString exec = act->data().toString().trimmed();
    if (Solid::Block *block = Solid::Device(myUdi).as<Solid::Block>())
        exec.replace("%@", block->device());

    BE::Shell::run(exec);
}

void
BE::Device::mount()
{
    Solid::StorageAccess *vol = Solid::Device(myUdi).as<Solid::StorageAccess>();
    if (!vol || vol->isAccessible())
        return;
    vol->setup();
}

void
BE::Device::umount()
{
    Solid::StorageAccess *vol = Solid::Device(myUdi).as<Solid::StorageAccess>();
    if (!(vol && vol->isAccessible()))
        return;
    vol->teardown();
}

void
BE::Device::setMounted( bool mounted, const QString &path )
{
    setCheckable(mounted);
    setChecked(mounted);
    QString diskname = myLabel;
    if ( diskname.isEmpty() )
        diskname = myProduct;
    diskname += "\n" + sizeString(myCapacity);

    if (mounted)
    {
        menu()->addAction( "Open", this, SLOT(open()) );
        menu()->addSeparator();
        menu()->addAction( "Release", this, SLOT(umount()) );
        diskname += QString("\n(mounted as %1)").arg(path);
    }
    else
    {
        menu()->addAction( "Open", this, SLOT(open()) );
        menu()->addAction( "Mount", this, SLOT(mount()) );
        menu()->addSeparator();
    }
    if (ejectable)
        menu()->addAction( "Eject", this, SLOT(toggleEject()) );
    setToolTip(diskname);
}

void
BE::Device::setCheckState()
{
    Solid::StorageAccess *vol = Solid::Device(myUdi).as<Solid::StorageAccess>();
    setChecked(vol && vol->isAccessible());
}

void
BE::Device::toggleEject()
{
    if (!ejectable)
        return;

    Solid::StorageAccess *vol = Solid::Device(myUdi).as<Solid::StorageAccess>();
    if (vol && vol->isAccessible())
    if (Solid::OpticalDrive *odrv = Solid::Device(myDriveUdi).as<Solid::OpticalDrive>())
    {
        odrv->eject();
        return;
    }

    QString path = KStandardDirs::findExe( "kdeeject" );

    if( path.isEmpty())
        return;

    QProcess::startDetached( path, QStringList() << "-T" << myDriveUdi.section('/', -1) );

//     Solid::OpticalDrive *odrv = Solid::Device(myDriveUdi).as<Solid::OpticalDrive>();
//     if (odrv)
//     {
//         connect (odrv, SIGNAL(ejectDone(Solid::ErrorType, QVariant, const QString&)), this, SLOT(ejected(Solid::ErrorType)));
//         ejectMonitor.restart();
//         odrv->eject();
//     }
}

void
BE::Device::ejected(Solid::ErrorType error)
{
    if( error != Solid::NoError )
        return;

    int ms = ejectMonitor.elapsed();

    Solid::OpticalDrive *odrv = Solid::Device(myDriveUdi).as<Solid::OpticalDrive>();
    if (!odrv)
        return;

    disconnect (odrv, SIGNAL(ejectDone(Solid::ErrorType, QVariant, const QString&)), this, SLOT(ejected(Solid::ErrorType)));

    if (ms < 1000) // this went a little fast an probably means the device is already open -> we actually want to tray it
    {   // Solid:: lacks this functionality - yet TODO: once there's a sold (tm) way, use it
        QDBusInterface hal( "org.freedesktop.Hal", myDriveUdi, "org.freedesktop.Hal.Device.Storage", QDBusConnection::systemBus() );

        if (!hal.isValid())
            return;

        hal.asyncCall("CloseTray", QStringList());
    }
}

void
BE::Device::open()
{
    Solid::StorageAccess *vol = Solid::Device(myUdi).as<Solid::StorageAccess>();
    if (!vol)
        return;
    if (vol->isAccessible())
        finishOpen(Solid::NoError);
    else
    {
        connect( vol, SIGNAL(setupDone(Solid::ErrorType,QVariant,const QString&)), this, SLOT(finishOpen(Solid::ErrorType)));
        mount();
    }
}

void
BE::Device::finishOpen(Solid::ErrorType error)
{
    if( error != Solid::NoError )
        return;

    Solid::Device device(myUdi);
    if( device.isValid() )
    {
        Solid::StorageAccess *vol = device.as<Solid::StorageAccess>();
        disconnect( vol, SIGNAL(setupDone(Solid::ErrorType,QVariant,const QString&)), this, SLOT(finishOpen(Solid::ErrorType)));
        if( vol && vol->isAccessible() )
            KRun::runUrl(vol->filePath(), "inode/directory", this);
    }
}

// class ActionDialog : public QDialog
// {
// public:
//     ActionDialog( QWidget *parent ) : QDialog(parent)
//     {
//     }
// };

BE::MediaTray::MediaTray( QWidget *parent ) : QFrame(parent), BE::Plugged(parent)
{
    setObjectName("MediaTray");
    FlowLayout *l = new FlowLayout(this);
    l->setContentsMargins(3, 1, 3, 1);
    l->setSpacing(2);
    connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceAdded(const QString&)), this, SLOT(addDevice(const QString&)));
    connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceRemoved(const QString&)), this, SLOT(removeDevice(const QString&)));

//     connect(this, SIGNAL(triggered(QAction*)), this, SLOT(slotActionTriggered(QAction*)));
    QMetaObject::invokeMethod(this, "collectDevices", Qt::QueuedConnection);
}

const char *types[BE::Device::NumTypes] = { "iPod", "VidoDisc", "AudioDisc", "WritableDisc", "Camera" };

void
BE::MediaTray::addDevice( const Solid::Device &dev )
{
    if (!dev.isValid())
        return;

    Solid::StorageDrive *drv = drive4(dev);
    if ( !(drv && (drv->isHotpluggable() || drv->isRemovable())) )
        return;

    QList<Device*> devices = findChildren<Device*>();
    foreach (Device *device, devices)
    {
        if (device->drive() == drv)
        {
            device->setVolume(false, dev.udi());
            return;
        }
    }

    layout()->addWidget(new Device(this, dev, drv));
}

void BE::MediaTray::addDevice( const QString &udi ) { addDevice(Solid::Device(udi)); }

void
BE::MediaTray::collectDevices()
{
    window()->setAttribute(Qt::WA_AlwaysShowToolTips);
    foreach( Solid::Device device, Solid::Device::listFromType(Solid::DeviceInterface::OpticalDrive))
        addDevice(device);
    foreach( Solid::Device device, Solid::Device::listFromType(Solid::DeviceInterface::StorageVolume))
        addDevice(device);
}

void
BE::MediaTray::configure(KConfigGroup *grp)
{
    for (int i = 0; i < BE::Device::NumTypes; ++i )
    {
        QStringList oldActions = BE::Device::ourTypeMap[i].keys();
        QStringList oldCommands = BE::Device::ourTypeMap[i].values();
        QStringList actions = grp->readEntry(types[i] + QString("_actions"), QStringList());
        QStringList commands = grp->readEntry(types[i] + QString("_commands"), QStringList());
        if (oldActions != actions || oldCommands != commands)
        {
            BE::Device::ourTypeMap[i].clear();
            int n = qMin(actions.count(), commands.count());
            for (int j = 0; j < n; ++j)
                BE::Device::ourTypeMap[i].insertMulti(actions.at(j), commands.at(j));
        }
    }
}

void
BE::MediaTray::removeDevice( const QString &udi )
{
    QList<Device*> devices = findChildren<Device*>();
    foreach (Device *device, devices)
    {
        if (device->udi() == udi)
        {
            if (device->isEjectable())
                device->setEmpty();
            else
                device->deleteLater();
            break;
        }
    }
}

void
BE::MediaTray::saveSettings( KConfigGroup *grp )
{
}

void
BE::MediaTray::themeChanged()
{
    QList<BE::Device*> devices = findChildren<BE::Device*>();
    foreach (BE::Device *device, devices)
        device->setIcon( themeIcon(device->iconString()) );
}

