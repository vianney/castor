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
#include "solver/boundsvar.h"
#include "solver_mock.h"
#include "constraint_mock.h"
#include "gtest/gtest.h"

using namespace castor::cp;
using ::testing::AtMost;
using ::testing::Between;

class SolverBoundsVarTest : public ::testing::Test {
protected:
    typedef BoundsVariable<unsigned> Var;
    MockSolver solver;
    Var x, y;
    MockConstraint xBind, xMin, xMax, yBind, yMin, yMax;

    //! Upper bound on the values in any domain, will never appear in any of them.
    static const unsigned MAXVAL = 20;

    SolverBoundsVarTest() : x(&solver, 0, 9), y(&solver, 5, 9) {}

    virtual void SetUp() {
        expect_initial_state();
    }

    /**
     * Register constraints to the variables' events
     */
    void registerConstraints() {
        x.registerBind(&xBind);
        x.registerMin(&xMin);
        x.registerMax(&xMax);
        y.registerBind(&yBind);
        y.registerMin(&yMin);
        y.registerMax(&yMax);
    }

    /**
     * Expect variable x to have the domain min..max.
     *
     * @param x the variable to check
     * @param min lower bound
     * @param max upper bound
     */
    void expect_domain(const Var& x, unsigned min, unsigned max) {
        ASSERT_LE(min, max);
        // check size and bound
        EXPECT_EQ(min, x.min());
        EXPECT_EQ(max, x.max());
        EXPECT_EQ(max - min + 1, x.size());
        if(min == max) {
            EXPECT_TRUE(x.isBound());
            EXPECT_EQ(min, x.value());
        } else {
            EXPECT_FALSE(x.isBound());
        }
        // check x.contains()
        for(unsigned v = 0; v <= MAXVAL; v++) {
            if(v >= min && v <= max)
                EXPECT_TRUE(x.contains(v)) << "with value " << v;
            else
                EXPECT_FALSE(x.contains(v)) << "with value " << v;
        }
    }

    /**
     * Check that the variables are left in their initial state
     */
    void expect_initial_state() {
        expect_domain(x, 0, 9);
        expect_domain(y, 5, 9);
    }
};

/**
 * checkpoint() should not overflow
 */
TEST_F(SolverBoundsVarTest, CheckpointOverflow) {
    std::size_t size = x.trailSize();
    char buf[21*size];

    memset(buf, 0xA5, 21*size);
    x.checkpoint(buf + 10*size);
    for(std::size_t i = 0; i < 10*size; i++) {
        EXPECT_EQ('\xA5', buf[i]) << "overflow index " << ((int)i - 10*(int)size);
        EXPECT_EQ('\xA5', buf[i+11*size]) << "overflow index " << i;
    }

    memset(buf, 0x5A, 21*size);
    x.checkpoint(buf + 10*size);
    for(std::size_t i = 0; i < 10*size; i++) {
        EXPECT_EQ('\x5A', buf[i]) << "overflow index " << ((int)i - 10*(int)size);
        EXPECT_EQ('\x5A', buf[i+11*size]) << "overflow index " << i;
    }
}

/**
 * checkpoint() should not modify the domain
 */
TEST_F(SolverBoundsVarTest, CheckpointSanity) {
    char trail[x.trailSize()];
    x.checkpoint(trail);
    y.checkpoint(trail);
    expect_initial_state();
}

/**
 * restore() should restore the domain to the state of a checkpoint
 */
TEST_F(SolverBoundsVarTest, CheckpointRestore) {
    char trail[x.trailSize()];

    x.checkpoint(trail);
    x.updateMin(3);
    x.restore(trail);
    expect_initial_state();

    y.checkpoint(trail);
    y.updateMax(7);
    y.restore(trail);
    expect_initial_state();
}

/**
 * Check the select() method
 */
TEST_F(SolverBoundsVarTest, Select) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate());
    EXPECT_CALL(xMin,    propagate()).Times(AtMost(1));
    EXPECT_CALL(xMax,    propagate()).Times(AtMost(1));
    EXPECT_CALL(yBind,   propagate());
    EXPECT_CALL(yMin,    propagate()).Times(AtMost(1));
    EXPECT_CALL(yMax,    propagate()).Times(AtMost(1));

    x.select();
    EXPECT_TRUE(x.isBound());
    EXPECT_LE(0u, x.value());
    EXPECT_GE(9u, x.value());
    expect_domain(x, x.value(), x.value());

    y.select();
    EXPECT_TRUE(y.isBound());
    EXPECT_LE(5u, y.value());
    EXPECT_GE(9u, y.value());
    expect_domain(y, y.value(), y.value());
}

/**
 * Check the unselect() method
 */
TEST_F(SolverBoundsVarTest, UnSelect) {
    char trail[x.trailSize()];
    unsigned val;

    x.checkpoint(trail);
    x.select();
    EXPECT_TRUE(x.isBound());
    val = x.value();
    x.restore(trail);
    EXPECT_FALSE(x.isBound());
    EXPECT_TRUE(x.contains(val));
    x.unselect();
    EXPECT_FALSE(x.contains(val));
    EXPECT_EQ(9u, x.size());

    y.checkpoint(trail);
    y.select();
    EXPECT_TRUE(y.isBound());
    val = y.value();
    y.restore(trail);
    EXPECT_FALSE(y.isBound());
    EXPECT_TRUE(y.contains(val));
    y.unselect();
    EXPECT_FALSE(y.contains(val));
    EXPECT_EQ(4u, y.size());
}

/**
 * Check the bind() method
 */
TEST_F(SolverBoundsVarTest, Bind) {
    registerConstraints();
    EXPECT_CALL(xBind, propagate());
    EXPECT_CALL(xMin, propagate());
    EXPECT_CALL(xMax, propagate());
    EXPECT_CALL(yBind, propagate());
    EXPECT_CALL(yMin, propagate());
    EXPECT_CALL(yMax, propagate());

    EXPECT_TRUE(x.bind(5));
    expect_domain(x, 5, 5);

    EXPECT_TRUE(x.bind(5));
    EXPECT_FALSE(x.bind(6));

    EXPECT_TRUE(y.bind(7));
    expect_domain(y, 7, 7);

    EXPECT_TRUE(y.bind(7));
    EXPECT_FALSE(y.bind(6));
}

/**
 * Check the bind() method when binding to the minimum value
 */
TEST_F(SolverBoundsVarTest, BindMin) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate());
    EXPECT_CALL(xMin,    propagate()).Times(0);
    EXPECT_CALL(xMax,    propagate());
    EXPECT_CALL(yBind,   propagate());
    EXPECT_CALL(yMin,    propagate()).Times(0);
    EXPECT_CALL(yMax,    propagate());

    EXPECT_TRUE(x.bind(0));
    expect_domain(x, 0, 0);

    EXPECT_TRUE(x.bind(0));
    EXPECT_FALSE(x.bind(6));

    EXPECT_TRUE(y.bind(5));
    expect_domain(y, 5, 5);

    EXPECT_TRUE(y.bind(5));
    EXPECT_FALSE(y.bind(6));
}


/**
 * Check the bind() method when binding to the maximum value
 */
TEST_F(SolverBoundsVarTest, BindMax) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate());
    EXPECT_CALL(xMin,    propagate());
    EXPECT_CALL(xMax,    propagate()).Times(0);
    EXPECT_CALL(yBind,   propagate());
    EXPECT_CALL(yMin,    propagate());
    EXPECT_CALL(yMax,    propagate()).Times(0);

    EXPECT_TRUE(x.bind(9));
    expect_domain(x, 9, 9);

    EXPECT_TRUE(x.bind(9));
    EXPECT_FALSE(x.bind(6));

    EXPECT_TRUE(y.bind(9));
    expect_domain(y, 9, 9);

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
    EXPECT_CALL(xMin,    propagate());
    EXPECT_CALL(xMax,    propagate()).Times(0);
    EXPECT_CALL(yBind,   propagate()).Times(0);
    EXPECT_CALL(yMin,    propagate());
    EXPECT_CALL(yMax,    propagate()).Times(0);

    EXPECT_TRUE(x.updateMin(0));
    expect_initial_state();
    EXPECT_TRUE(y.updateMin(0));
    expect_initial_state();
    EXPECT_TRUE(y.updateMin(3));
    expect_initial_state();
    EXPECT_TRUE(y.updateMin(5));
    expect_initial_state();

    EXPECT_TRUE(x.updateMin(3));
    expect_domain(x, 3, 9);

    EXPECT_FALSE(x.updateMin(15));

    EXPECT_TRUE(y.updateMin(8));
    expect_domain(y, 8, 9);

    EXPECT_FALSE(y.updateMin(16));
}

/**
 * Check updateMin()
 */
TEST_F(SolverBoundsVarTest, UpdateMinBind) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate());
    EXPECT_CALL(xMin,    propagate());
    EXPECT_CALL(xMax,    propagate()).Times(0);
    EXPECT_CALL(yBind,   propagate());
    EXPECT_CALL(yMin,    propagate());
    EXPECT_CALL(yMax,    propagate()).Times(0);

    EXPECT_TRUE(x.updateMin(9));
    expect_domain(x, 9, 9);

    EXPECT_TRUE(y.updateMin(9));
    expect_domain(y, 9, 9);
}

/**
 * Check updateMax()
 */
TEST_F(SolverBoundsVarTest, UpdateMax) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate()).Times(0);
    EXPECT_CALL(xMin,    propagate()).Times(0);
    EXPECT_CALL(xMax,    propagate());
    EXPECT_CALL(yBind,   propagate()).Times(0);
    EXPECT_CALL(yMin,    propagate()).Times(0);
    EXPECT_CALL(yMax,    propagate());

    EXPECT_TRUE(x.updateMax(15));
    expect_initial_state();
    EXPECT_TRUE(x.updateMax(9));
    expect_initial_state();
    EXPECT_TRUE(y.updateMax(10));
    expect_initial_state();
    EXPECT_TRUE(y.updateMax(9));
    expect_initial_state();

    EXPECT_TRUE(x.updateMax(7));
    expect_domain(x, 0, 7);

    EXPECT_TRUE(y.updateMax(8));
    expect_domain(y, 5, 8);

    EXPECT_FALSE(y.updateMax(3));
}

/**
 * Check updateMin()
 */
TEST_F(SolverBoundsVarTest, UpdateMaxBind) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate());
    EXPECT_CALL(xMin,    propagate()).Times(0);
    EXPECT_CALL(xMax,    propagate());
    EXPECT_CALL(yBind,   propagate());
    EXPECT_CALL(yMin,    propagate()).Times(0);
    EXPECT_CALL(yMax,    propagate());

    EXPECT_TRUE(x.updateMax(0));
    expect_domain(x, 0, 0);

    EXPECT_TRUE(y.updateMax(5));
    expect_domain(y, 5, 5);
}
