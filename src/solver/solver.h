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

    /**
     * Adds a static constraint.
     * The solver takes ownership of the constraint.
     *
     * @param c the constraint
     */
    MOCKABLE void add(Constraint *c);

    /**
     * Indicates a static constraint has been updated and should be
     * reposted.
     *
     * @param c the constraint
     */
    MOCKABLE void refresh(Constraint *c);

    /**
     * Enqueue constraints for propagation. Only variable should call this
     * method.
     *
     * @param constraints the list of constraints
     */
    MOCKABLE void enqueue(std::vector<Constraint*> &constraints);

    /**
     * @return the current subtree
     */
    Subtree* getCurrent() { return current; }

    /**
     * @return the number of backtracks so far
     */
    int getStatBacktracks() { return statBacktracks; }
    /**
     * @return the number of subtree activations so far
     */
    int getStatSubtrees() { return statSubtrees; }
    /**
     * @return number of times a constraint's post method has been called
     */
    int getStatPost() { return statPost; }
    /**
     * @return number of times a constraint's propagate method has been called
     */
    int getStatPropagate() { return statPropagate; }

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
    MOCKABLE bool post(std::vector<Constraint*> *constraints);

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
     * Propagation stack (linked list using nextPropag field)
     */
    Constraint *propagQueue[Constraint::PRIOR_COUNT];

    /**
     * Current active subtree.
     */
    Subtree *current;

    /**
     * Static constraints.
     */
    std::vector<Constraint*> constraints;

    /**
     * Timestamps for static constraints.
     * tsLastConstraint is the timestamp of the latest added/refreshed
     * constraint. tsCurrent represents the current state of the domains.
     */
    int tsCurrent, tsLastConstraint;

    /**
     * Number of backtracks so far
     */
    int statBacktracks;

    /**
     * Number of subtree activiations so far
     */
    int statSubtrees;

    /**
     * Number of times a constraint's post method has been called
     */
    int statPost;

    /**
     * Number of times a constraint's propagate method has been called
     */
    int statPropagate;

    friend class Subtree;
};

}
}

#include "subtree.h"

#endif // CASTOR_CP_SOLVER_H
