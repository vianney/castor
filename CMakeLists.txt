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
cmake_minimum_required(VERSION 2.8)
project(Castor)

find_package(PkgConfig)

pkg_check_modules(RAPTOR REQUIRED raptor)
set(INCLUDES ${INCLUDES} ${RAPTOR_INCLUDE_DIRS})
set(LIBS ${LIBS} ${RAPTOR_LIBRARIES})
set(LINKDIRS ${LINKDIRS} ${RAPTOR_LIBRARY_DIRS})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${RAPTOR_CFLAGS}")

pkg_check_modules(RASQAL REQUIRED rasqal)
set(INCLUDES ${INCLUDES} ${RASQAL_INCLUDE_DIRS})
set(LIBS ${LIBS} ${RASQAL_LIBRARIES})
set(LINKDIRS ${LINKDIRS} ${RASQAL_LIBRARY_DIRS})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${RASQAL_CFLAGS}")

pkg_check_modules(REDLAND REQUIRED redland)
set(INCLUDES ${INCLUDES} ${REDLAND_INCLUDE_DIRS})
set(LIBS ${LIBS} ${REDLAND_LIBRARIES})
set(LINKDIRS ${LINKDIRS} ${REDLAND_LIBRARY_DIRS})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${REDLAND_CFLAGS}")

include_directories(SYSTEM ${INCLUDES})
link_directories(${LINKDIRS})

set(CMAKE_C_FLAGS_DEBUG "-g -Wall")

add_subdirectory(src)
