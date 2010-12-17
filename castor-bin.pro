
HEADERS += \
    defs.h \
    solver.h \
    constraints.h

SOURCES += \
    castor.c \
    solver.c \
    constraints.c

TARGET = castor

CONFIG += link_pkgconfig
PKGCONFIG += monetdb-mapi raptor rasqal
