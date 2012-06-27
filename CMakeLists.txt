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
cmake_minimum_required(VERSION 2.6)
project(Castor)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

option(CASTOR_UNITTESTS
       "Add check target to run unit tests (may affect performances)"
       OFF)

option(CASTOR_NOFILTERS
       "Do not post filters as constraints, always perform post-processing"
       OFF)

option(CASTOR_CSTR_TIMING
       "Perform time measurements on some constraints (may affect performances)"
       OFF)

find_package(Raptor2 REQUIRED)
find_package(Rasqal REQUIRED)

include_directories(SYSTEM
    ${RAPTOR2_INCLUDE_DIRS}
    ${RASQAL_INCLUDE_DIRS}
)

set(LIBS
    ${RAPTOR2_LIBRARIES}
    ${RASQAL_LIBRARIES}
)

set(CMAKE_CXX_FLAGS "-std=c++0x -Wall")

add_subdirectory(thirdparty)
add_subdirectory(src)
if(CASTOR_UNITTESTS)
    add_subdirectory(test EXCLUDE_FROM_ALL)
endif(CASTOR_UNITTESTS)
add_subdirectory(tools)
add_subdirectory(doc)
