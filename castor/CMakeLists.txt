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
    solver/solver.h
    solver/solver.cpp
    solver/varint.h
    solver/varint.cpp
    solver/constraint.h
    solver/subtree.h
    solver/subtree.cpp
    util.h
    librdfwrapper.h
    librdfwrapper.cpp
    xsddecimal.h
    model.h
    model.cpp
    store.h
    store.cpp
    expression.h
    expression.cpp
    pattern.h
    pattern.cpp
    query.h
    query.cpp
    constraints.h
    constraints.cpp
    distinct.h
    distinct.cpp
)

add_library(libcastor ${CASTOR_SRCS})
set_target_properties(libcastor PROPERTIES OUTPUT_NAME castor)
target_link_libraries(libcastor ${LIBS})
