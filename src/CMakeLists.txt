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
    config.h.in
    solver/solver.h
    solver/solver.cpp
    solver/trail.h
    solver/trail.cpp
    solver/variable.h
    solver/discretevar.h
    solver/boundsvar.h
    solver/smallvar.h
    solver/constraint.h
    solver/subtree.h
    solver/subtree.cpp
    store/btree.h
    store/triplecache.h
    store/triplecache.cpp
    constraints/unary.h
    constraints/triple.h
    constraints/triple.cpp
    constraints/fallback.h
    constraints/fallback.cpp
    constraints/compare.h
    constraints/compare.cpp
    constraints/distinct.h
    constraints/distinct.cpp
    constraints/bnborder.h
    constraints/bnborder.cpp
    util.h
    util.cpp
    librdfwrapper.h
    librdfwrapper.cpp
    xsddecimal.h
    xsddatetime.h
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
)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
               "${CMAKE_CURRENT_BINARY_DIR}/config.h")
include_directories("${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(libcastor ${CASTOR_SRCS})
set_target_properties(libcastor PROPERTIES OUTPUT_NAME castor)
target_link_libraries(libcastor ${LIBS})
