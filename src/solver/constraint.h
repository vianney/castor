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
#ifndef CASTOR_CP_CONSTRAINT_H
#define CASTOR_CP_CONSTRAINT_H

#include "reversible.h"

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

    /**
     * Type of the timestamps used for static constraints. Defined to be at
     * least 64bits long.
     */
    typedef unsigned long long timestamp_t;

    /**
     * Create a new constraint.
     * The constraint will be marked as "propagating" (nextPropag_ = nullptr)
     * and not entailed (done_ = false).
     *
     * @param solver the solver
     * @param priority the priority of the constraint
     */
    Constraint(Solver* solver, Priority priority = PRIOR_MEDIUM);
    virtual ~Constraint() {}

    //! Non-copyable
    Constraint(const Constraint&) = delete;
    Constraint& operator=(const Constraint&) = delete;

    /**
     * Priority of this constraint.
     */
    Priority priority() { return priority_; }

    /**
     * Constraint (re)initialization. Called when parent subtree is activated
     * and before any propagation occurs. Should not propagate anything.
     */
    virtual void init() {}

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

protected:
    /**
     * Solver containing this constraint.
     */
    Solver* solver_;

    /**
     * Parent subtree in which this constraint is posted or nullptr if posted
     * globally.
     * This variable is initialized by Subtree or Solver.
     */
    Subtree* parent_;

    /**
     * If this variable is set to true, the constraint will not react to further
     * events.
     */
    Reversible<bool> done_;

private:
    /**
     * Constraint priority. The priority is constant for a constraint.
     */
    Priority priority_;
    /**
     * Internal use by solver.
     * Next constraint in propagation queue.
     */
    Constraint* nextPropag_;
    /**
     * Timestamp of this constraint. This is only used for static constraints.
     */
    timestamp_t timestamp_;

    friend class Solver;
    friend class Subtree;
};

static inline Constraint::Priority& operator++(Constraint::Priority& p) {
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
public:
    StatelessConstraint(Solver* solver) : Constraint(solver) {}
    StatelessConstraint(Solver* solver, Priority priority) :
        Constraint(solver, priority) {}

    void init() { Constraint::init(); posted_ = false; }
    bool post() { return posted_ ? true : propagate(); }
    bool propagate() { posted_ = true; return true; }

private:
    bool posted_; //!< true if initial propagation has been performed
};

}
}

#endif // CASTOR_CP_CONSTRAINT_H
