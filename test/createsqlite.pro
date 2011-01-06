INCLUDEPATH += ..
LIBS += -L.. -lsolver

CONFIG += link_pkgconfig
PKGCONFIG += sqlite3 raptor

HEADERS += ../model.h

SOURCES += \
    createsqlite.c \
    ../model.c
