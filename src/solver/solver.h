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
#include "trail.h"
#include "constraint.h"

#ifdef CASTOR_CSTR_TIMING
#include <unordered_map>
#include <typeinfo>
#include <typeindex>
#include <sys/time.h>
#include <sys/resource.h>
#endif

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
     * @return the trail
     */
    Trail& trail() { return trail_; }

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
    unsigned long statBacktracks() const { return statBacktracks_; }
    /**
     * @return the number of subtree activations so far
     */
    unsigned long statSubtrees()   const { return statSubtrees_;   }
    /**
     * @return number of times a constraint's post method has been called
     */
    unsigned long statPost()       const { return statPost_;       }
    /**
     * @return number of times a constraint's propagate method has been called
     */
    unsigned long statPropagate()  const { return statPropagate_;  }

#ifdef CASTOR_CSTR_TIMING
    /**
     * @return number of times the constraint's propagate or post method has
     *         been called, for each constraint type.
     */
    const std::unordered_map<std::type_index, unsigned long>&
    statCstrCount() const { return statCstrCount_; }
    /**
     * @return time in milliseconds spent in the constraint's propagate and post
     *         methods, for each constraint type.
     */
    const std::unordered_map<std::type_index, unsigned long>&
    statCstrTime() const { return statCstrTime_; }
#endif

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
     * Trail.
     */
    Trail trail_;

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
    unsigned long statBacktracks_;

    /**
     * Number of subtree activiations so far
     */
    unsigned long statSubtrees_;

    /**
     * Number of times a constraint's post method has been called
     */
    unsigned long statPost_;

    /**
     * Number of times a constraint's propagate method has been called
     */
    unsigned long statPropagate_;

#ifdef CASTOR_CSTR_TIMING
    /**
     * Number of times the constraint's propagate or post method has been
     * called, for each constraint type.
     */
    std::unordered_map<std::type_index, unsigned long> statCstrCount_;
    /**
     * Time in milliseconds spent in the constraint's propagate and post
     * methods, for each constraint type.
     */
    std::unordered_map<std::type_index, unsigned long> statCstrTime_;
    /**
     * Add timing information.
     *
     * @param c the constraint
     * @param start start time
     */
    void addTiming(Constraint* c, const rusage& start);
#endif

    friend class Subtree;
};

}
}

#endif // CASTOR_CP_SOLVER_H
