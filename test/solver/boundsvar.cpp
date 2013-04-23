/* This file is part of Castor
 *
 * Author: Vianney le Clément de Saint-Marcq <vianney.leclement@uclouvain.be>
 * Copyright (C) 2010-2013, Université catholique de Louvain
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
#include "solver/boundsvar.h"
#include "solver_mock.h"
#include "constraint_mock.h"
#include "gtest/gtest.h"

using namespace castor::cp;
using ::testing::AtMost;
using ::testing::Between;

////////////////////////////////////////////////////////////////////////////////
// Assertions

/**
 * Expect variable x to have the domain lb..ub.
 *
 * @param x the variable to check
 * @param lb lower bound
 * @param ub upper bound
 */
#define EXPECT_DOMAIN(x, lb, ub) { \
    ASSERT_LE(lb, ub); \
    EXPECT_EQ(lb, x.min()); \
    EXPECT_EQ(ub, x.max()); \
    EXPECT_EQ(ub - lb + 1, x.size()); \
    if(lb == ub) { \
        EXPECT_TRUE(x.bound()); \
        EXPECT_EQ(lb, x.value()); \
    } else { \
        EXPECT_FALSE(x.bound()); \
    } \
    for(unsigned v = 0; v <= MAXVAL; v++) { \
        if(v >= lb && v <= ub) \
            EXPECT_TRUE(x.contains(v)) << "with value " << v; \
        else \
            EXPECT_FALSE(x.contains(v)) << "with value " << v; \
    } \
}

/**
 * Check that the variables are left in their initial state
 */
#define EXPECT_INITIAL_STATE { \
    EXPECT_DOMAIN(x, 0u, 9u); \
    EXPECT_DOMAIN(y, 5u, 9u); \
}

////////////////////////////////////////////////////////////////////////////////
// Fixture

class SolverBoundsVarTest : public ::testing::Test {
protected:
    typedef BoundsDecisionVariable<unsigned> Var;
    MockSolver solver;
    Var x, y;
    MockConstraint xBind, xBounds, yBind, yBounds;

    //! Upper bound on the values in any domain, will never appear in any of them.
    static const unsigned MAXVAL = 20;

    SolverBoundsVarTest() : x(&solver, 0, 9), y(&solver, 5, 9),
        xBind(&solver), xBounds(&solver),
        yBind(&solver), yBounds(&solver) {}

    virtual void SetUp() {
        EXPECT_INITIAL_STATE;
    }

    /**
     * Register constraints to the variables' events
     */
    void registerConstraints() {
        x.registerBind(&xBind);
        x.registerBounds(&xBounds);
        y.registerBind(&yBind);
        y.registerBounds(&yBounds);
    }
};

////////////////////////////////////////////////////////////////////////////////
// Tests

/**
 * save() should not modify the domain
 */
TEST_F(SolverBoundsVarTest, SaveSanity) {
    x.save(solver.trail());
    y.save(solver.trail());
    EXPECT_INITIAL_STATE;
}

/**
 * restore() should restore the domain to the state of a checkpoint
 */
TEST_F(SolverBoundsVarTest, Restore) {
    Trail::checkpoint_t chkp = solver.trail().checkpoint();
    x.updateMin(3);
    solver.trail().restore(chkp);
    EXPECT_INITIAL_STATE;

    chkp = solver.trail().checkpoint();
    y.updateMax(7);
    solver.trail().restore(chkp);
    EXPECT_INITIAL_STATE;
}

/**
 * Check the select() method
 */
TEST_F(SolverBoundsVarTest, Label) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate());
    EXPECT_CALL(xBounds, propagate()).Times(AtMost(1));
    EXPECT_CALL(yBind,   propagate());
    EXPECT_CALL(yBounds, propagate()).Times(AtMost(1));

    x.label();
    EXPECT_TRUE(x.bound());
    EXPECT_LE(0u, x.value());
    EXPECT_GE(9u, x.value());
    EXPECT_DOMAIN(x, x.value(), x.value());

    y.label();
    EXPECT_TRUE(y.bound());
    EXPECT_LE(5u, y.value());
    EXPECT_GE(9u, y.value());
    EXPECT_DOMAIN(y, y.value(), y.value());
}

/**
 * Check the unselect() method
 */
TEST_F(SolverBoundsVarTest, UnLabel) {
    unsigned val;

    Trail::checkpoint_t chkp = solver.trail().checkpoint();
    x.label();
    EXPECT_TRUE(x.bound());
    val = x.value();
    solver.trail().restore(chkp);
    EXPECT_FALSE(x.bound());
    EXPECT_TRUE(x.contains(val));
    x.unlabel();
    EXPECT_FALSE(x.contains(val));
    EXPECT_EQ(9u, x.size());

    chkp = solver.trail().checkpoint();
    y.label();
    EXPECT_TRUE(y.bound());
    val = y.value();
    solver.trail().restore(chkp);
    EXPECT_FALSE(y.bound());
    EXPECT_TRUE(y.contains(val));
    y.unlabel();
    EXPECT_FALSE(y.contains(val));
    EXPECT_EQ(4u, y.size());
}

/**
 * Check the bind() method
 */
TEST_F(SolverBoundsVarTest, Bind) {
    registerConstraints();
    EXPECT_CALL(xBind, propagate());
    EXPECT_CALL(xBounds, propagate());
    EXPECT_CALL(yBind, propagate());
    EXPECT_CALL(yBounds, propagate());

    EXPECT_TRUE(x.bind(5));
    EXPECT_DOMAIN(x, 5u, 5u);

    EXPECT_TRUE(x.bind(5));
    EXPECT_FALSE(x.bind(6));

    EXPECT_TRUE(y.bind(7));
    EXPECT_DOMAIN(y, 7u, 7u);

    EXPECT_TRUE(y.bind(7));
    EXPECT_FALSE(y.bind(6));
}

/**
 * Check the bind() method when binding to the minimum value
 */
TEST_F(SolverBoundsVarTest, BindMin) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate());
    EXPECT_CALL(xBounds, propagate());
    EXPECT_CALL(yBind,   propagate());
    EXPECT_CALL(yBounds, propagate());

    EXPECT_TRUE(x.bind(0));
    EXPECT_DOMAIN(x, 0u, 0u);

    EXPECT_TRUE(x.bind(0));
    EXPECT_FALSE(x.bind(6));

    EXPECT_TRUE(y.bind(5));
    EXPECT_DOMAIN(y, 5u, 5u);

    EXPECT_TRUE(y.bind(5));
    EXPECT_FALSE(y.bind(6));
}


/**
 * Check the bind() method when binding to the maximum value
 */
TEST_F(SolverBoundsVarTest, BindMax) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate());
    EXPECT_CALL(xBounds, propagate());
    EXPECT_CALL(yBind,   propagate());
    EXPECT_CALL(yBounds, propagate());

    EXPECT_TRUE(x.bind(9));
    EXPECT_DOMAIN(x, 9u, 9u);

    EXPECT_TRUE(x.bind(9));
    EXPECT_FALSE(x.bind(6));

    EXPECT_TRUE(y.bind(9));
    EXPECT_DOMAIN(y, 9u, 9u);

    EXPECT_TRUE(y.bind(9));
    EXPECT_FALSE(y.bind(6));
}

/**
 * bind() to a value outside the domain should fail
 */
TEST_F(SolverBoundsVarTest, BindFail) {
    EXPECT_FALSE(x.bind(18));
    EXPECT_FALSE(y.bind(3));
}

/**
 * Check updateMin()
 */
TEST_F(SolverBoundsVarTest, UpdateMin) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate()).Times(0);
    EXPECT_CALL(xBounds, propagate());
    EXPECT_CALL(yBind,   propagate()).Times(0);
    EXPECT_CALL(yBounds, propagate());

    EXPECT_TRUE(x.updateMin(0));
    EXPECT_INITIAL_STATE;
    EXPECT_TRUE(y.updateMin(0));
    EXPECT_INITIAL_STATE;
    EXPECT_TRUE(y.updateMin(3));
    EXPECT_INITIAL_STATE;
    EXPECT_TRUE(y.updateMin(5));
    EXPECT_INITIAL_STATE;

    EXPECT_TRUE(x.updateMin(3));
    EXPECT_DOMAIN(x, 3u, 9u);

    EXPECT_FALSE(x.updateMin(15));

    EXPECT_TRUE(y.updateMin(8));
    EXPECT_DOMAIN(y, 8u, 9u);

    EXPECT_FALSE(y.updateMin(16));
}

/**
 * Check updateMin()
 */
TEST_F(SolverBoundsVarTest, UpdateMinBind) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate());
    EXPECT_CALL(xBounds, propagate());
    EXPECT_CALL(yBind,   propagate());
    EXPECT_CALL(yBounds, propagate());

    EXPECT_TRUE(x.updateMin(9));
    EXPECT_DOMAIN(x, 9u, 9u);

    EXPECT_TRUE(y.updateMin(9));
    EXPECT_DOMAIN(y, 9u, 9u);
}

/**
 * Check updateMax()
 */
TEST_F(SolverBoundsVarTest, UpdateMax) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate()).Times(0);
    EXPECT_CALL(xBounds, propagate());
    EXPECT_CALL(yBind,   propagate()).Times(0);
    EXPECT_CALL(yBounds, propagate());

    EXPECT_TRUE(x.updateMax(15));
    EXPECT_INITIAL_STATE;
    EXPECT_TRUE(x.updateMax(9));
    EXPECT_INITIAL_STATE;
    EXPECT_TRUE(y.updateMax(10));
    EXPECT_INITIAL_STATE;
    EXPECT_TRUE(y.updateMax(9));
    EXPECT_INITIAL_STATE;

    EXPECT_TRUE(x.updateMax(7));
    EXPECT_DOMAIN(x, 0u, 7u);

    EXPECT_TRUE(y.updateMax(8));
    EXPECT_DOMAIN(y, 5u, 8u);

    EXPECT_FALSE(y.updateMax(3));
}

/**
 * Check updateMin()
 */
TEST_F(SolverBoundsVarTest, UpdateMaxBind) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate());
    EXPECT_CALL(xBounds, propagate());
    EXPECT_CALL(yBind,   propagate());
    EXPECT_CALL(yBounds, propagate());

    EXPECT_TRUE(x.updateMax(0));
    EXPECT_DOMAIN(x, 0u, 0u);

    EXPECT_TRUE(y.updateMax(5));
    EXPECT_DOMAIN(y, 5u, 5u);
}
