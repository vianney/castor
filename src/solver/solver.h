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
#ifndef CASTOR_CP_SOLVER_H
#define CASTOR_CP_SOLVER_H

#include <vector>

#include "config.h"
#include "constraint.h"

namespace castor {
namespace cp {

class Subtree;

/**
 * Main solver class
 */
class Solver {
public:
    /**
     * Initialize a new solver.
     */
    Solver();

    MOCKABLE ~Solver();

    //! Non-copyable
    Solver(const Solver&) = delete;
    Solver& operator=(const Solver&) = delete;

    /**
     * Adds a static constraint.
     * The solver takes ownership of the constraint.
     *
     * @param c the constraint
     */
    MOCKABLE void add(Constraint* c);

    /**
     * Indicates a static constraint has been updated and should be
     * reposted.
     *
     * @param c the constraint
     */
    MOCKABLE void refresh(Constraint* c);

    /**
     * Enqueue constraints for propagation. Only variable should call this
     * method.
     *
     * @param constraints the list of constraints
     */
    MOCKABLE void enqueue(std::vector<Constraint*>& constraints_);

    /**
     * @return the number of backtracks so far
     */
    int statBacktracks() const { return statBacktracks_; }
    /**
     * @return the number of subtree activations so far
     */
    int statSubtrees()   const { return statSubtrees_;   }
    /**
     * @return number of times a constraint's post method has been called
     */
    int statPost()       const { return statPost_;       }
    /**
     * @return number of times a constraint's propagate method has been called
     */
    int statPropagate()  const { return statPropagate_;  }

private: // for subtree
    /**
     * Post all static constraints whose timestamp is greater than tsCurrent.
     *
     * @return false if there is a failure, true otherwise
     */
    MOCKABLE bool postStatic();

    /**
     * Post a list of constraints. Perform initial propagation on all the
     * constraints.
     *
     * @param array of constraint lists by priority
     * @return false if there is a failure, true otherwise
     */
    MOCKABLE bool post(std::vector<Constraint*>* constraints);

    /**
     * Perform propagation of the constraints in the queue. After this call,
     * either the queue is empty and we have reached the fixpoint, or a failure
     * has been detected.
     *
     * @return false if there is a failure, true otherwise
     */
    MOCKABLE bool propagate();

    /**
     * Clear the propagation queue.
     */
    MOCKABLE void clearQueue();

private:
    /**
     * Dummy non-null pointer to indicate a constraint is not currently queued
     * for propagation.
     * This value is used in the nextPropag field of Constraint. We use the
     * address of the solver object, so we are sure no Constraint object will
     * have this address.
     *
     * @return a dummy non-null pointer to a non-existing Constraint
     */
    Constraint* unqueued() {
        return reinterpret_cast<Constraint*>(this);
    }

    /**
     * Propagation stack (linked list using nextPropag field)
     */
    Constraint* propagQueue_[Constraint::PRIOR_COUNT];

    /**
     * Current active subtree.
     */
    Subtree* current_;

    /**
     * Static constraints.
     */
    std::vector<Constraint*> constraints_;

    /**
     * Timestamp of the current state of the domains (used for static
     * constraints).
     */
    Constraint::timestamp_t tsCurrent_;
    /**
     * Timestamp of the latest added/refreshed static constraint.
     */
    Constraint::timestamp_t tsLastConstraint_;

    /**
     * Number of backtracks so far
     */
    int statBacktracks_;

    /**
     * Number of subtree activiations so far
     */
    int statSubtrees_;

    /**
     * Number of times a constraint's post method has been called
     */
    int statPost_;

    /**
     * Number of times a constraint's propagate method has been called
     */
    int statPropagate_;

    friend class Subtree;
};

}
}

#endif // CASTOR_CP_SOLVER_H
