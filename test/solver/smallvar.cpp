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
#include "solver/smallvar.h"
#include "solver_mock.h"
#include "constraint_mock.h"
#include "gtest/gtest.h"

using namespace castor::cp;
using ::testing::AtMost;
using ::testing::Between;

////////////////////////////////////////////////////////////////////////////////
// Assertions

/**
 * Expect variable x to have the domain dom.
 *
 * @param x the variable to check
 * @param ... the domain values (at least 1)
 */
#define EXPECT_DOMAIN(x, ...) { \
    std::initializer_list<unsigned> dom = {__VA_ARGS__}; \
    ASSERT_LT(0u, dom.size()); \
    if(dom.size() == 1) { \
        EXPECT_TRUE(x.bound()); \
        EXPECT_EQ(*dom.begin(), x.value()); \
        EXPECT_EQ(*dom.begin(), x.min()); \
        EXPECT_EQ(*dom.begin(), x.max()); \
    } else { \
        EXPECT_FALSE(x.bound()); \
    } \
    unsigned min = MAXVAL, max = 0; \
    for(unsigned v : dom) { \
        if(v < min) min = v; \
        if(v > max) max = v; \
    } \
    EXPECT_EQ(min, x.min()); \
    EXPECT_EQ(max, x.max()); \
    for(unsigned v = 0; v <= MAXVAL; v++) { \
        bool found = false; \
        for(unsigned v2 : dom) { \
            if(v2 == v) { \
                found = true; \
                break; \
            } \
        } \
        if(found) \
            EXPECT_TRUE(x.contains(v)) << "with value " << v; \
        else \
            EXPECT_FALSE(x.contains(v)) << "with value " << v; \
    } \
}

/**
 * Expect boolean variable b to be unbound.
 */
#define EXPECT_BOOLEAN_UNBOUND(b) { \
    EXPECT_FALSE(b.bound()); \
    EXPECT_TRUE(b.contains(false)); \
    EXPECT_TRUE(b.contains(true)); \
    EXPECT_EQ(0, b.min()); \
    EXPECT_EQ(1, b.max()); \
}

/**
 * Expect boolean variable b to be bound to value v.
 */
#define EXPECT_BOOLEAN(v, b) { \
    EXPECT_TRUE(b.bound()); \
    EXPECT_TRUE(b.contains(v)); \
    EXPECT_FALSE(b.contains(!v)); \
    if(v) \
        EXPECT_TRUE(b.value()); \
    else \
        EXPECT_FALSE(b.value()); \
    EXPECT_EQ(v ? 1 : 0, b.min()); \
    EXPECT_EQ(v ? 1 : 0, b.max()); \
}

/**
 * Check that the variables are left in their initial state
 */
#define EXPECT_INITIAL_STATE { \
    EXPECT_DOMAIN(x, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9); \
    EXPECT_DOMAIN(y, 5, 6, 7, 8, 9); \
    EXPECT_BOOLEAN_UNBOUND(b); \
}

////////////////////////////////////////////////////////////////////////////////
// Fixture

class SolverSmallVarTest : public ::testing::Test {
protected:
    typedef SmallVariable<unsigned> Var;
    MockSolver solver;
    Var x, y;
    BooleanVariable b;
    MockConstraint xBind, xChange, xMin, xMax,
                   yBind, yChange, yMin, yMax,
                   bBind, bChange;

    //! Upper bound on the values in any domain, will never appear in any of them.
    static const unsigned MAXVAL = 20;

    SolverSmallVarTest() : x(&solver, 0, 9), y(&solver, 5, 9), b(&solver) {}

    virtual void SetUp() {
        EXPECT_INITIAL_STATE;
    }

    /**
     * Register constraints to the variables' events
     */
    void registerConstraints() {
        x.registerBind(&xBind);
        x.registerChange(&xChange);
        x.registerMin(&xMin);
        x.registerMax(&xMax);
        y.registerBind(&yBind);
        y.registerChange(&yChange);
        y.registerMin(&yMin);
        y.registerMax(&yMax);
        b.registerBind(&bBind);
        b.registerChange(&bChange);
    }
};

////////////////////////////////////////////////////////////////////////////////
// Tests

/**
 * checkpoint() should not overflow
 */
TEST_F(SolverSmallVarTest, CheckpointOverflow) {
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
TEST_F(SolverSmallVarTest, CheckpointSanity) {
    char trail[x.trailSize()];
    x.checkpoint(trail);
    y.checkpoint(trail);
    b.checkpoint(trail);
    EXPECT_INITIAL_STATE;
}

/**
 * restore() should restore the domain to the state of a checkpoint
 */
TEST_F(SolverSmallVarTest, CheckpointRestore) {
    char trail[x.trailSize()];

    x.checkpoint(trail);
    x.remove(8);
    x.restore(trail);
    EXPECT_INITIAL_STATE;

    y.checkpoint(trail);
    y.updateMin(7);
    y.restore(trail);
    EXPECT_INITIAL_STATE;

    b.checkpoint(trail);
    b.bind(false);
    b.restore(trail);
    EXPECT_INITIAL_STATE;
}

/**
 * Check the bind() method
 */
TEST_F(SolverSmallVarTest, Bind) {
    registerConstraints();
    EXPECT_CALL(xBind, propagate());
    EXPECT_CALL(xChange, propagate());
    EXPECT_CALL(xMin, propagate());
    EXPECT_CALL(xMax, propagate());
    EXPECT_CALL(yBind, propagate());
    EXPECT_CALL(yChange, propagate());
    EXPECT_CALL(yMin, propagate());
    EXPECT_CALL(yMax, propagate());
    EXPECT_CALL(bBind, propagate());
    EXPECT_CALL(bChange, propagate());

    EXPECT_TRUE(x.bind(5));
    EXPECT_DOMAIN(x, 5);

    EXPECT_TRUE(x.bind(5));
    EXPECT_FALSE(x.bind(6));

    EXPECT_TRUE(y.bind(7));
    EXPECT_DOMAIN(y, 7);

    EXPECT_TRUE(y.bind(7));
    EXPECT_FALSE(y.bind(6));

    EXPECT_TRUE(b.bind(true));
    EXPECT_BOOLEAN(true, b);

    EXPECT_TRUE(b.bind(true));
    EXPECT_FALSE(b.bind(false));
}

/**
 * Check the bind() method when binding to the minimum value
 */
TEST_F(SolverSmallVarTest, BindMin) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate());
    EXPECT_CALL(xChange, propagate());
    EXPECT_CALL(xMin,    propagate()).Times(0);
    EXPECT_CALL(xMax,    propagate());
    EXPECT_CALL(yBind,   propagate());
    EXPECT_CALL(yChange, propagate());
    EXPECT_CALL(yMin,    propagate()).Times(0);
    EXPECT_CALL(yMax,    propagate());

    EXPECT_TRUE(x.bind(0));
    EXPECT_DOMAIN(x, 0);

    EXPECT_TRUE(x.bind(0));
    EXPECT_FALSE(x.bind(6));

    EXPECT_TRUE(y.bind(5));
    EXPECT_DOMAIN(y, 5);

    EXPECT_TRUE(y.bind(5));
    EXPECT_FALSE(y.bind(6));
}


/**
 * Check the bind() method when binding to the maximum value
 */
TEST_F(SolverSmallVarTest, BindMax) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate());
    EXPECT_CALL(xChange, propagate());
    EXPECT_CALL(xMin,    propagate());
    EXPECT_CALL(xMax,    propagate()).Times(0);
    EXPECT_CALL(yBind,   propagate());
    EXPECT_CALL(yChange, propagate());
    EXPECT_CALL(yMin,    propagate());
    EXPECT_CALL(yMax,    propagate()).Times(0);

    EXPECT_TRUE(x.bind(9));
    EXPECT_DOMAIN(x, 9);

    EXPECT_TRUE(x.bind(9));
    EXPECT_FALSE(x.bind(6));

    EXPECT_TRUE(y.bind(9));
    EXPECT_DOMAIN(y, 9);

    EXPECT_TRUE(y.bind(9));
    EXPECT_FALSE(y.bind(6));
}

/**
 * bind() to a value outside the domain should fail
 */
TEST_F(SolverSmallVarTest, BindFail) {
    EXPECT_FALSE(x.bind(18));
    EXPECT_FALSE(y.bind(3));
}

/**
 * Check remove()
 */
TEST_F(SolverSmallVarTest, Remove) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate()).Times(0);
    EXPECT_CALL(xChange, propagate());
    EXPECT_CALL(xMin,    propagate()).Times(0);
    EXPECT_CALL(xMax,    propagate()).Times(0);
    EXPECT_CALL(yBind,   propagate()).Times(0);
    EXPECT_CALL(yChange, propagate());
    EXPECT_CALL(yMin,    propagate()).Times(0);
    EXPECT_CALL(yMax,    propagate()).Times(0);
    EXPECT_CALL(bBind,   propagate());
    EXPECT_CALL(bChange, propagate());

    EXPECT_TRUE(x.remove(6));
    EXPECT_DOMAIN(x, 0, 1, 2, 3, 4, 5, /*6,*/ 7, 8, 9);

    EXPECT_TRUE(y.remove(7));
    EXPECT_DOMAIN(y, 5, 6, /*7,*/ 8, 9);

    EXPECT_TRUE(b.remove(false));
    EXPECT_BOOLEAN(true, b);
}

/**
 * Check remove() when removing minimum value
 */
TEST_F(SolverSmallVarTest, RemoveMin) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate()).Times(0);
    EXPECT_CALL(xChange, propagate());
    EXPECT_CALL(xMin,    propagate());
    EXPECT_CALL(xMax,    propagate()).Times(0);
    EXPECT_CALL(yBind,   propagate()).Times(0);
    EXPECT_CALL(yChange, propagate());
    EXPECT_CALL(yMin,    propagate());
    EXPECT_CALL(yMax,    propagate()).Times(0);

    EXPECT_TRUE(x.remove(0));
    EXPECT_DOMAIN(x, /*0,*/ 1, 2, 3, 4, 5, 6, 7, 8, 9);

    EXPECT_TRUE(y.remove(5));
    EXPECT_DOMAIN(y, /*5,*/ 6, 7, 8, 9);
}

/**
 * Check remove() when removing maximum value
 */
TEST_F(SolverSmallVarTest, RemoveMax) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate()).Times(0);
    EXPECT_CALL(xChange, propagate());
    EXPECT_CALL(xMin,    propagate()).Times(0);
    EXPECT_CALL(xMax,    propagate());
    EXPECT_CALL(yBind,   propagate()).Times(0);
    EXPECT_CALL(yChange, propagate());
    EXPECT_CALL(yMin,    propagate()).Times(0);
    EXPECT_CALL(yMax,    propagate());

    EXPECT_TRUE(x.remove(9));
    EXPECT_DOMAIN(x, 0, 1, 2, 3, 4, 5, 6, 7, 8/*, 9*/);

    EXPECT_TRUE(y.remove(9));
    EXPECT_DOMAIN(y, 5, 6, 7, 8/*, 9*/);
}

/**
 * Check remove() when removing all values but one (should generate a bind
 * event)
 */
TEST_F(SolverSmallVarTest, RemoveAllButOne) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate());
    EXPECT_CALL(xChange, propagate()).Times(9);
    EXPECT_CALL(xMin,    propagate());
    EXPECT_CALL(xMax,    propagate());
    EXPECT_CALL(yBind,   propagate());
    EXPECT_CALL(yChange, propagate()).Times(4);
    EXPECT_CALL(yMin,    propagate());
    EXPECT_CALL(yMax,    propagate());
    EXPECT_CALL(bBind,   propagate());
    EXPECT_CALL(bChange, propagate());

    for(unsigned v = 2; v <= 9; v++)
        EXPECT_TRUE(x.remove(v)) << "with value " << v;
    EXPECT_TRUE(x.remove(0));
    EXPECT_DOMAIN(x, 1);

    EXPECT_TRUE(y.remove(6));
    EXPECT_TRUE(y.remove(8));
    EXPECT_TRUE(y.remove(5));
    EXPECT_TRUE(y.remove(9));
    EXPECT_DOMAIN(y, 7);

    EXPECT_TRUE(b.remove(true));
    EXPECT_BOOLEAN(false, b);
}

/**
 * Check restrictToMarks()
 */
TEST_F(SolverSmallVarTest, Restrict) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate()).Times(0);
    EXPECT_CALL(xChange, propagate());
    EXPECT_CALL(xMin,    propagate());
    EXPECT_CALL(xMax,    propagate());
    EXPECT_CALL(yBind,   propagate()).Times(0);
    EXPECT_CALL(yChange, propagate());
    EXPECT_CALL(yMin,    propagate());
    EXPECT_CALL(yMax,    propagate());

    x.clearMarks();
    x.mark(4);
    x.mark(2);
    x.mark(7);
    EXPECT_TRUE(x.restrictToMarks());
    EXPECT_DOMAIN(x, 2, 4, 7);

    y.clearMarks();
    y.mark(0);
    y.mark(8);
    y.mark(6);
    EXPECT_TRUE(y.restrictToMarks());
    EXPECT_DOMAIN(y, 6, 8);
}

/**
 * Check restrictToMarks()
 */
TEST_F(SolverSmallVarTest, RestrictMin) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate()).Times(0);
    EXPECT_CALL(xChange, propagate());
    EXPECT_CALL(xMin,    propagate()).Times(0);
    EXPECT_CALL(xMax,    propagate());
    EXPECT_CALL(yBind,   propagate()).Times(0);
    EXPECT_CALL(yChange, propagate());
    EXPECT_CALL(yMin,    propagate()).Times(0);
    EXPECT_CALL(yMax,    propagate());

    x.clearMarks();
    x.mark(4);
    x.mark(2);
    x.mark(0);
    x.mark(7);
    EXPECT_TRUE(x.restrictToMarks());
    EXPECT_DOMAIN(x, 0, 2, 4, 7);

    y.clearMarks();
    y.mark(5);
    y.mark(0);
    y.mark(8);
    y.mark(6);
    EXPECT_TRUE(y.restrictToMarks());
    EXPECT_DOMAIN(y, 5, 6, 8);
}

/**
 * Check restrictToMarks()
 */
TEST_F(SolverSmallVarTest, RestrictMax) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate()).Times(0);
    EXPECT_CALL(xChange, propagate());
    EXPECT_CALL(xMin,    propagate());
    EXPECT_CALL(xMax,    propagate()).Times(0);
    EXPECT_CALL(yBind,   propagate()).Times(0);
    EXPECT_CALL(yChange, propagate());
    EXPECT_CALL(yMin,    propagate());
    EXPECT_CALL(yMax,    propagate()).Times(0);

    x.clearMarks();
    x.mark(4);
    x.mark(2);
    x.mark(7);
    x.mark(9);
    EXPECT_TRUE(x.restrictToMarks());
    EXPECT_DOMAIN(x, 2, 4, 7, 9);

    y.clearMarks();
    y.mark(0);
    y.mark(9);
    y.mark(8);
    y.mark(6);
    EXPECT_TRUE(y.restrictToMarks());
    EXPECT_DOMAIN(y, 6, 8, 9);
}

/**
 * Check restrictToMarks()
 */
TEST_F(SolverSmallVarTest, RestrictNoOp) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate()).Times(0);
    EXPECT_CALL(xChange, propagate()).Times(0);
    EXPECT_CALL(xMin,    propagate()).Times(0);
    EXPECT_CALL(xMax,    propagate()).Times(0);
    EXPECT_CALL(yBind,   propagate()).Times(0);
    EXPECT_CALL(yChange, propagate()).Times(0);
    EXPECT_CALL(yMin,    propagate()).Times(0);
    EXPECT_CALL(yMax,    propagate()).Times(0);

    x.clearMarks();
    for(unsigned v = 0; v <= 9; v++)
        x.mark(v);
    EXPECT_TRUE(x.restrictToMarks());
    EXPECT_INITIAL_STATE;

    y.clearMarks();
    y.mark(6);
    y.mark(5);
    y.mark(8);
    y.mark(9);
    y.mark(7);
    EXPECT_TRUE(y.restrictToMarks());
    EXPECT_INITIAL_STATE;
}

/**
 * Check restrictToMarks()
 */
TEST_F(SolverSmallVarTest, RestrictFail) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate()).Times(0);
    EXPECT_CALL(xChange, propagate()).Times(0);
    EXPECT_CALL(xMin,    propagate()).Times(0);
    EXPECT_CALL(xMax,    propagate()).Times(0);
    EXPECT_CALL(yBind,   propagate()).Times(0);
    EXPECT_CALL(yChange, propagate()).Times(0);
    EXPECT_CALL(yMin,    propagate()).Times(0);
    EXPECT_CALL(yMax,    propagate()).Times(0);

    x.clearMarks();
    x.mark(10);
    x.mark(15);
    EXPECT_FALSE(x.restrictToMarks());

    y.clearMarks();
    y.mark(0);
    y.mark(2);
    y.mark(16);
    EXPECT_FALSE(y.restrictToMarks());
}

/**
 * Check restrictToMarks()
 */
TEST_F(SolverSmallVarTest, RestrictFail2) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate()).Times(0);
    EXPECT_CALL(xChange, propagate());
    EXPECT_CALL(xMin,    propagate());
    EXPECT_CALL(xMax,    propagate());

    x.clearMarks();
    x.mark(2);
    x.mark(3);
    x.mark(4);
    EXPECT_TRUE(x.restrictToMarks());
    EXPECT_DOMAIN(x, 2, 3, 4);

    x.clearMarks();
    x.mark(0);
    x.mark(1);
    EXPECT_FALSE(x.restrictToMarks());
}

/**
 * Check restrictToMarks()
 */
TEST_F(SolverSmallVarTest, RestrictBind) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate());
    EXPECT_CALL(xChange, propagate());
    EXPECT_CALL(xMin,    propagate());
    EXPECT_CALL(xMax,    propagate());
    EXPECT_CALL(yBind,   propagate());
    EXPECT_CALL(yChange, propagate());
    EXPECT_CALL(yMin,    propagate());
    EXPECT_CALL(yMax,    propagate());

    x.clearMarks();
    x.mark(4);
    EXPECT_TRUE(x.restrictToMarks());
    EXPECT_DOMAIN(x, 4);

    y.clearMarks();
    y.mark(8);
    EXPECT_TRUE(y.restrictToMarks());
    EXPECT_DOMAIN(y, 8);
}

/**
 * Check restrictToMarks()
 */
TEST_F(SolverSmallVarTest, RestrictBindMin) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate());
    EXPECT_CALL(xChange, propagate());
    EXPECT_CALL(xMin,    propagate()).Times(0);
    EXPECT_CALL(xMax,    propagate());
    EXPECT_CALL(yBind,   propagate());
    EXPECT_CALL(yChange, propagate());
    EXPECT_CALL(yMin,    propagate()).Times(0);
    EXPECT_CALL(yMax,    propagate());

    x.clearMarks();
    x.mark(0);
    EXPECT_TRUE(x.restrictToMarks());
    EXPECT_DOMAIN(x, 0);

    y.clearMarks();
    y.mark(5);
    EXPECT_TRUE(y.restrictToMarks());
    EXPECT_DOMAIN(y, 5);
}

/**
 * Check restrictToMarks()
 */
TEST_F(SolverSmallVarTest, RestrictBindMax) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate());
    EXPECT_CALL(xChange, propagate());
    EXPECT_CALL(xMin,    propagate());
    EXPECT_CALL(xMax,    propagate()).Times(0);
    EXPECT_CALL(yBind,   propagate());
    EXPECT_CALL(yChange, propagate());
    EXPECT_CALL(yMin,    propagate());
    EXPECT_CALL(yMax,    propagate()).Times(0);

    x.clearMarks();
    x.mark(9);
    EXPECT_TRUE(x.restrictToMarks());
    EXPECT_DOMAIN(x, 9);

    y.clearMarks();
    y.mark(9);
    EXPECT_TRUE(y.restrictToMarks());
    EXPECT_DOMAIN(y, 9);
}

/**
 * Check updateMin()
 */
TEST_F(SolverSmallVarTest, UpdateMin) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate()).Times(0);
    EXPECT_CALL(xChange, propagate());
    EXPECT_CALL(xMin,    propagate());
    EXPECT_CALL(xMax,    propagate()).Times(0);
    EXPECT_CALL(yBind,   propagate()).Times(0);
    EXPECT_CALL(yChange, propagate());
    EXPECT_CALL(yMin,    propagate());
    EXPECT_CALL(yMax,    propagate()).Times(0);

    EXPECT_TRUE(x.updateMin(0));
    EXPECT_INITIAL_STATE;
    EXPECT_TRUE(y.updateMin(0));
    EXPECT_INITIAL_STATE;
    EXPECT_TRUE(y.updateMin(3));
    EXPECT_INITIAL_STATE;
    EXPECT_TRUE(y.updateMin(5));
    EXPECT_INITIAL_STATE;

    EXPECT_TRUE(x.updateMin(3));
    EXPECT_DOMAIN(x, 3, 4, 5, 6, 7, 8, 9);

    EXPECT_FALSE(x.updateMin(15));

    EXPECT_TRUE(y.updateMin(8));
    EXPECT_DOMAIN(y, 8, 9);

    EXPECT_FALSE(y.updateMin(16));
}

/**
 * Check updateMin()
 */
TEST_F(SolverSmallVarTest, UpdateMinBind) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate());
    EXPECT_CALL(xChange, propagate());
    EXPECT_CALL(xMin,    propagate());
    EXPECT_CALL(xMax,    propagate()).Times(0);
    EXPECT_CALL(yBind,   propagate());
    EXPECT_CALL(yChange, propagate());
    EXPECT_CALL(yMin,    propagate());
    EXPECT_CALL(yMax,    propagate()).Times(0);

    EXPECT_TRUE(x.updateMin(9));
    EXPECT_DOMAIN(x, 9);

    EXPECT_TRUE(y.updateMin(9));
    EXPECT_DOMAIN(y, 9);
}

/**
 * Check updateMax()
 */
TEST_F(SolverSmallVarTest, UpdateMax) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate()).Times(0);
    EXPECT_CALL(xChange, propagate());
    EXPECT_CALL(xMin,    propagate()).Times(0);
    EXPECT_CALL(xMax,    propagate());
    EXPECT_CALL(yBind,   propagate()).Times(0);
    EXPECT_CALL(yChange, propagate());
    EXPECT_CALL(yMin,    propagate()).Times(0);
    EXPECT_CALL(yMax,    propagate());

    EXPECT_TRUE(x.updateMax(15));
    EXPECT_INITIAL_STATE;
    EXPECT_TRUE(x.updateMax(9));
    EXPECT_INITIAL_STATE;
    EXPECT_TRUE(y.updateMax(10));
    EXPECT_INITIAL_STATE;
    EXPECT_TRUE(y.updateMax(9));
    EXPECT_INITIAL_STATE;

    EXPECT_TRUE(x.updateMax(7));
    EXPECT_DOMAIN(x, 0, 1, 2, 3, 4, 5, 6, 7);

    EXPECT_TRUE(y.updateMax(8));
    EXPECT_DOMAIN(y, 5, 6, 7, 8);

    EXPECT_FALSE(y.updateMax(3));
}

/**
 * Check updateMin()
 */
TEST_F(SolverSmallVarTest, UpdateMaxBind) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate());
    EXPECT_CALL(xChange, propagate());
    EXPECT_CALL(xMin,    propagate()).Times(0);
    EXPECT_CALL(xMax,    propagate());
    EXPECT_CALL(yBind,   propagate());
    EXPECT_CALL(yChange, propagate());
    EXPECT_CALL(yMin,    propagate()).Times(0);
    EXPECT_CALL(yMax,    propagate());

    EXPECT_TRUE(x.updateMax(0));
    EXPECT_DOMAIN(x, 0);

    EXPECT_TRUE(y.updateMax(5));
    EXPECT_DOMAIN(y, 5);
}
