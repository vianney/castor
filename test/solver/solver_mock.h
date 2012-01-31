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
#include "solver/solver.h"
#include "gmock/gmock.h"

using ::testing::Return;
using ::testing::_;

namespace castor {
namespace cp {

class MockSolver : public Solver {
public:
    MockSolver() {
        ON_CALL(*this, postStatic()).WillByDefault(Return(true));
        ON_CALL(*this, post(_))     .WillByDefault(Return(true));
        ON_CALL(*this, propagate()) .WillByDefault(Return(true));
    }

    MOCK_METHOD1(add,     void(Constraint *c));
    MOCK_METHOD1(refresh, void(Constraint *c));

    void enqueue(std::vector<Constraint*> &constraints) {
        for(Constraint *c : constraints)
            c->propagate();
    }

private:
    MOCK_METHOD0(postStatic, bool());
    MOCK_METHOD1(post, bool(std::vector<Constraint*> *constraints));
    MOCK_METHOD0(propagate, bool());
    MOCK_METHOD0(clearQueue, void());
};

}
}
