

#include "touchwheel.h"
#include "be.plugged.h"

#include <QAction>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QHBoxLayout>
#include <QToolButton>

namespace BE {

static TouchWheel *s_instance = 0;

TouchWheel::TouchWheel() : QFrame() {
    if (s_instance) {
        qWarning("there's already a touchwheel");
        deleteLater();
        return;
    }

    s_instance = this;
    iCanClose = true;
    setObjectName("TouchWheel");
    setWindowFlags( Qt::Popup );
    setAttribute(Qt::WA_X11NetWmWindowTypeDock, true);
    setFocusPolicy(Qt::NoFocus);
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->addWidget(myDown = new QToolButton(this));
    myDown->setIconSize(QSize(64,64));
    myDown->setAutoRepeat(true);
    layout->addWidget(myToggle = new QToolButton(this));
    myToggle->setIconSize(QSize(64,64));
    layout->addWidget(myUp = new QToolButton(this));
    myUp->setIconSize(QSize(64,64));
    myUp->setAutoRepeat(true);
    myIconsAreDirty = true;

    connect(myDown, SIGNAL(clicked(bool)), SLOT(wheelDown()));
    connect(myUp, SIGNAL(clicked(bool)), SLOT(wheelUp()));
}

void TouchWheel::blockClose(bool b)
{
    if (s_instance)
        s_instance->iCanClose = !b;
}

bool TouchWheel::claimFor(QObject *o, const char *toggle, bool force)
{
    if (!s_instance)
        s_instance = new TouchWheel;
    else {
        if (s_instance->myClaimer.data() && !force)
            return false;
        s_instance->myToggle->disconnect(SIGNAL(clicked(bool)));
        s_instance->myIconsAreDirty = true;
    }
    s_instance->myClaimer = o;
    connect(s_instance->myToggle, SIGNAL(clicked(bool)), o, toggle);
    return true;
}

void TouchWheel::closeEvent(QCloseEvent *ce)
{
    if (!iCanClose) {
        ce->ignore();
        return;
    }
    myClaimer.clear();
    QFrame::closeEvent(ce);
}

void TouchWheel::setIcons(QString down, QString toggle, QString up)
{
    if (!s_instance)
        s_instance = new TouchWheel;
    s_instance->myIconsAreDirty = false;
    s_instance->myDown->setIcon(Plugged::themeIcon(down, static_cast<QWidget*>(NULL)));
    if (toggle.isEmpty())
        s_instance->myToggle->hide();
    else {
        s_instance->myToggle->setIcon(Plugged::themeIcon(toggle, static_cast<QWidget*>(NULL)));
        s_instance->myToggle->show();
    }
    s_instance->myUp->setIcon(Plugged::themeIcon(up, static_cast<QWidget*>(NULL)));
    s_instance->adjustSize();
}

void TouchWheel::show(QPoint pos)
{
    if (!s_instance)
        s_instance = new TouchWheel;
    if (s_instance->myIconsAreDirty)
        setIcons("media-seek-backward", QString(), "media-seek-foreward");
    if (pos.isNull())
        pos = QCursor::pos();
    s_instance->move(pos);
    s_instance->QFrame::show();
}

QSize TouchWheel::size()
{
    if (s_instance)
        return s_instance->QFrame::size();
    return QSize();
}

void
TouchWheel::wheelDown() {
    QObject *receiver = myClaimer.data();
    if (!receiver)
        return;
    QWheelEvent we(QPoint(), -120, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(receiver, &we);
}

void
TouchWheel::wheelUp() {
    QObject *receiver = myClaimer.data();
    if (!receiver)
        return;
    QWheelEvent we(QPoint(), 120, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(receiver, &we);
}

} // namespace