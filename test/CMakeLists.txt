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
set(TEST_SRCS
    solver/solver_mock.h
    solver/constraint_mock.h
    solver/discretevar.cpp
)

include_directories("${PROJECT_SOURCE_DIR}/src"
                    "${PROJECT_BINARY_DIR}/src")
include_directories("${PROJECT_SOURCE_DIR}/thirdparty/googlemock/include")
include_directories("${PROJECT_SOURCE_DIR}/thirdparty/googlemock/gtest/include")

add_executable(unittests ${TEST_SRCS})
target_link_libraries(unittests libcastor gmock_main)

add_custom_target(check unittests >&2
    DEPENDS unittests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running unit tests")
