/* This file is part of Castor
 *
 * Author: Vianney le Clément de Saint-Marcq <vianney.leclement@uclouvain.be>
 * Copyright (C) 2010-2012, Université catholique de Louvain
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "solver/constraint.h"
#include "gmock/gmock.h"

using ::testing::Return;

namespace castor {
namespace cp {

class MockConstraint : public Constraint {
public:
    MockConstraint(Solver* solver, Priority priority = PRIOR_MEDIUM) :
            Constraint(solver, priority) {
        ON_CALL(*this, post())     .WillByDefault(Return(true));
        ON_CALL(*this, propagate()).WillByDefault(Return(true));
    }

    MOCK_METHOD0(init, void());
    MOCK_METHOD0(post, bool());
    MOCK_METHOD0(propagate, bool());
};

}
}
