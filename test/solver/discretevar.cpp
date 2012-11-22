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
#include "solver/discretevar.h"
#include "solver_mock.h"
#include "constraint_mock.h"
#include "gtest/gtest.h"

using namespace castor::cp;
using ::testing::AtMost;
using ::testing::Between;

class SolverDiscreteVarTest : public ::testing::Test {
protected:
    typedef DiscreteVariable<unsigned> Var;
    MockSolver solver;
    Var x, y;
    MockConstraint xBind, xChange, xMin, xMax, yBind, yChange, yMin, yMax;

    //! Upper bound on the values in any domain, will never appear in any of them.
    static const unsigned MAXVAL = 20;

    SolverDiscreteVarTest() : x(&solver, 0, 9), y(&solver, 5, 9) {}

    virtual void SetUp() {
        expect_initial_state();
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
    }

    /**
     * Expect variable x to have the domain dom (with n values), ignoring the
     * ordering. Bounds are not checked.
     *
     * @param x the variable to check
     * @param dom array of the domain values (dom.size() > 0)
     * @param min lower bound (if unsynchronized)
     * @param max upper bound (if unsynchronized)
     */
    void expect_domain(const Var& x, std::initializer_list<unsigned> dom,
                       unsigned min=0, unsigned max=MAXVAL) {
        ASSERT_LT(0u, dom.size());
        // check size and bound
        EXPECT_EQ(dom.size(), x.size());
        if(dom.size() == 1) {
            EXPECT_TRUE(x.bound());
            EXPECT_EQ(*dom.begin(), x.value());
            EXPECT_EQ(*dom.begin(), x.min());
            EXPECT_EQ(*dom.begin(), x.max());
        } else {
            EXPECT_FALSE(x.bound());
        }
        // check x.contains()
        for(unsigned v = 0; v <= MAXVAL; v++) {
            bool found = false;
            if(v >= min && v <= max) {
                for(unsigned v2 : dom) {
                    if(v2 == v) {
                        found = true;
                        break;
                    }
                }
            }
            if(found)
                EXPECT_TRUE(x.contains(v)) << "with value " << v;
            else
                EXPECT_FALSE(x.contains(v)) << "with value " << v;
        }
        // every value in x.domain() should appear in dom
        for(unsigned i = 0; i < dom.size(); i++) {
            unsigned v = x.domain()[i];
            bool found = false;
            for(unsigned v2 : dom) {
                if(v2 == v) {
                    found = true;
                    break;
                }
            }
            EXPECT_TRUE(found) << "Value " << v << " at index " << i
                               << " of x.domain() should not be there";
        }
        // every value in dom should appear in x.domain()
        for(unsigned v : dom) {
            bool found = false;
            for(unsigned i = 0; i < dom.size(); i++) {
                if(x.domain()[i] == v) {
                    found = true;
                    break;
                }
            }
            EXPECT_TRUE(found) << "Value " << v
                               << " is missing from x.domain()";
        }
    }

    /**
     * Check that the variables are left in their initial state
     */
    void expect_initial_state() {
        expect_domain(x, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
        EXPECT_EQ( 0u, x.min());
        EXPECT_EQ( 9u, x.max());

        expect_domain(y, {5, 6, 7, 8, 9});
        EXPECT_EQ(5u, y.min());
        EXPECT_EQ(9u, y.max());
    }
};

/**
 * save() should not modify the domain
 */
TEST_F(SolverDiscreteVarTest, SaveSanity) {
    x.save(solver.trail());
    y.save(solver.trail());
    expect_initial_state();
}

/**
 * restore() should restore the domain to the state of a checkpoint
 */
TEST_F(SolverDiscreteVarTest, Restore) {
    Trail::checkpoint_t chkp = solver.trail().checkpoint();
    x.remove(8);
    solver.trail().restore(chkp);
    expect_initial_state();

    chkp = solver.trail().checkpoint();
    y.updateMin(7);
    solver.trail().restore(chkp);
    expect_initial_state();
}

/**
 * Check the select() method
 */
TEST_F(SolverDiscreteVarTest, Label) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate());
    EXPECT_CALL(xChange, propagate());
    EXPECT_CALL(xMin,    propagate()).Times(AtMost(1));
    EXPECT_CALL(xMax,    propagate()).Times(AtMost(1));
    EXPECT_CALL(yBind,   propagate());
    EXPECT_CALL(yChange, propagate());
    EXPECT_CALL(yMin,    propagate()).Times(AtMost(1));
    EXPECT_CALL(yMax,    propagate()).Times(AtMost(1));

    x.label();
    EXPECT_TRUE(x.bound());
    EXPECT_LE(0u, x.value());
    EXPECT_GE(9u, x.value());
    expect_domain(x, {x.value()});

    y.label();
    EXPECT_TRUE(y.bound());
    EXPECT_LE(5u, y.value());
    EXPECT_GE(9u, y.value());
    expect_domain(y, {y.value()});
}

/**
 * Check the unselect() method
 */
TEST_F(SolverDiscreteVarTest, UnLabel) {
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
 * Marking values should not modify the domain
 */
TEST_F(SolverDiscreteVarTest, MarkSanity) {
    x.clearMarks();
    expect_initial_state();
    x.mark(x.domain()[0]);
    expect_initial_state();
    x.mark(4);
    expect_initial_state();;
    x.mark(16);
    expect_initial_state();
    x.mark(3);
    expect_initial_state();
    x.clearMarks();
    expect_initial_state();

    y.clearMarks();
    expect_initial_state();
    y.mark(y.domain()[0]);
    expect_initial_state();
    y.mark(8);
    expect_initial_state();;
    y.mark(16);
    expect_initial_state();
    y.mark(4);
    expect_initial_state();
    y.clearMarks();
    expect_initial_state();
}

/**
 * Check the bind() method
 */
TEST_F(SolverDiscreteVarTest, Bind) {
    registerConstraints();
    EXPECT_CALL(xBind, propagate());
    EXPECT_CALL(xChange, propagate());
    EXPECT_CALL(xMin, propagate());
    EXPECT_CALL(xMax, propagate());
    EXPECT_CALL(yBind, propagate());
    EXPECT_CALL(yChange, propagate());
    EXPECT_CALL(yMin, propagate());
    EXPECT_CALL(yMax, propagate());

    EXPECT_TRUE(x.bind(5));
    expect_domain(x, {5});

    EXPECT_TRUE(x.bind(5));
    EXPECT_FALSE(x.bind(6));

    EXPECT_TRUE(y.bind(7));
    expect_domain(y, {7});

    EXPECT_TRUE(y.bind(7));
    EXPECT_FALSE(y.bind(6));
}

/**
 * Check the bind() method when binding to the minimum value
 */
TEST_F(SolverDiscreteVarTest, BindMin) {
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
    expect_domain(x, {0});

    EXPECT_TRUE(x.bind(0));
    EXPECT_FALSE(x.bind(6));

    EXPECT_TRUE(y.bind(5));
    expect_domain(y, {5});

    EXPECT_TRUE(y.bind(5));
    EXPECT_FALSE(y.bind(6));
}


/**
 * Check the bind() method when binding to the maximum value
 */
TEST_F(SolverDiscreteVarTest, BindMax) {
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
    expect_domain(x, {9});

    EXPECT_TRUE(x.bind(9));
    EXPECT_FALSE(x.bind(6));

    EXPECT_TRUE(y.bind(9));
    expect_domain(y, {9});

    EXPECT_TRUE(y.bind(9));
    EXPECT_FALSE(y.bind(6));
}

/**
 * bind() to a value outside the domain should fail
 */
TEST_F(SolverDiscreteVarTest, BindFail) {
    EXPECT_FALSE(x.bind(18));
    EXPECT_FALSE(y.bind(3));
}

/**
 * Check remove()
 */
TEST_F(SolverDiscreteVarTest, Remove) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate()).Times(0);
    EXPECT_CALL(xChange, propagate());
    EXPECT_CALL(xMin,    propagate()).Times(0);
    EXPECT_CALL(xMax,    propagate()).Times(0);
    EXPECT_CALL(yBind,   propagate()).Times(0);
    EXPECT_CALL(yChange, propagate());
    EXPECT_CALL(yMin,    propagate()).Times(0);
    EXPECT_CALL(yMax,    propagate()).Times(0);

    EXPECT_TRUE(x.remove(6));
    expect_domain(x, {0, 1, 2, 3, 4, 5, /*6,*/ 7, 8, 9});

    EXPECT_TRUE(y.remove(7));
    expect_domain(y, {5, 6, /*7,*/ 8, 9});
}

/**
 * Check remove() when removing minimum value
 */
TEST_F(SolverDiscreteVarTest, RemoveMin) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate()).Times(0);
    EXPECT_CALL(xChange, propagate());
    EXPECT_CALL(xMin,    propagate()).Times(AtMost(1));
    EXPECT_CALL(xMax,    propagate()).Times(0);
    EXPECT_CALL(yBind,   propagate()).Times(0);
    EXPECT_CALL(yChange, propagate());
    EXPECT_CALL(yMin,    propagate()).Times(AtMost(1));
    EXPECT_CALL(yMax,    propagate()).Times(0);

    EXPECT_TRUE(x.remove(0));
    expect_domain(x, {/*0,*/ 1, 2, 3, 4, 5, 6, 7, 8, 9});

    EXPECT_TRUE(y.remove(5));
    expect_domain(y, {/*5,*/ 6, 7, 8, 9});
}

/**
 * Check remove() when removing maximum value
 */
TEST_F(SolverDiscreteVarTest, RemoveMax) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate()).Times(0);
    EXPECT_CALL(xChange, propagate());
    EXPECT_CALL(xMin,    propagate()).Times(0);
    EXPECT_CALL(xMax,    propagate()).Times(AtMost(1));
    EXPECT_CALL(yBind,   propagate()).Times(0);
    EXPECT_CALL(yChange, propagate());
    EXPECT_CALL(yMin,    propagate()).Times(0);
    EXPECT_CALL(yMax,    propagate()).Times(AtMost(1));

    EXPECT_TRUE(x.remove(9));
    expect_domain(x, {0, 1, 2, 3, 4, 5, 6, 7, 8/*, 9*/});

    EXPECT_TRUE(y.remove(9));
    expect_domain(y, {5, 6, 7, 8/*, 9*/});
}

/**
 * Check remove() when removing all values but one (should generate a bind
 * event)
 */
TEST_F(SolverDiscreteVarTest, RemoveAllButOne) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate());
    EXPECT_CALL(xChange, propagate()).Times(9);
    EXPECT_CALL(xMin,    propagate());
    EXPECT_CALL(xMax,    propagate()).Times(Between(1, 2));
    EXPECT_CALL(yBind,   propagate());
    EXPECT_CALL(yChange, propagate()).Times(4);
    EXPECT_CALL(yMin,    propagate()).Times(Between(1, 2));
    EXPECT_CALL(yMax,    propagate());

    for(unsigned v = 2; v <= 9; v++)
        EXPECT_TRUE(x.remove(v)) << "with value " << v;
    EXPECT_TRUE(x.remove(0));
    expect_domain(x, {1});

    EXPECT_TRUE(y.remove(6));
    EXPECT_TRUE(y.remove(8));
    EXPECT_TRUE(y.remove(5));
    EXPECT_TRUE(y.remove(9));
    expect_domain(y, {7});
}

/**
 * Check remove() when removing a value triggers a bind because there is only
 * one value left in the representations' intersection.
 */
TEST_F(SolverDiscreteVarTest, RemoveSyncBind) {
    Var z(&solver, 10, 12);
    z.updateMax(11);
    expect_domain(z, {10, 11, 12}, 10, 11);
    z.remove(10);
    if(z.min() == z.max())
        expect_domain(z, {11});
}

/**
 * Check restrictToMarks()
 */
TEST_F(SolverDiscreteVarTest, Restrict) {
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
    expect_domain(x, {2, 4, 7});
    EXPECT_EQ(2u, x.min());
    EXPECT_EQ(7u, x.max());

    y.clearMarks();
    y.mark(0);
    y.mark(8);
    y.mark(6);
    EXPECT_TRUE(y.restrictToMarks());
    expect_domain(y, {6, 8});
    EXPECT_EQ(6u, y.min());
    EXPECT_EQ(8u, y.max());
}

/**
 * Check restrictToMarks()
 */
TEST_F(SolverDiscreteVarTest, RestrictMin) {
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
    expect_domain(x, {0, 2, 4, 7});
    EXPECT_EQ(0u, x.min());
    EXPECT_EQ(7u, x.max());

    y.clearMarks();
    y.mark(5);
    y.mark(0);
    y.mark(8);
    y.mark(6);
    EXPECT_TRUE(y.restrictToMarks());
    expect_domain(y, {5, 6, 8});
    EXPECT_EQ(5u, y.min());
    EXPECT_EQ(8u, y.max());
}

/**
 * Check restrictToMarks()
 */
TEST_F(SolverDiscreteVarTest, RestrictMax) {
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
    expect_domain(x, {2, 4, 7, 9});
    EXPECT_EQ(2u, x.min());
    EXPECT_EQ(9u, x.max());

    y.clearMarks();
    y.mark(0);
    y.mark(9);
    y.mark(8);
    y.mark(6);
    EXPECT_TRUE(y.restrictToMarks());
    expect_domain(y, {6, 8, 9});
    EXPECT_EQ(6u, y.min());
    EXPECT_EQ(9u, y.max());
}

/**
 * Check restrictToMarks()
 */
TEST_F(SolverDiscreteVarTest, RestrictNoOp) {
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
    expect_initial_state();

    y.clearMarks();
    y.mark(6);
    y.mark(5);
    y.mark(8);
    y.mark(9);
    y.mark(7);
    EXPECT_TRUE(y.restrictToMarks());
    expect_initial_state();
}

/**
 * Check restrictToMarks()
 */
TEST_F(SolverDiscreteVarTest, RestrictFail) {
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
TEST_F(SolverDiscreteVarTest, RestrictFail2) {
    registerConstraints();
    EXPECT_CALL(xBind,   propagate()).Times(0);
    EXPECT_CALL(xChange, propagate());
    EXPECT_CALL(xMin,    propagate());
    EXPECT_CALL(xMax,    propagate());

    x.clearMarks();
    x.mark(2);
    x.mark(3);
    x.mark(4);
    x.restrictToMarks();
    expect_domain(x, {2, 3, 4});

    x.clearMarks();
    x.mark(0);
    x.mark(1);
    EXPECT_FALSE(x.restrictToMarks());
}

/**
 * Check restrictToMarks()
 */
TEST_F(SolverDiscreteVarTest, RestrictBind) {
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
    expect_domain(x, {4});

    y.clearMarks();
    y.mark(8);
    EXPECT_TRUE(y.restrictToMarks());
    expect_domain(y, {8});
}

/**
 * Check restrictToMarks()
 */
TEST_F(SolverDiscreteVarTest, RestrictBindMin) {
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
    expect_domain(x, {0});

    y.clearMarks();
    y.mark(5);
    EXPECT_TRUE(y.restrictToMarks());
    expect_domain(y, {5});
}

/**
 * Check restrictToMarks()
 */
TEST_F(SolverDiscreteVarTest, RestrictBindMax) {
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
    expect_domain(x, {9});

    y.clearMarks();
    y.mark(9);
    EXPECT_TRUE(y.restrictToMarks());
    expect_domain(y, {9});
}

/**
 * Check updateMin()
 */
TEST_F(SolverDiscreteVarTest, UpdateMin) {
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
    expect_initial_state();
    EXPECT_TRUE(y.updateMin(0));
    expect_initial_state();
    EXPECT_TRUE(y.updateMin(3));
    expect_initial_state();
    EXPECT_TRUE(y.updateMin(5));
    expect_initial_state();

    EXPECT_TRUE(x.updateMin(3));
    expect_domain(x, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, 3, 9); // no sync
    EXPECT_EQ(3u, x.min());
    EXPECT_EQ(9u, x.max());

    EXPECT_FALSE(x.updateMin(15));

    EXPECT_TRUE(y.updateMin(8));
    expect_domain(y, {5, 6, 7, 8, 9}, 8, 9); // no sync
    EXPECT_EQ(8u, y.min());
    EXPECT_EQ(9u, y.max());

    EXPECT_FALSE(y.updateMin(16));
}

/**
 * Check updateMin()
 */
TEST_F(SolverDiscreteVarTest, UpdateMinBind) {
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
    expect_domain(x, {9});

    EXPECT_TRUE(y.updateMin(9));
    expect_domain(y, {9});
}

/**
 * Check updateMax()
 */
TEST_F(SolverDiscreteVarTest, UpdateMax) {
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
    expect_initial_state();
    EXPECT_TRUE(x.updateMax(9));
    expect_initial_state();
    EXPECT_TRUE(y.updateMax(10));
    expect_initial_state();
    EXPECT_TRUE(y.updateMax(9));
    expect_initial_state();

    EXPECT_TRUE(x.updateMax(7));
    expect_domain(x, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, 0, 7); // no sync
    EXPECT_EQ(0u, x.min());
    EXPECT_EQ(7u, x.max());

    EXPECT_TRUE(y.updateMax(8));
    expect_domain(y, {5, 6, 7, 8, 9}, 5, 8); // no sync
    EXPECT_EQ(5u, y.min());
    EXPECT_EQ(8u, y.max());

    EXPECT_FALSE(y.updateMax(3));
}

/**
 * Check updateMin()
 */
TEST_F(SolverDiscreteVarTest, UpdateMaxBind) {
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
    expect_domain(x, {0});

    EXPECT_TRUE(y.updateMax(5));
    expect_domain(y, {5});
}
