/* This file is part of Castor
 *
 * Author: Vianney le Clément de Saint-Marcq <vianney.leclement@uclouvain.be>
 * Copyright (C) 2010-2011, Université catholique de Louvain
 *
 * Castor is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * Castor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Castor; if not, see <http://www.gnu.org/licenses/>.
 */
#ifndef CASTOR_CONSTRAINT_H
#define CASTOR_CONSTRAINT_H

namespace castor {

class Solver;
class Subtree;

/**
 * Constraint propagation priority
 */
enum ConstraintPriority {
    /**
     * High priority, will be propagated first. Use this for value-based
     * constraints or very quick constraints.
     */
    CSTR_PRIOR_HIGH,
    /**
     * Medium priority. Use this for bound consistent constraints.
     */
    CSTR_PRIOR_MEDIUM,
    /**
     * Low priority. Use this for heavy constraints that should be called after
     * as many values as possible have been removed.
     */
    CSTR_PRIOR_LOW,

    CSTR_PRIOR_FIRST = CSTR_PRIOR_HIGH,
    CSTR_PRIOR_LAST = CSTR_PRIOR_LOW
};
#define CSTR_PRIOR_COUNT (CSTR_PRIOR_LAST - CSTR_PRIOR_FIRST + 1)
inline ConstraintPriority& operator++(ConstraintPriority &p) {
    return p = (ConstraintPriority)(p + 1);
}

/**
 * Constraint base class
 */
class Constraint {
public:
    Constraint() : priority(CSTR_PRIOR_MEDIUM) {}
    Constraint(ConstraintPriority priority) : priority(priority) {}
    virtual ~Constraint() {}

    /**
     * Priority of this constraint.
     */
    ConstraintPriority getPriority() { return priority; }

    /**
     * Initial propagation callback. It is called during solver_post. It should
     * perform the initial propagation and return true if all went well or false
     * if the propagation failed.
     */
    virtual bool post() { return propagate(); }

    /**
     * Propagate callback. It is called when an event this contraint has
     * registered to, has been triggered. It should propagate this event and
     * return true if all went well or false if the propagation failed.
     */
    virtual bool propagate() { return true; }

    /**
     * Restore callback. It is called after a backtrack.
     * This is useful for resetting structures.
     */
    virtual void restore() {}

protected:
    /**
     * Parent subtree in which this constraint is posted.
     * This variable is initialized by the Subtree.
     */
    Subtree *parent;

private:
    /**
     * Constraint priority. The priority is constant for a constraint.
     */
    ConstraintPriority priority;
    /**
     * Internal use by solver.
     * Next constraint in propagation queue.
     */
    Constraint *nextPropag;

    friend class Solver;
    friend class Subtree;
};

}

#include "solver.h"

#endif // CASTOR_CONSTRAINT_H
