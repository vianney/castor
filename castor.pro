CONFIG -= QT
LIBS += -L. -lsolver

CONFIG += link_pkgconfig
PKGCONFIG += raptor rasqal

HEADERS += \
    defs.h \
    constraints.h \
    store.h \
    model.h

SOURCES += \
    castor.c \
    constraints.c
