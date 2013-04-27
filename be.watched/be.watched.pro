TARGET = be.watched
HEADERS = be.watched.h
SOURCES = be.watched.cpp
CONFIG += qt
QT += core dbus
DEFINES += VERSION=0.1
target.path += $$[QT_INSTALL_BINS]
INSTALLS += target
