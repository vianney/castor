CONFIG -= QT
LIBS += -L. -lsolver

CONFIG += link_pkgconfig
PKGCONFIG += raptor rasqal
PKGCONFIG += sqlite3

HEADERS += \
    defs.h \
    constraints.h \
    store.h \
    model.h \
    stores/store_sqlite.h \
    query.h \
    expression.h \
    xsddecimal.h

SOURCES += \
    castor.c \
    constraints.c \
    stores/store_sqlite.c \
    model.c \
    store.c \
    query.c \
    expression.c \
    xsddecimal.c
