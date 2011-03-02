INCLUDEPATH += ..

CONFIG += link_pkgconfig
PKGCONFIG += sqlite3 raptor rasqal

HEADERS += ../model.h ../xsddecimal.h

SOURCES += \
    createsqlite.c \
    ../model.c \
    ../xsddecimal.c
