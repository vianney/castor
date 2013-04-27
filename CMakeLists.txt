# This file is part of Castor
#
# Author: Vianney le Clément de Saint-Marcq <vianney.leclement@uclouvain.be>
# Copyright (C) 2010-2013, Université catholique de Louvain
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
cmake_minimum_required(VERSION 2.8.0 FATAL_ERROR)
project(Castor)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

option(CASTOR_UNITTESTS
       "Add check target to run unit tests (may affect performances)"
       OFF)

option(CASTOR_NOFILTERS
       "Do not post filters as constraints, always perform post-processing"
       OFF)

option(CASTOR_CSTR_TIMING
       "Perform time measurements during propagation (may affect performances)"
       OFF)

option(CASTOR_ARITHMETIC_CSTR
       "Use redundant arithmetic constraints"
       ON)

set(CASTOR_TRIPLEPROPAG "fcplus" CACHE STRING "Triple constraint propagation")
set_property(CACHE CASTOR_TRIPLEPROPAG PROPERTY STRINGS
             "fc" "fcplus" "dc")

set(CASTOR_SEARCH "dom" CACHE STRING "Search heuristic")
set_property(CACHE CASTOR_SEARCH PROPERTY STRINGS
             "dom" "deg" "ddeg" "domdeg" "domddeg")

find_package(Raptor2 REQUIRED)
find_package(Rasqal REQUIRED)
find_package(Pcrecpp REQUIRED)

include_directories(SYSTEM
    ${RAPTOR2_INCLUDE_DIRS}
    ${RASQAL_INCLUDE_DIRS}
    ${PCRECPP_INCLUDE_DIRS}
)

set(LIBS
    ${RAPTOR2_LIBRARIES}
    ${RASQAL_LIBRARIES}
    ${PCRECPP_LIBRARIES}
)

set(CMAKE_CXX_FLAGS "-std=c++11 -Wall")

add_subdirectory(thirdparty)
add_subdirectory(src)
if(CASTOR_UNITTESTS)
    add_subdirectory(test EXCLUDE_FROM_ALL)
endif(CASTOR_UNITTESTS)
add_subdirectory(tools)
add_subdirectory(doc)
