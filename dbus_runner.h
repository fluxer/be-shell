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

#ifndef RUNNER_ADAPTOR_H
#define RUNNER_ADAPTOR_H

#include <QtDBus/QDBusAbstractAdaptor>
#include "runner.h"

namespace BE
{
class RunnerAdaptor : public QDBusAbstractAdaptor
{
   Q_OBJECT
   Q_CLASSINFO("D-Bus Interface", "org.kde.be.shell")

private:
    BE::Run *myRunner;

public:
    RunnerAdaptor(BE::Run *runner) : QDBusAbstractAdaptor(runner), myRunner(runner) { }

public slots:
    void showAsDialog() { myRunner->showAsDialog(); }
    void toggleDialog() { myRunner->isVisible() ? myRunner->hide() : myRunner->showAsDialog(); }
    void togglePopup(int x, int y) { myRunner->togglePopup(x,y); }
};
}
#endif //BLAZER_ADAPTOR_H
