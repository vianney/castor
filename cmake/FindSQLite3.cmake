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

include(LibFindMacros)

libfind_pkg_check_modules(PC_SQLITE3 sqlite3)

find_path(SQLITE3_INCLUDE_DIR
          NAMES sqlite3.h
          HINTS ${PC_SQLITE3_INCLUDE_DIRS})

find_library(SQLITE3_LIBRARY
             NAMES sqlite3
             HINTS ${PC_SQLITE3_LIBRARY_DIRS})

set(SQLITE3_PROCESS_INCLUDES SQLITE3_INCLUDE_DIR)
set(SQLITE3_PROCESS_LIBS SQLITE3_LIBRARY)
libfind_process(SQLITE3)
