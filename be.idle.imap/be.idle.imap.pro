TARGET = be.idle.imap
HEADERS = be.idle.imap.h
SOURCES = be.idle.imap.cpp
CONFIG += qt
QT += core network
DEFINES += VERSION=0.1
target.path += $$[QT_INSTALL_BINS]
INSTALLS += target
