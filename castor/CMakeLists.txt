# This file is part of Castor
#
# Author: Vianney le Clément de Saint-Marcq <vianney.leclement@uclouvain.be>
# Copyright (C) 2010 - UCLouvain
#
# Castor is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# Castor is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Castor; if not, see <http://www.gnu.org/licenses/>.
set(CASTOR_SRCS
    defs.h
    castor.h
    castor.c
    model.h
    model.c
    xsddecimal.h
    xsddecimal.c
    store.h
    store.c
    query.h
    query.c
    expression.h
    expression.c
    pattern.h
    pattern.c
    solver.h
    solver.c
    constraints.h
    constraints.c
    stores/store_sqlite.h
    stores/store_sqlite.c
)

add_library(libcastor ${CASTOR_SRCS})
set_target_properties(libcastor PROPERTIES OUTPUT_NAME castor)
target_link_libraries(libcastor ${LIBS})
