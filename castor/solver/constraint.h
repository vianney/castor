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
#ifndef CASTOR_CP_CONSTRAINT_H
#define CASTOR_CP_CONSTRAINT_H

namespace castor {
namespace cp {

class Solver;
class Subtree;

/**
 * Constraint base class
 */
class Constraint {
public:
    /**
     * Constraint propagation priority
     */
    enum Priority {
        /**
         * High priority, will be propagated first. Use this for value-based
         * constraints or very quick constraints.
         */
        PRIOR_HIGH,
        /**
         * Medium priority. Use this for bound consistent constraints.
         */
        PRIOR_MEDIUM,
        /**
         * Low priority. Use this for heavy constraints that should be called after
         * as many values as possible have been removed.
         */
        PRIOR_LOW,

        PRIOR_FIRST = PRIOR_HIGH,
        PRIOR_LAST = PRIOR_LOW
    };
    static const int PRIOR_COUNT = PRIOR_LAST - PRIOR_FIRST + 1;

    Constraint(Priority priority = PRIOR_MEDIUM) :
            priority(priority) {}
    virtual ~Constraint() {}

    /**
     * Priority of this constraint.
     */
    Priority getPriority() { return priority; }

    /**
     * Constraint (re)initialization. Called when parent subtree is activated
     * and before any propagation occurs. Should not propagate anything.
     */
    virtual void init() { done = false; }

    /**
     * Initial propagation callback. It should perform the initial propagation
     * and return true if all went well or false if the propagation failed.
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
     * Solver containing this constraint.
     */
    Solver *solver;

    /**
     * Parent subtree in which this constraint is posted or nullptr if posted
     * globally.
     * This variable is initialized by Subtree or Solver.
     */
    Subtree *parent;

    /**
     * If this variable is set to true, the constraint will not react to further
     * events. The restore callback will still be called, such that done can
     * be reset to false when appropriate.
     */
    bool done;

private:
    /**
     * Constraint priority. The priority is constant for a constraint.
     */
    Priority priority;
    /**
     * Internal use by solver.
     * Next constraint in propagation queue.
     */
    Constraint *nextPropag;
    /**
     * Timestamp of this constraint. This is only used for static constraints.
     */
    int timestamp;

    friend class Solver;
    friend class Subtree;
};

static inline Constraint::Priority& operator++(Constraint::Priority &p) {
    return p = static_cast<Constraint::Priority>(p + 1);
}

/**
 * A stateless constraint does not do anything in his post method apart from
 * calling propagate. As such, it can react to variable events before being
 * "posted".
 *
 * @note Implementations should not forget to call the parent class
 *       implementation when overriding methods.
 */
class StatelessConstraint : public Constraint {
    bool posted; //!< true if initial propagation has been performed
public:
    StatelessConstraint() : Constraint() {}
    StatelessConstraint(Priority priority) : Constraint(priority) {}

    void init() { Constraint::init(); posted = false; }
    bool post() { return posted ? true : propagate(); }
    bool propagate() { posted = true; return true; }
};

}
}

#include "solver.h"

#endif // CASTOR_CP_CONSTRAINT_H
