
HEADERS += \
    defs.h \
    solver.h \
    constraints.h \
    store.h \
    model.h

SOURCES += \
    castor.c \
    solver.c \
    constraints.c

TARGET = castor

CONFIG += link_pkgconfig
PKGCONFIG += raptor rasqal
