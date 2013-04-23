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
#include <QLinearGradient>
#include <QList>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPointer>
#include <QPropertyAnimation>
#include <QStyle>
#include <QTimer>
#include <QVarLengthArray>

#include <KDE/KSharedConfig>
#include <KDE/KConfigGroup>
#include <KDE/KLocale>
#include <KDE/KWindowSystem>

#include <QtDebug>

#include <X11/Xlib.h>
#include <QX11Info>
#include "fixx11h.h"

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
//         KWindowSystem::setType( winId(), NET::Dock );
        setAttribute(Qt::WA_X11NetWmWindowTypeDock, true);
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
        // NOTICE WORKAROUND
        // sttuts are to be passed in logical screen dimensions, but all WMs -including KWin- have
        // invocation of displaySize() "somewhere" (Client::strutRect()) which need temporarily
        // compensation by one more "bug" (in be.shell) by "fixing" bottom strut widths.
        // r[BE::Panel::Bottom].height() -> QApplication::desktop()->height() - r[BE::Panel::Bottom].top()
        const int bottomWidth = r[BE::Panel::Bottom].height() ? QApplication::desktop()->height() - r[BE::Panel::Bottom].top() : 0;
        KWindowSystem::setExtendedStrut(winId(), r[BE::Panel::Left].width(), r[BE::Panel::Left].top(), r[BE::Panel::Left].bottom(),
                                                 r[BE::Panel::Right].width(), r[BE::Panel::Right].top(), r[BE::Panel::Right].bottom(),
                                                 r[BE::Panel::Top].height(), r[BE::Panel::Top].left(), r[BE::Panel::Top].right(),
                                                 bottomWidth, r[BE::Panel::Bottom].left(), r[BE::Panel::Bottom].right());
//         KWindowSystem::setStrut( winId(), r[BE::Panel::Left].width(), r[BE::Panel::Right].width(), r[BE::Panel::Top].height(), r[BE::Panel::Bottom].height());
    }
private:
    typedef QList< QPointer<BE::Panel> > PanelList;
    PanelList panels;
};

QMap<int, StrutManager*> gs_struts;
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

class PanelProxy : public QWidget {
public:
    PanelProxy(QWidget *panel) : QWidget(panel->window(),
                                         panel->windowFlags() & Qt::Window ?
                                         Qt::Window|Qt::X11BypassWindowManagerHint : Qt::Widget)
                                , myPanel(panel) {
        setAttribute(Qt::WA_TranslucentBackground, windowFlags() & Qt::Window);
        setAttribute(Qt::WA_X11NetWmWindowTypeDock, windowFlags() & Qt::Window);
        setAttribute(Qt::WA_NoSystemBackground, true);
        installEventFilter(panel);
        ++s_numProxies;
        if (!s_raiseTriggerTimer) {
            s_raiseTriggerTimer = new QTimer;
            s_raiseTriggerTimer->start(30000); // every 30 secs we conditionally raise the proxy
        }
        connect (s_raiseTriggerTimer, SIGNAL(timeout()), panel, SLOT(raiseProxy()));
    }
    ~PanelProxy() {
        --s_numProxies;
        Q_ASSERT(s_numProxies > -1);
        if (!s_numProxies) {
            delete s_raiseTriggerTimer;
            s_raiseTriggerTimer = 0;
        }
    }
protected:
    bool eventFilter(QObject *o, QEvent *e)
    {
        if (o != myPanel) {
            o->removeEventFilter(this);
            return false;
        }
        switch (e->type()) {
            case QEvent::Show:
                hide();
                break;
            default:
                break;
        }
        return false;
    }
private:
    static int s_numProxies;
    static QTimer *s_raiseTriggerTimer;
    QWidget *myPanel;
};

int PanelProxy::s_numProxies = 0;
QTimer *PanelProxy::s_raiseTriggerTimer = 0;

}

BE::Panel::Panel( QWidget *parent ) : QFrame( parent, Qt::FramelessWindowHint)
, BE::Plugged(parent)
, myPosition(Top)
, mySize(26)
, myLength(100)
, myOffset(0)
, myBlurRadius(0)
, myLayer(0)
, myScreen(-1)
, myBgPix(0)
, myMoveResizeMode(Qt::BlankCursor)
, iStrut(false)
, myProxy(0)
, myShadowRadius(0)
, myShadowPadding(0)
, myAutoHideTimer(0)
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
    connect( shell(), SIGNAL(styleSheetChanged()), SLOT(themeUpdated()) );
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

void
BE::Panel::configure( KConfigGroup *grp )
{
    myConfigMenuEntry->setTitle(name());

    bool updateGeometry = false;
    const QString oldId = myForcedId;
    myForcedId = grp->readEntry("Id", QString());
    if (!iAmNested)
    {
        int layer = grp->readEntry("Layer", 0);
        if (layer != myLayer) {
            delete myProxy; myProxy = 0;
            delete myAutoHideTimer; myAutoHideTimer = 0;
        }
        const bool isWindow(layer & 1);
        if (isWindow)
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
        if (layer > 1)
        {
            if (BE::Shell::touchMode())
                setWindowFlags( Qt::Popup );
            if (!myProxy)
                myProxy = new PanelProxy(this);
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

    bool castedShadow = iCastAShadow;
    iCastAShadow = grp->readEntry("CastShadow", true);

    if ((castedShadow != iCastAShadow || wasVis || updateGeometry) && parentWidget())
        parentWidget()->update(); // necessary?

    if (oldId != myForcedId) {
        style()->unpolish(this);
        style()->polish(this);
    }

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
    if (myLayer > 1)
        hide();
    updateSlideHint();
}

void
BE::Panel::updateSlideHint() {
    static Atom atom = 0;
    if (!atom)
        atom = XInternAtom(QX11Info::display(), "_KDE_SLIDE", false);

    if (myLayer == 3) { // autohiding real window
        QVarLengthArray<long, 4> data(4);
        data[0] = 0;
        data[2] = 300;
        data[3] = 300;
        switch (myPosition) {
            case Left: data[1] = 0; break;
            default:
            case Top: data[1] = 1; break;
            case Bottom: data[1] = 3; break;
            case Right: data[1] = 2; break;
        }
        XChangeProperty(QX11Info::display(), winId(), atom, atom, 32, PropModeReplace,
                        reinterpret_cast<unsigned char *>(data.data()), data.size());
    } else if (testAttribute(Qt::WA_WState_Created) && internalWinId())
        XDeleteProperty(QX11Info::display(), winId(), atom);
}

void
BE::Panel::updateParent()
{
    if (parentWidget())
        parentWidget()->update(geometry().adjusted(-48,-48,48,48));
}

static QPoint startPos;
void
BE::Panel::mousePressEvent(QMouseEvent *me)
{
    if (iAmNested) {
        me->ignore();
        return;
    }
    if (myMoveResizeMode == Qt::BlankCursor && BE::Shell::touchMode())
    {   // just a regular slide - let's see whether the user pushes the panel back
        if (me->button() == Qt::LeftButton)
            startPos = me->pos();
    }
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
    else if ( me->button() == Qt::RightButton )
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

void
BE::Panel::slide(bool in)
{
    if ((myLayer & 1)) {
        in ? show() : hide();
        return; // that's a real window - we use the WM sliding.
    }

    QRect geo(geometry());
    switch (myPosition)
    {
        default:
        case Top:
            move(geo.x(), geo.top() - geo.height() + 1);
            break;
        case Bottom:
            move(geo.x(), geo.bottom() -  1);
            break;
        case Left:
            move(geo.left() - geo.width() + 1, geo.y());
            break;
        case Right:
            move(geo.right() - 1, geo.y());
            break;
    }
    QPropertyAnimation *animation = new QPropertyAnimation(this, "geometry");
    animation->setEasingCurve(QEasingCurve::InOutCubic);
    animation->setDuration(200);
    if (in) {
        show();
        connect (animation, SIGNAL(finished()), SLOT(updateEffectBg()));
        animation->setStartValue(geometry());
        animation->setEndValue(geo);
    }
    else {
        connect (animation, SIGNAL(finished()), SLOT(hide()));
        animation->setStartValue(geo);
        animation->setEndValue(geometry());
    }
    connect (animation, SIGNAL(valueChanged(QVariant)), SLOT(updateParent()));
    connect (animation, SIGNAL(finished()), animation, SLOT(deleteLater()));
    animation->start();
}

void
BE::Panel::enterEvent(QEvent *e)
{
    if (myAutoHideTimer && myAutoHideTime.elapsed() > 100) {
        myAutoHideTimer->stop();
    }
    QWidget::enterEvent(e);
}

bool
BE::Panel::eventFilter(QObject *o, QEvent *e)
{
    if (o == myProxy && e->type() == QEvent::Enter)
    {
        window()->setUpdatesEnabled(false);
        if (BE::Shell::touchMode()) {
            setEnabled(false);
            slide(true);
            QTimer::singleShot(250, this, SLOT(enable()));
        }
        else
            slide(true);
        window()->setUpdatesEnabled(true);
    }
    return false;
}

void
BE::Panel::hideEvent( QHideEvent *event )
{
    int elapsed = myAutoHideTime.isValid() ? myAutoHideTime.elapsed() : 801;
    if (myLayer > 1 && BE::Shell::touchMode() && elapsed < 800) {
//         int layer = myLayer;
//         myLayer = 0; // to avoid timer restart
//         show();
//         myLayer = layer;
        QMetaObject::invokeMethod(this, "show", Qt::QueuedConnection);
        return;
    }
    QFrame::hideEvent(event);
    if (!BE::Shell::desktopGeometry(myScreen).contains(geometry())) // internal slide polluted geometry
        desktopResized(); // fixed
    if (!myProxy)
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
    QTimer::singleShot(250, this, SLOT(raiseProxy()));
}

void
BE::Panel::leaveEvent(QEvent *e)
{
    QFrame::leaveEvent(e);
    if (myLayer > 1 && !BE::Shell::touchMode()) {
        if (!myAutoHideTimer) {
            myAutoHideTimer = new QTimer(this);
            myAutoHideTimer->setSingleShot(true);
            connect (myAutoHideTimer, SIGNAL(timeout()), SLOT(conditionalHide()));
        }
        myAutoHideTime.start();
        myAutoHideTimer->start(myAutoHideDelay);
    }
}

void
BE::Panel::showEvent(QShowEvent *e)
{
    if (myAutoHideTimer) {
        myAutoHideTimer->stop();
    }
    QWidget::showEvent(e);
    updateEffectBg();
    QSize maxSize = (orientation() == Qt::Horizontal) ? QSize(QWIDGETSIZE_MAX, mySize) : QSize(mySize, QWIDGETSIZE_MAX);
    QList<QWidget*> kids;
    findSameWindowKids(this, kids);
    foreach (QWidget *kid, kids)
        kid->setMaximumSize(maxSize);
    emit orientationChanged(orientation());
    if (BE::Shell::touchMode() && myLayer > 1) {
        if (!myAutoHideTimer) {
            myAutoHideTimer = new QTimer(this);
            myAutoHideTimer->setSingleShot(true);
            connect (myAutoHideTimer, SIGNAL(timeout()), SLOT(conditionalHide()));
        }
        myAutoHideTime.start();
        myAutoHideTimer->start(8000); // hide after 8 inactive seconds
    }
}

void
BE::Panel::mouseMoveEvent(QMouseEvent *me)
{
    if (iAmNested) {
        me->ignore();
        return;
    }

    if (myMoveResizeMode == Qt::BlankCursor) {
        if (BE::Shell::touchMode()) {
            // let's see whether the user pushes the panel back
            bool doHide = false;
            switch (myPosition)
            {
            default:
            case Top:
                if (startPos.y() - me->y() > height()/2 && qAbs(startPos.x() - me->x()) < 64)
                    doHide = true; break;
            case Bottom:
                if (me->y() - startPos.y() > height()/2 && qAbs(startPos.x() - me->x()) < 64)
                    doHide = true; break;
            case Left:
                if (startPos.x() - me->x() > width()/2 && qAbs(startPos.y() - me->y()) < 64)
                    doHide = true; break;
            case Right:
                if (me->x() - startPos.x() > width()/2 && qAbs(startPos.y() - me->y()) < 64)
                    doHide = true; break;
            }
            if (doHide) {
                foreach (QWidget *window, QApplication::topLevelWidgets()) {
                    if (window != myProxy && window->parentWidget() &&
                        window->parentWidget()->window() == this)
                        window->hide();
                }
                slide(false);
                return;
            }
        }
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
            updateParent();
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
            updateParent();
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
                if (o != orientation()) {
                    updateSlideHint();
                    emit orientationChanged(orientation());
                }
                if (parentWidget())
                    parentWidget()->update(); // needs full repaint, pot. large change
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
    return gs_struts.value(qMax(0,myScreen), 0);
}

void BE::Panel::themeChanged()
{
    foreach (Plugged *p, myPlugs)
        BE::Shell::callThemeChangeEventFor(p);
}


void BE::Panel::themeUpdated()
{
    int oldShadowPadding = myShadowPadding;
    myShadowPadding = BE::Shell::shadowPadding(objectName());
    myShadowRadius = BE::Shell::shadowRadius(objectName());
    myShadowBorder = BE::Shell::shadowBorder(objectName());
    if (oldShadowPadding != myShadowPadding)
        updateEffectBg();
}

void BE::Panel::raiseProxy()
{
    if (isVisible())
        return; // no
    if (!myProxy) // should not happen
        return; // no
    myProxy->show();
    myProxy->raise();
}

void
BE::Panel::registerStrut()
{
    StrutManager *s = strut();
    if (!s) {
        s = new StrutManager;
        QRect screen = BE::Shell::desktopGeometry(myScreen);
        s->setGeometry(screen.x(), screen.y(), 1, 1);
        gs_struts.insert( qMax(0,myScreen), s );
    }
    s->registrate(this);
}

void
BE::Panel::unregisterStrut()
{
    StrutManager *s = strut();
    if (s) {
        s->unregistrate(this);
        if (s->isEmpty()) {
            gs_struts.remove(qMax(0,myScreen));
            delete s;
        }
    }
}

const char *names[4] = { "Top", "Bottom", "Left", "Right" };

void
BE::Panel::updateName()
{
    if (iAmNested)
        return;
    if (!myForcedId.isEmpty()) {
        setObjectName(myForcedId);
        myShadowPadding = BE::Shell::shadowPadding(myForcedId);
        myShadowRadius = BE::Shell::shadowRadius(myForcedId);
        myShadowBorder = BE::Shell::shadowBorder(myForcedId);
        return;
    }
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
    const QString formerName = objectName();
    setObjectName(name);
    myShadowPadding = BE::Shell::shadowPadding(name);
    myShadowRadius = BE::Shell::shadowRadius(name);
    myShadowBorder = BE::Shell::shadowBorder(name);
    if (formerName != name) {
        style()->unpolish(this);
        style()->polish(this);
    }
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
    bool haveMouse = BE::Shell::touchMode() ? false : underMouse(); // for the touch thing this is a popup and grabs the pointer
    if (!haveMouse)
    {
        foreach (const QWidget *window, QApplication::topLevelWidgets()) {
            haveMouse = window->isVisible() && window->parentWidget() &&
                        window->parentWidget()->window() == this &&
                        window->geometry().contains(QCursor::pos());
            if (haveMouse) {
                break;
            }
        }
    }
    if (!haveMouse)
    {
        foreach (QWidget *window, QApplication::topLevelWidgets())
        {
            if (window != myProxy && window->parentWidget() && window->parentWidget()->window() == this)
                window->hide();
        }
        slide(false);
    } else if (BE::Shell::touchMode()) {
        myAutoHideTimer->start(8000);
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
    if (!isVisible() && myLayer > 1)
    {   // to reposition the proxy
        const int layer = myLayer; // for layer == 2, this causes a recursion, because the hideevent calls desktop resize
        myLayer = 0;
        QHideEvent he;
        hideEvent(&he);
        myLayer = layer;
    }
    if (parentWidget())
        parentWidget()->update(); // needs full repaint, pot. large update
}

void
BE::Panel::updateEffectBg()
{
    delete myBgPix; myBgPix = 0; // ensure the desktop will not paint our effect background
    if (!parentWidget())
        return;

    int x(0),y(0),w(0),h(0);
    QRect prect = geometry();
    BE::Shell::getContentsMargins(this, &x, &y, &w, &h);
    const int v = shadowPadding();
    int d[4] = { (v&0xff) - 128, ((v >> 8) & 0xff) - 128,
                ((v >> 16) & 0xff) - 128, ((v >> 24) & 0xff) - 128 };
    if (d[1] == d[2] && d[2] == d[3] && d[3] == 0xff)
        d[1] = d[2] = d[3] = d[0];
    prect.adjust(x-d[3],y-d[0],d[1]-w,d[2]-h);
    const QRect desktop = BE::Shell::desktopGeometry(myScreen);

    QPainterPath path;
    if (myBlurRadius) {
        path.addRoundedRect(prect, myShadowRadius, myShadowRadius);
        QPainterPath p2; p2.addRect(desktop);
        path = path.intersected(p2);
    }

    prect &= desktop;

    if (myBlurRadius)
        path.translate(-prect.topLeft());

    if (myBlurRadius)
    {
        QImage *img = new QImage(prect.size(), QImage::Format_ARGB32);
        int sr = myShadowRadius;
        myShadowRadius = -1; // ensure the desktop will not paint our shadow now
        parentWidget()->render(img, QPoint(), prect, DrawWindowBackground);
        myShadowRadius = sr;
        QImage *blurCp = new QImage(*img);
        BE::Shell::blur( *blurCp, myBlurRadius );
        QPainter p(img);
        p.setPen( Qt::NoPen );
        p.setBrush( Qt::white );
        p.setRenderHint( QPainter::Antialiasing );
        p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
        p.drawPath(path);
        p.end();
        p.begin(blurCp);
        p.drawImage(0,0,*img);
        p.end();
        delete img;
        blurCp->convertToFormat(QImage::Format_RGB32);
        myBgPix = new QPixmap(QPixmap::fromImage(*blurCp));
        delete blurCp;
        parentWidget()->update(prect);
    }
    parentWidget()->update(prect);
}
