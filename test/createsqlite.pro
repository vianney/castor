INCLUDEPATH += ..

CONFIG += link_pkgconfig
PKGCONFIG += sqlite3 raptor2 rasqal

HEADERS += ../model.h ../xsddecimal.h

SOURCES += \
    createsqlite.c \
    ../model.c \
    ../xsddecimal.c
