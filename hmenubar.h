#ifndef HMENUBAR_H
#define HMENUBAR_H

#include <QMenuBar>
namespace BE {
class HMenuBar : public QMenuBar
{
    Q_OBJECT
public:
    HMenuBar ( const QString &service = QString(), qlonglong key = 0, QWidget *parent = 0);
#include "menubar.h"
};
}

#endif
