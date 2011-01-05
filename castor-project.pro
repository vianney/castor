TEMPLATE = subdirs
SUBDIRS += solver
solver.file = solver.pro

SUBDIRS += castor
castor.file = castor.pro
castor.depends = solver

SUBDIRS += nqueens
nqueens.file = test/nqueens.pro
nqueens.depends = solver

SUBDIRS += createsqlite
createsqlite.file = test/createsqlite.pro
