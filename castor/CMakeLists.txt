# This file is part of Castor
#
# Author: Vianney le Clément de Saint-Marcq <vianney.leclement@uclouvain.be>
# Copyright (C) 2010-2012, Université catholique de Louvain
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, version 3 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
set(CASTOR_SRCS
    config.h
    solver/solver.h
    solver/solver.cpp
    solver/variable.h
    solver/discretevar.h
    solver/constraint.h
    solver/subtree.h
    solver/subtree.cpp
    store/readutils.h
    store/readutils.cpp
    store/btree.h
    store/btree.cpp
    store/triplecache.h
    store/triplecache.cpp
    util.h
    util.cpp
    librdfwrapper.h
    librdfwrapper.cpp
    xsddecimal.h
    model.h
    model.cpp
    store.h
    store.cpp
    variable.h
    variable.cpp
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
    bnborder.h
    bnborder.cpp
)

add_library(libcastor ${CASTOR_SRCS})
set_target_properties(libcastor PROPERTIES OUTPUT_NAME castor)
target_link_libraries(libcastor ${LIBS})
