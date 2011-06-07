# This file is part of Castor
#
# Author: Vianney le Clément de Saint-Marcq <vianney.leclement@uclouvain.be>
# Copyright (C) 2010-2011, UCLouvain
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
cmake_minimum_required(VERSION 2.6)
project(Castor)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

find_package(Raptor2 REQUIRED)
find_package(Rasqal REQUIRED)
find_package(SQLite3 REQUIRED)

include_directories(SYSTEM
    ${RAPTOR2_INCLUDE_DIRS}
    ${RASQAL_INCLUDE_DIRS}
    ${SQLITE3_INCLUDE_DIRS}
)

set(LIBS
    ${RAPTOR2_LIBRARIES}
    ${RASQAL_LIBRARIES}
    ${SQLITE3_LIBRARIES}
)

set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall")

add_subdirectory(castor)
add_subdirectory(test)
