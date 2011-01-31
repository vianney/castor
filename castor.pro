CONFIG -= QT

CONFIG += link_pkgconfig
PKGCONFIG += raptor rasqal
PKGCONFIG += sqlite3

HEADERS += \
    defs.h \
    solver.h \
    constraints.h \
    store.h \
    model.h \
    stores/store_sqlite.h \
    query.h \
    expression.h \
    xsddecimal.h

SOURCES += \
    castor.c \
    solver.c \
    constraints.c \
    stores/store_sqlite.c \
    model.c \
    store.c \
    query.c \
    expression.c \
    xsddecimal.c
