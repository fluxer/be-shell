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

#include "panel.h"
#include "be.shell.h"


#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QDesktopWidget>
#include <QElapsedTimer>
#include <QLinearGradient>
#include <QList>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPointer>
#include <QTimer>
#include <QX11Info>

#include <KDE/KSharedConfig>
#include <KDE/KConfigGroup>
#include <KDE/KLocale>
#include <KDE/KWindowSystem>

#include <QtDebug>

namespace BE {
class Stretch : public QWidget, public BE::Plugged {
public:
    Stretch(QWidget *parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_NoSystemBackground);
    }
};

class StrutManager : public QWidget
{
public:
    StrutManager() : QWidget( 0, Qt::Window | Qt::FramelessWindowHint )
    {
        KWindowSystem::setType( winId(), NET::Dock );
        KWindowSystem::setOnAllDesktops( winId(), true );
        setMask( QRegion(-1,-1,1,1) );
        show();
    }
    void registrate(BE::Panel *panel)
    {
        if (!panel || panels.contains(panel))
            return;

        panel->installEventFilter(this);
        panels.append(panel);
        updateStruts();
    }
    void unregistrate(BE::Panel *panel)
    {
        if (panel)
        {
            panel->removeEventFilter(this);
            panels.removeOne(panel);
        }
        updateStruts();
    }
protected:
    bool eventFilter(QObject *o, QEvent *e)
    {
        switch (e->type())
        {
        case QEvent::Move:
        case QEvent::Resize:
        case QEvent::Show:
        case QEvent::Hide:
            if (qobject_cast<BE::Panel*>(o))
                updateStruts();
            break;
        default:
            break;
        }
        return false;
    }
public:
    bool isEmpty() { return panels.isEmpty(); }
    void updateStruts()
    {
        QRect r[4];
//         enum Position { Top = 0, Bottom, Left, Right }
        PanelList::iterator i = panels.begin();
        while (i != panels.end())
        {
            BE::Panel *p = *i;
            if (!p)
                { i = panels.erase(i); continue; }
            ++i;

            if (p->layer() > 1 || !p->isVisible())
                continue;

            r[p->position()] |= p->rect().translated(p->mapToGlobal(QPoint(0,0)));
        }
        KWindowSystem::setExtendedStrut(winId(), r[BE::Panel::Left].width(), r[BE::Panel::Left].top(), r[BE::Panel::Left].bottom(),
                                                 r[BE::Panel::Right].width(), r[BE::Panel::Right].top(), r[BE::Panel::Right].bottom(),
                                                 r[BE::Panel::Top].height(), r[BE::Panel::Top].left(), r[BE::Panel::Top].right(),
                                                 r[BE::Panel::Bottom].height(), r[BE::Panel::Bottom].left(), r[BE::Panel::Bottom].right());
//         KWindowSystem::setStrut( winId(), r[BE::Panel::Left].width(), r[BE::Panel::Right].width(), r[BE::Panel::Top].height(), r[BE::Panel::Bottom].height());
    }
private:
    typedef QList< QPointer<BE::Panel> > PanelList;
    PanelList panels;
};

QMap<int, StrutManager*> struts;
static QMenu *configSubMenu = 0;

static void findSameWindowKids( QWidget *root, QList<QWidget*> &list )
{
    foreach (QObject *kid, root->children())
        if (QWidget *w = qobject_cast<QWidget*>(kid))
        if (!w->isWindow())
        {
            list << w;
            findSameWindowKids( w, list );
        }
}

}

BE::Panel::Panel( QWidget *parent ) : QFrame( parent, Qt::FramelessWindowHint), BE::Plugged(parent), myPosition(Top),
                                      mySize(26), myLength(100), myOffset(0), myBlurRadius(0), myLayer(0), myScreen(-1),
                                      myBgPix(0), myMoveResizeMode(Qt::BlankCursor), iStrut(false), myProxy(0)
{
    QBoxLayout *layout = new QBoxLayout(QBoxLayout::LeftToRight, this);
    layout->setSpacing(0);
    layout->setMargin(0);
    layout->setAlignment(Qt::AlignCenter);

    iAmNested = bool(dynamic_cast<Panel*>(parent));

    if (!configSubMenu)
        configSubMenu = ((QMenu*)configMenu())->addMenu("Panels");
    myConfigMenuEntry = configSubMenu->addMenu(name());
    myVisibility = myConfigMenuEntry->addAction( i18n("Visible"), this, SLOT(setAndSaveVisible(bool)) );
    myVisibility->setCheckable(true);
    if (!iAmNested)
    {
        myConfigMenuEntry->addAction( i18n("Move & Resize"), this, SLOT(startMoveResize()) );
        myConfigMenuEntry->addMenu( BE::Shell::screenMenu() );
        connect (myConfigMenuEntry, SIGNAL(aboutToShow()), SLOT(bindScreenMenu()));
        desktopResized();
        updateEffectBg();
        if (parent)
            connect (parent, SIGNAL(wallpaperChanged()), this, SLOT(updateEffectBg()));
    }
    setFocusPolicy(Qt::NoFocus);
    connect( shell(), SIGNAL(desktopResized()), SLOT(desktopResized()) );
    connect( QApplication::desktop(), SIGNAL(screenCountChanged(int)), SLOT(desktopResized()) );
}

BE::Panel::~Panel()
{
    const QAction *screenAction = BE::Shell::screenMenu()->menuAction();
    unregisterStrut();
    foreach (QAction *act, myConfigMenuEntry->actions())
        if (act != screenAction) // not ours exclusively
            delete act;
    delete myConfigMenuEntry;
}

BE::Plugged*
BE::Panel::addStretch(int s)
{
    BE::Stretch *stretch = new BE::Stretch(this);
    static_cast<QBoxLayout*>(layout())->addWidget(stretch, s);
    myPlugs << stretch;
    return stretch;
}

void
BE::Panel::childEvent( QChildEvent *ce )
{
    if ( !ce->child()->isWidgetType() )
        return;

    QWidget *w = static_cast<QWidget*>(ce->child());

    if (ce->added())
    {
        w->setMaximumSize((orientation() == Qt::Horizontal) ? QSize(QWIDGETSIZE_MAX, mySize) : QSize(mySize, QWIDGETSIZE_MAX));
        static_cast<QBoxLayout*>(layout())->addWidget(w, 0, Qt::AlignCenter);
    }
    else if (ce->removed())
        layout()->removeWidget(w);

    QWidget::childEvent(ce);
}

const char *names[4] = { "Top", "Bottom", "Left", "Right" };

void
BE::Panel::configure( KConfigGroup *grp )
{
    myConfigMenuEntry->setTitle(name());

    bool updateGeometry = false;
    if (!iAmNested)
    {
        int layer = grp->readEntry("Layer", 0);
        if (layer > 0)
        {
            setWindowFlags( Qt::Window );
            setAttribute(Qt::WA_TranslucentBackground, grp->readEntry("ARGB", true));
            setAttribute(Qt::WA_X11NetWmWindowTypeDock, true);
            KWindowSystem::setOnAllDesktops( winId(), true );
        }
        else
        {
            setWindowFlags( Qt::Widget );
            setAttribute(Qt::WA_TranslucentBackground, false);
            setAttribute(Qt::WA_X11NetWmWindowTypeDock, false);
        }
        if (layer > 2)
        {
            if (BE::Shell::touchMode())
                setWindowFlags( Qt::Popup );
            if (!myProxy)
                myProxy = new QWidget(this, Qt::Window|Qt::X11BypassWindowManagerHint);
            myProxy->setAttribute(Qt::WA_TranslucentBackground, true);
            myProxy->setAttribute(Qt::WA_X11NetWmWindowTypeDock, true);
            myProxy->installEventFilter(this);
            myProxyThickness = grp->readEntry("AutoHideSensorSize", BE::Shell::touchMode() ? 2 : 1);
            myAutoHideDelay = grp->readEntry("AutoHideDelay", 2000);
        }
        else
        {
            delete myProxy; myProxy = 0;
        }
        if (bool(layer) != bool(myLayer))
            updateGeometry = true;
        myLayer = layer;

        int old = myPosition; Qt::Orientation o = orientation();
        myPosition = (BE::Panel::Position)grp->readEntry("Position", (int)Top);
        qobject_cast<QBoxLayout*>(layout())->setDirection(orientation() == Qt::Horizontal ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom );
        if (o != orientation())
            emit orientationChanged(orientation());
        if (old != myPosition) updateGeometry = true;

        old = myBlurRadius;
        myBlurRadius = grp->readEntry("BlurRadius", QVariant(6) ).toInt();
        if (old != myBlurRadius) updateGeometry = true;

        old = mySize;
        mySize = grp->readEntry("Size", QVariant(26) ).toInt();
        if (old != mySize) updateGeometry = true;

        old = myLength;
        myLength = grp->readEntry("Length", QVariant(100) ).toInt();
        if (old != myLength) updateGeometry = true;

        old = myOffset;
        myOffset = grp->readEntry("Offset", QVariant(0) ).toInt();
        if (old != myOffset) updateGeometry = true;

        old = myScreen;
        myScreen = grp->readEntry("Screen", QVariant(-1) ).toInt();
        if (old != myScreen) updateGeometry = true;

        bool iDidStrut = iStrut;
        iStrut = myLayer < 2 && grp->readEntry("Struts", QVariant(true) ).toBool();
        if ( iDidStrut != iStrut )
        {
            if (iStrut)
                registerStrut();
            else
                unregisterStrut();
        }

        updateName();
        if (updateGeometry)
            desktopResized();
    } else {
        /*int old = myPosition; */Qt::Orientation o = orientation();
        myPosition = (BE::Panel::Position)grp->readEntry("Position", (int)Top);
        qobject_cast<QBoxLayout*>(layout())->setDirection(orientation() == Qt::Horizontal ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom );
        if (o != orientation())
            emit orientationChanged(orientation());
    }
    bool wasVis = isVisibleTo(parentWidget());
    setVisible(grp->readEntry("Visible", QVariant(true) ).toBool());
    if ((wasVis || updateGeometry) && parentWidget())
        parentWidget()->update(); // necessary?

    myVisibility->setChecked(isVisibleTo(parentWidget()));

    // load applets
    QStringList applets = grp->readEntry("Applets", QStringList());
    QList<BE::Plugged*> newPlugs, plugs = myPlugs;
    QList<BE::Plugged*>::iterator i;
    bool needNew;
    foreach (QString applet, applets)
    {
        needNew = true;
        for (i = plugs.begin(); i != plugs.end(); ++i)
        {
            if (!applet.compare((*i)->name(), Qt::CaseSensitive))
            {
                needNew = false;
                newPlugs << *i;
                (*i)->configure();
                plugs.erase(i);
                break;
            }
        }
        if (needNew)
        if (Plugged *p = BE::Shell::addApplet(applet, this))
            newPlugs << p;
    }
    while (!plugs.isEmpty())
        delete plugs.takeFirst();

    myPlugs = newPlugs;
    updatePlugOrder();
    if (myLayer > 2)
        hide();
}

static QPoint startPos;

void
BE::Panel::mousePressEvent(QMouseEvent *me)
{
    if (iAmNested)
        return;
    if (myMoveResizeMode == Qt::ArrowCursor)
    {
        releaseMouse();
        myMoveResizeMode = Qt::BlankCursor;
        setMouseTracking(false);
        updateName();
        updateEffectBg();
        Plugged::saveSettings();
    }
    else if (myMoveResizeMode != Qt::BlankCursor)
        startPos = me->pos();
    else if( me->button() == Qt::RightButton )
        myConfigMenuEntry->exec(QCursor::pos());
    else
        QFrame::mousePressEvent(me);
    me->accept();
}

void
BE::Panel::mouseReleaseEvent(QMouseEvent *me)
{
    if (isWindow() && parentWidget())
        QCoreApplication::sendEvent(parentWidget(), me);
    else
        QFrame::mouseReleaseEvent(me);
}

void
BE::Panel::paintEvent(QPaintEvent *pe)
{
    if (testAttribute(Qt::WA_TranslucentBackground))
    {
        QColor c = palette().color(foregroundRole());
        int v = qMin(qMin(c.red(), c.green()), c.blue());
        if (v > 160)
            c = Qt::black;
        else
            c = Qt::white;
        QLinearGradient lg(0,0,0,height());
        if (BE::Shell::compositingActive())
        {
            c.setAlpha(qAbs(c.red()-85));
            lg.setColorAt(0.1, c);
            c.setAlpha(255-c.alpha());
        }
        else
        {
            v = qMin(c.red()+64,255);
            c.setRgb(v,v,v);
            lg.setColorAt(0.1, c);
            v -= 32;
            c.setRgb(v,v,v);
        }
        lg.setColorAt(0.9, c);
        QPainter p(this);
        p.setClipRegion(pe->region());
        p.setPen(Qt::NoPen);
        p.setBrush(lg);
        p.drawRect(rect());
        p.end();
    }
}

void
BE::Panel::wheelEvent(QWheelEvent *we) {
    if (isWindow() && parentWidget())
        QCoreApplication::sendEvent(parentWidget(), we);
    else
        QFrame::wheelEvent(we);
}

bool
BE::Panel::eventFilter(QObject *o, QEvent *e)
{
    if (o == myProxy && e->type() == QEvent::Enter)
    {
        myProxy->hide();
        show();
    }
    return false;
}

QElapsedTimer myShowTime;

void
BE::Panel::hideEvent( QHideEvent *event )
{
    int elapsed = myShowTime.elapsed();
    if (myLayer > 2 && BE::Shell::touchMode() && elapsed < 800) {
        int layer = myLayer;
        myLayer = 0; // to avoid timer restart
        show();
        myLayer = layer;
        return;
    }
    QFrame::hideEvent(event);
    if (myLayer < 3)
        return;
    QRect r(geometry());
    switch (myPosition)
    {
        default:
        case Top:
            r.setHeight(myProxyThickness);
            break;
        case Bottom:
            r.setHeight(myProxyThickness);
            r.moveBottom(geometry().bottom());
            break;
        case Left:
            r.setWidth(myProxyThickness);
            break;
        case Right:
            r.setWidth(myProxyThickness);
            r.moveRight(geometry().right());
            break;
    }
    myProxy->setFixedSize(r.size());
    myProxy->move(r.topLeft());
    myProxy->show();
}

void
BE::Panel::leaveEvent(QEvent *e)
{
    QFrame::leaveEvent(e);
    if (myLayer > 2 && !BE::Shell::touchMode())
        QTimer::singleShot(myAutoHideDelay, this, SLOT(conditionalHide()));
}

void
BE::Panel::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    updateEffectBg();
    QSize maxSize = (orientation() == Qt::Horizontal) ? QSize(QWIDGETSIZE_MAX, mySize) : QSize(mySize, QWIDGETSIZE_MAX);
    QList<QWidget*> kids;
    findSameWindowKids(this, kids);
    foreach (QWidget *kid, kids)
        kid->setMaximumSize(maxSize);
    emit orientationChanged(orientation());
    if (BE::Shell::touchMode() && myLayer > 2)
        myShowTime.start();
}

void
BE::Panel::mouseMoveEvent(QMouseEvent *me)
{
    if (iAmNested)
        return;

    if (myMoveResizeMode == Qt::BlankCursor)
    {
        QFrame::mouseMoveEvent(me);
        return;
    }

    if (me->buttons() == Qt::NoButton)
    {
        if (!rect().contains(me->pos()))
        {
            if (myMoveResizeMode != Qt::ArrowCursor)
                setCursor(myMoveResizeMode = Qt::ArrowCursor);
            return;
        }

        Qt::CursorShape newMode = Qt::SizeAllCursor;
        switch (myPosition)
        {
        default:
        case Top:
            if (me->y() > 2*height()/3)
                newMode = Qt::SizeVerCursor;
        case Bottom:
            if (newMode == Qt::SizeAllCursor && me->y() < height()/3)
                newMode = Qt::SizeVerCursor;
            if (me->x() < width()/4 || me->x() > 3*width()/4)
                newMode = Qt::SizeHorCursor;
            break;
        case Left:
            if (me->x() > 2*width()/3)
                newMode = Qt::SizeHorCursor;
        case Right:
            if (newMode == Qt::SizeAllCursor && me->x() < width()/3)
                newMode = Qt::SizeHorCursor;
            if (me->y() < height()/4 || me->y() > 3*height()/4)
                newMode = Qt::SizeVerCursor;
            break;
        }
        if (myMoveResizeMode != newMode)
            setCursor(myMoveResizeMode = newMode);
    }
    else if (me->buttons() == Qt::LeftButton)
    {
//         const QRect screen = QApplication::desktop()->screenGeometry(myScreen);
        const QRect screen = BE::Shell::desktopGeometry(myScreen);
        switch (myMoveResizeMode)
        {
        case Qt::SizeHorCursor:
            if (myPosition == Left)
                mySize += me->pos().x() - startPos.x();
            else if (myPosition == Right)
                mySize += startPos.x() - me->pos().x();
            else if (myPosition == Top || myPosition == Bottom)
            {
                const int oldLength = myLength;
                const int d = 100*(startPos.x() - me->pos().x())/screen.width();
                if (me->pos().x() < width()/2)
                {
                    myOffset = qMax(0, myOffset-d);
                    myLength = qMin(100-myOffset, myLength+d);
                }
                else
                    myLength = qMin(100, myLength - d);
                if (oldLength == myLength)
                    break; // avoid startPos update, this changes percentwise
            }
            startPos = me->pos();
            desktopResized();
            if (parentWidget()) parentWidget()->update();
            break;
        case Qt::SizeVerCursor:
            if (myPosition == Top)
                mySize += me->pos().y() - startPos.y();
            else if (myPosition == Bottom)
                mySize += startPos.y() - me->pos().y();
            else if (myPosition == Left || myPosition == Right)
            {
                const int oldLength = myLength;
                const int d = 100*(startPos.y() - me->pos().y())/screen.height();
                if (me->pos().y() < height()/2)
                {
                    myOffset = qMax(0, myOffset-d);
                    myLength = qMin(100-myOffset, myLength+d);
                }
                else
                    myLength = qMin(100, myLength - d);
                if (oldLength == myLength)
                    break; // avoid startPos update, this changes percentwise
            }
            startPos = me->pos();
            desktopResized();
            if (parentWidget())
                parentWidget()->update();
            break;
        case Qt::SizeAllCursor:
        {
            Qt::Orientation o = orientation();
            QBoxLayout::Direction dir; Position oldPos = myPosition;
            if (me->globalPos().x() < screen.x() + screen.width()/4)
            {
                if (myPosition != Left)
                    { myPosition = Left; dir = QBoxLayout::TopToBottom; }
            }
            else if (me->globalPos().x() > screen.x() + 3*screen.width()/4)
            {
                if (myPosition != Right)
                    { myPosition = Right; dir = QBoxLayout::TopToBottom; }
            }
            else if (me->globalPos().y() < screen.y() + screen.height()/4)
            {
                if (myPosition != Top)
                    { myPosition = Top; dir = QBoxLayout::LeftToRight; }
            }
            else if (me->globalPos().y() > screen.y() + 3*screen.height()/4)
            {
                if (myPosition != Bottom)
                    { myPosition = Bottom; dir = QBoxLayout::LeftToRight; }
            }
            if (oldPos != myPosition)
            {
                qobject_cast<QBoxLayout*>(layout())->setDirection(dir);
                desktopResized();
                if (o != orientation())
                    emit orientationChanged(orientation());
                if (parentWidget())
                    parentWidget()->update();
            }
            break;
        }
        default:
            break;
        }
    }
}

Qt::Orientation
BE::Panel::orientation() const
{
//     if (iAmNested)
//     {
//         if (Panel *parpnl = dynamic_cast<Panel*>(parentWidget()))
//             return parpnl->orientation();
//         else
//             return Qt::Horizontal;
//     }
    if (myPosition == Top || myPosition == Bottom)
        return Qt::Horizontal;
    return Qt::Vertical;
}

void
BE::Panel::saveSettings( KConfigGroup *grp )
{
    grp->writeEntry( "Visible", isVisibleTo(parentWidget()) );
    grp->writeEntry( "Size", mySize );
    grp->writeEntry( "Length", myLength );
    grp->writeEntry( "Offset", myOffset );
    grp->writeEntry( "Position", (int)myPosition );
    grp->writeEntry( "Screen", myScreen );
}

void
BE::Panel::setAndSaveVisible( bool vis )
{
    setVisible(vis);
    Plugged::saveSettings();
}

void
BE::Panel::bindScreenMenu()
{
    BE::Shell::screenMenu()->disconnect(SIGNAL(triggered(QAction*)));
    BE::Shell::screenMenu()->setProperty("CurrentScreen", myScreen);
    connect (BE::Shell::screenMenu(), SIGNAL(triggered(QAction*)), SLOT(setOnScreen(QAction*)));
}

void
BE::Panel::setOnScreen(QAction *action)
{
    BE::Shell::screenMenu()->disconnect(this);
    if (!action)
        return;
    bool ok; int screen = action->data().toInt(&ok);
    if (!ok)
        return;
    if (screen >= QApplication::desktop()->screenCount())
        return;
    const bool changeStrut = iStrut && (qMax(0,screen) != qMax(0,myScreen));
    if (changeStrut)
        unregisterStrut();
    const bool needToSave = (screen != myScreen);
    if (((screen < 0) xor (myScreen < 0)) || QApplication::desktop()->screenGeometry(screen) != geometry()) {
        myScreen = screen;
        desktopResized();
    }
    if (needToSave) {
        myScreen = screen;
        Plugged::saveSettings();
    }
    if (changeStrut)
        registerStrut();
}

void
BE::Panel::startMoveResize()
{
    myMoveResizeMode = Qt::ArrowCursor;
    setMouseTracking(true);
    grabMouse();
}

BE::StrutManager *
BE::Panel::strut() const
{
    return struts.value(qMax(0,myScreen), 0);
}

void BE::Panel::themeChanged()
{
    foreach (Plugged *p, myPlugs)
        BE::Shell::callThemeChangeEventFor(p);
}

void
BE::Panel::registerStrut()
{
    StrutManager *s = strut();
    if (!s) {
        s = new StrutManager;
        QRect screen = BE::Shell::desktopGeometry(myScreen);
        s->setGeometry(screen.x(), screen.y(), 1, 1);
        struts.insert( qMax(0,myScreen), s );
    }
    s->registrate(this);
}

void
BE::Panel::unregisterStrut()
{
    StrutManager *s = iStrut ? strut() : NULL;
    if (s) {
        s->unregistrate(this);
        if (s->isEmpty()) {
            struts.remove(qMax(0,myScreen));
            delete s;
        }
    }
}

void
BE::Panel::updateName()
{
    if (iAmNested)
        return;
    QString name = names[myPosition];
    int n = 2*(orientation() == Qt::Horizontal);
    if (myLength < 100)
    {
        if (myOffset > 0)
            name.prepend( (myOffset+myLength < 100) ? "Central" : names[n+1] );
        else
            name.prepend( names[n] );
    }
    name.append("Panel");
    setObjectName(name);
}

void
BE::Panel::updatePlugOrder()
{
    QList<QWidget*> items;
    QBoxLayout *bl = static_cast<QBoxLayout*>(layout());
    foreach (BE::Plugged *plug, myPlugs)
    {
        if (QWidget *w = dynamic_cast<QWidget*>(plug))
        {
            bl->removeWidget(w);
            items << w;
        }
    }
    foreach (QWidget *w, items)
        bl->addWidget(w, dynamic_cast<BE::Stretch*>(w) ? 2 : 0);
}

void
BE::Panel::conditionalHide()
{
    bool haveMouse = underMouse();
    if (!haveMouse)
    {
        foreach (const QWidget *window, QApplication::topLevelWidgets()) {
            haveMouse = window->isVisible() && window->parentWidget() &&
                        window->parentWidget()->window() == this &&
                        window->geometry().contains(QCursor::pos());
            if (haveMouse)
                break;
        }
    }
    if (!haveMouse)
    {
        foreach (QWidget *window, QApplication::topLevelWidgets())
        {
            if (window != myProxy && window->parentWidget() && window->parentWidget()->window() == this)
                window->hide();
        }
        hide();
    }
}

void
BE::Panel::desktopResized()
{
    if (iAmNested)
        return;

    QRect screen = BE::Shell::desktopGeometry(myScreen);
    StrutManager *s = strut();
    if (s) {
        const int oldScreen = QApplication::desktop()->screenNumber(s);
        s->move(screen.topLeft());
        if (oldScreen != QApplication::desktop()->screenNumber(s))
            s->updateStruts();
    }


    if (!myLayer && BE::Shell::screen() > -1) // is on a particular screen and reparented to desktop - fix geometry
        screen.moveTo(0,0);

    int off = 0;
    switch (myPosition)
    {
    case Bottom:
        off = screen.height() - mySize;
    default:
    case Top:
        setFixedSize( screen.width()*myLength/100, mySize );
        move(screen.x() + screen.width()*myOffset/100, off );
        break;
    case Right:
        off = screen.width() - mySize;
    case Left:
        setFixedSize( mySize, screen.height()*myLength/100 );
        move(off, screen.y() + screen.height()*myOffset/100);
        break;
    }
    if (myMoveResizeMode == Qt::BlankCursor)
        updateEffectBg();
    QSize maxSize = (orientation() == Qt::Horizontal) ? QSize(QWIDGETSIZE_MAX, mySize) : QSize(mySize, QWIDGETSIZE_MAX);
    QList<QWidget*> kids;
    findSameWindowKids(this, kids);
    foreach (QWidget *kid, kids)
        kid->setMaximumSize(maxSize);
    if (!isVisible() && myLayer > 2)
    {   // to reposition the proxy
        QHideEvent he;
        hideEvent(&he);
    }
    if (parentWidget())
        parentWidget()->update();
}

void
BE::Panel::updateEffectBg()
{
    if (myBlurRadius && parentWidget())
    {
        QImage *img = new QImage(QWidget::size(), QImage::Format_RGB32);
        delete myBgPix; myBgPix = 0;
        parentWidget()->render(img, QPoint(), geometry(), DrawWindowBackground);
        BE::Shell::blur( *img, myBlurRadius );
        myBgPix = new QPixmap(QPixmap::fromImage(*img));
        delete img;
        parentWidget()->update(geometry());
    }
    else
        { delete myBgPix; myBgPix = 0L; }
}
