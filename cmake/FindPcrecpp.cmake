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

libfind_pkg_check_modules(PC_PCRECPP libpcrecpp)

find_path(PCRECPP_INCLUDE_DIR
          NAMES pcrecpp.h
          HINTS ${PC_PCRECPP_INCLUDE_DIRS})

find_library(PCRECPP_LIBRARY
             NAMES pcrecpp
             HINTS ${PC_PCRECPP_LIBRARY_DIRS})

set(PCRECPP_PROCESS_INCLUDES PCRECPP_INCLUDE_DIR)
set(PCRECPP_PROCESS_LIBS PCRECPP_LIBRARY)
libfind_process(PCRECPP)
