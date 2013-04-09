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

libfind_pkg_check_modules(PC_RAPTOR2 raptor2)

find_path(RAPTOR2_INCLUDE_DIR
          NAMES raptor2.h
          HINTS ${PC_RAPTOR2_INCLUDE_DIRS}
          PATH_SUFFIXES raptor2)

find_library(RAPTOR2_LIBRARY
             NAMES raptor2
             HINTS ${PC_RAPTOR2_LIBRARY_DIRS})

set(RAPTOR2_PROCESS_INCLUDES RAPTOR2_INCLUDE_DIR)
set(RAPTOR2_PROCESS_LIBS RAPTOR2_LIBRARY)
libfind_process(RAPTOR2)
