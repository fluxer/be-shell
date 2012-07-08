/**************************************************************************
*   Copyright (C) 2009 by Ian Reinhart Geiser                             *
*   geiseri@kde.org                                                       *
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

#include "pager.h"

#include <KDE/KWindowSystem>

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QToolButton>

BE::Pager::Pager(QWidget *parent) : QFrame(parent), BE::Plugged(parent)
{
    setObjectName("Pager");

    myDesktops = new QButtonGroup(this);
    myLayout = new QHBoxLayout(this);
    myLayout->setSpacing(0);
    myLayout->setMargin(0);

    connect( KWindowSystem::self(), SIGNAL(currentDesktopChanged(int)), this, SLOT(currentDesktopChanged(int)) );
    connect( KWindowSystem::self(), SIGNAL(desktopNamesChanged()), this, SLOT(desktopNamesChanged()) );
    connect( KWindowSystem::self(), SIGNAL(numberOfDesktopsChanged(int)), this, SLOT(numberOfDesktopsChanged(int)) );
    connect( myDesktops, SIGNAL(buttonClicked(QAbstractButton*)), this, SLOT(setCurrentDesktop(QAbstractButton*)) );

    numberOfDesktopsChanged(KWindowSystem::numberOfDesktops());
}

void BE::Pager::currentDesktopChanged (int desktop)
{
    myDesktops->button(desktop)->setChecked(true);
}

void BE::Pager::desktopNamesChanged ()
{
    int buttonCount = myDesktops->buttons().count();
    for ( int i = 1; i <= buttonCount; ++i )
    {
        if (QAbstractButton *button = myDesktops->button(i))
            button->setToolTip(KWindowSystem::desktopName(i));
    }
}

void BE::Pager::numberOfDesktopsChanged(int desktopCount)
{
    qDeleteAll( myDesktops->buttons() );
    for ( int i = 1; i <= desktopCount; ++i )
    {
        QToolButton *button = new QToolButton(this);
        button->setShortcut(QKeySequence());
        button->setCheckable(true);
        if ( i == KWindowSystem::currentDesktop() )
            button->setChecked(true);
        button->setText( QString::number(i) );
        button->setToolTip( KWindowSystem::desktopName(i) );
        myLayout->addWidget( button );
        myDesktops->addButton( button, i );
    }

    if (desktopCount == 1)
        hide();
    else
        show();
}

void BE::Pager::setCurrentDesktop(QAbstractButton *button)
{
    KWindowSystem::setCurrentDesktop(myDesktops->id(button));
}
