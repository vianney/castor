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
include(LibFindMacros)

libfind_package(Rasqal Raptor2)

libfind_pkg_check_modules(PC_RASQAL rasqal)

find_path(RASQAL_INCLUDE_DIR
          NAMES rasqal.h
          HINTS ${PC_RASQAL_INCLUDE_DIRS}
          PATH_SUFFIXES rasqal)

find_library(RASQAL_LIBRARY
             NAMES rasqal
             HINTS ${PC_RASQAL_LIBRARY_DIRS})

set(RASQAL_PROCESS_INCLUDES RASQAL_INCLUDE_DIR RAPTOR2_INCLUDE_DIRS)
set(RASQAL_PROCESS_LIBS RASQAL_LIBRARY RAPTOR2_LIBRARIES)
libfind_process(RASQAL)
