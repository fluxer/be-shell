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
#include <KDE/KDesktopFile>
#include <KDE/KLocale>
#include <KDE/KRun>
#include <KDE/KStandardDirs>
#include <KDE/KToolInvocation>
#include <KDE/KUriFilter>
#include <KDE/KUriFilterData>

#include <solid/block.h>
#include <solid/devicenotifier.h>
#include <solid/deviceinterface.h>
#include <solid/opticaldrive.h>
#include <solid/predicate.h>
#include <solid/storageaccess.h>
#include <solid/storagevolume.h>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDialog>
#include <QIcon>
#include <QMenu>
#include <QMouseEvent>
#include <QProcess>
#include <QTime>
#include <QToolButton>

#include <kdebug.h>

static QTime ejectMonitor;
QList<QAction*> BE::Device::ourSolidActions;
bool BE::MediaTray::ourSolidActionsAreDirty = true;
QString BE::MediaTray::ourEjectCommand("eject");

Q_DECLARE_METATYPE(Solid::Predicate)

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
    if (!dev.isValid()) {
        deleteLater();
        return;
    }

    if (ourSolidActions.isEmpty()) // TODO: thread?
        updateSolidActions();

//         setPopupMode( QToolButton::MenuButtonPopup );
    setMinimumSize(16,16);

    myProduct = dev.product();
    myDrv = drv ? drv : drive4(dev);
    ejectable = dev.as<Solid::OpticalDrive>();

    if (const Solid::StorageAccess *acc = dev.as<Solid::StorageAccess>())
        connect(acc, SIGNAL(accessibilityChanged(bool,const QString&)), SLOT(setVolume(bool,const QString&)));

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
BE::Device::lostActions()
{
    setVolume(false, myUdi);
}

void
BE::Device::resizeEvent(QResizeEvent *)
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
BE::Device::setMediaDisc(Solid::OpticalDisc::ContentTypes type, QString label)
{
    if (type & Solid::OpticalDisc::Audio)
        { myIcon = "media-optical-audio"; if (label.isEmpty()) label = "CDDA"; }
    else if (type & Solid::OpticalDisc::VideoDvd)
        { myIcon = "media-optical-dvd-video"; if (label.isEmpty()) label = "DvD"; }
    else if (type & Solid::OpticalDisc::VideoBluRay)
        { myIcon = "media-optical-blu-ray"; if (label.isEmpty()) label = "Blu-ray"; }
    else if (type & Solid::OpticalDisc::VideoCd)
        { myIcon = "media-optical-video"; if (label.isEmpty()) label = "VCD"; }
    else if (type & Solid::OpticalDisc::SuperVideoCd)
        { myIcon = "media-optical-video"; if (label.isEmpty()) label = "SVCD"; }
    else
        { myIcon = "media-optical"; if (label.isEmpty()) label = i18n("Disc"); }
    setIcon( BE::Plugged::themeIcon(myIcon) );
    setToolTip(label);
}

void
BE::Device::setVolume(bool, const QString &udi)
{
    const Solid::Device dev(udi);

    const Solid::StorageVolume *vol = dev.as<Solid::StorageVolume>();
    if (!vol)
    {
        ejectable ? setEmpty() : deleteLater();
        return;
    }
    const Solid::StorageAccess *acc = dev.as<Solid::StorageAccess>();
    if (!(acc || ejectable)) { // somehow invalid
        if (myUdi.isEmpty()) // do not replace god with junk
            deleteLater(); // but get rid of junk
        return;
    }

    disconnect(SIGNAL( clicked(bool) ));

    myUdi = dev.udi();
    myLabel = vol->label();
    myCapacity = vol->size();
    myIcon = dev.icon();
    setIcon( BE::Plugged::themeIcon(myIcon) );

    if (!menu())
    {
        QMenu *m = new QMenu("Actions", this);
        setMenu(m);
        connect (m, SIGNAL(triggered(QAction*)), SLOT(run(QAction*)));
    }
    else
        menu()->clear();

    foreach (QAction *action, ourSolidActions) {
        if (action->data().value<Solid::Predicate>().matches(dev))
            menu()->addAction(action);
    }
    if (!ourSolidActions.isEmpty())
        connect (ourSolidActions.last(), SIGNAL(destroyed()), SLOT(lostActions()), Qt::QueuedConnection);
    else
        qWarning("MediaTray has empty global action list. This is a BUG!");

    if (const Solid::OpticalDisc *disc = dev.as<Solid::OpticalDisc>()) {

        if (disc->isBlank()) {
            setToolTip(i18n("Blank Disc"));
            if (ejectable) {
                    menu()->addSeparator();
                    menu()->addAction( "Eject", this, SLOT(toggleEject()) );
            }
            return;
        }
        if (!(disc->availableContent() & Solid::OpticalDisc::Data)) {
            if (disc->availableContent()) { // some unmountable content, DvD or CDDA etc.
                setMediaDisc(disc->availableContent(), disc->label());
                if (ejectable) {
                    menu()->addSeparator();
                    menu()->addAction( "Eject", this, SLOT(toggleEject()) );
                }
                return;
            } else {
                acc = 0;
            }
        }
    }

    if (acc)
        setMounted(acc->isAccessible(), acc->filePath());
    else
        setEmpty();

    connect(this, SIGNAL( clicked(bool) ), this, SLOT(open()) );
    connect(this, SIGNAL( clicked(bool) ), this, SLOT(setCheckState()) );
}

void
BE::Device::run(QAction *act)
{
    if (!act)
        return;
    QString exec = act->toolTip().trimmed();
    Solid::Device dev(myUdi);
    if (!dev.isValid()) {
        qDebug() << "action for invalid device" << this << myUdi << "this is a BUG!";
    }
    if (exec.contains("%f")) { // want's path. first ensure it's mounted'
        if (Solid::StorageAccess *vol = dev.as<Solid::StorageAccess>()) {
            if (!vol->isAccessible()) {
                connect(vol, SIGNAL(setupDone(Solid::ErrorType,QVariant,const QString&)), act, SIGNAL(triggered()));
                vol->setup(); // the command likes the device to be mounted
                return;
            } else { // tidy up
                disconnect(vol, SIGNAL(setupDone(Solid::ErrorType,QVariant,const QString&)), act, SIGNAL(triggered()));
            }
            exec.replace("%f", vol->filePath());
        }
    }
    exec.replace("%i", myUdi);
    if (Solid::Block *block = dev.as<Solid::Block>())
        exec.replace("%@", block->device()).replace("%d", block->device());

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
//         menu()->addAction( "Open", this, SLOT(open()) );
        menu()->addSeparator();
        menu()->addAction( "Release", this, SLOT(umount()) );
        diskname += QString("\n(mounted as %1)").arg(path);
    }
    else
    {
//         menu()->addAction( "Open", this, SLOT(open()) );
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

    QString path = KStandardDirs::findExe( MediaTray::ejectCommand() );

    if( path.isEmpty())
        return;

    QString blockDevice;
    if (Solid::Block *block = Solid::Device(myDriveUdi).as<Solid::Block>())
        blockDevice = block->device();
    else // last resort - does not work with udisks2 ...
        myDriveUdi.section('/', -1);

    QStringList args;
    if (!vol)
        args << "-T";
    args << blockDevice;
    QProcess::startDetached( path, args );

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

void
BE::Device::updateSolidActions()
{
    static QElapsedTimer t;
    if (t.isValid() && t.elapsed() < 5000)
        return; // we don't read this over and over again... nothing changed in that 5 secs

    qDeleteAll(ourSolidActions);
    ourSolidActions.clear();

    QStringList serviceFiles = KGlobal::dirs()->findAllResources("data", "solid/actions/");
    foreach (const QString &serviceFile, serviceFiles) {
        KDesktopFile service(serviceFile);
        QStringList actions = service.desktopGroup().readEntry("Actions", QString()).split(';', QString::SkipEmptyParts);
        if (actions.isEmpty())
            continue;
        Solid::Predicate predicate = Solid::Predicate::fromString(service.desktopGroup().readEntry("X-KDE-Solid-Predicate"));
        foreach (const QString &action, actions) {
            const KConfigGroup group(&service, "Desktop Action " + action.trimmed());
            QAction *act = new QAction(BE::Plugged::themeIcon(group.readEntry("Icon")), group.readEntry("Name"), 0);
            act->setToolTip(group.readEntry("Exec"));
            act->setData(QVariant::fromValue(predicate));
            ourSolidActions << act;
        }
    }
    // add a dummy, tagging for sure that this list has been set
    QAction *act = new QAction("", 0);
    act->setData(QVariant::fromValue(Solid::Predicate()));
    ourSolidActions << act;
    t.start();
}

// =======================================================================

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


void
BE::MediaTray::mousePressEvent(QMouseEvent *ev)
{
    if( ev->button() == Qt::RightButton )
    {
        BE::Shell::run("kcmshell4 solid-actions");
        ev->accept();
    }
    else
        QFrame::mousePressEvent(ev);
}

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
    ourEjectCommand = grp->readEntry("Eject", "eject");
    ourSolidActionsAreDirty = true;
    QMetaObject::invokeMethod(this, "updateSolidActions", Qt::QueuedConnection);
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
BE::MediaTray::saveSettings(KConfigGroup *)
{
}

void
BE::MediaTray::themeChanged()
{
    QList<BE::Device*> devices = findChildren<BE::Device*>();
    foreach (BE::Device *device, devices)
        device->setIcon( themeIcon(device->iconString()) );
}

void
BE::MediaTray::updateSolidActions()
{
    if (ourSolidActionsAreDirty) {
        BE::Device::updateSolidActions();
        ourSolidActionsAreDirty = false;
    }
}
