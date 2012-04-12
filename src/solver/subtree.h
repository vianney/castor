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
#ifndef CASTOR_CP_SUBTREE_H
#define CASTOR_CP_SUBTREE_H

#include <vector>

#include "solver.h"
#include "variable.h"
#include "constraint.h"

namespace castor {
namespace cp {

class Checkpoint;

/**
 * Search subtree containing the posted constraints
 */
class Subtree {
public:
    /**
     * Initialize a subtree.
     *
     * @param solver the solver
     */
    Subtree(Solver* solver);

    /**
     * Destructor.
     */
    ~Subtree();

    //! Non-copyable
    Subtree(const Subtree&) = delete;
    Subtree& operator=(const Subtree&) = delete;

    /**
     * Add a trailable object. Make sure to add all trailable objects that need
     * to be backtracked.
     *
     * The subtree does NOT take ownership of the object.
     *
     * @note Should not be called once the tree has been activated once.
     *
     * @param x the trailable object
     */
    void add(Trailable* x);

    /**
     * Add a decision variable. Make sure to add all variables that need to be
     * backtracked.
     *
     * The subtree does NOT take ownership of the variable.
     *
     * @note Should not be called once the tree has been activated once.
     *
     * @param x the variable
     * @param label true if x should be labeled during the search, false if x
     *              is an auxiliary variable
     */
    void add(DecisionVariable* x, bool label = false);

    /**
     * Add a constraint.
     * The subtree takes ownership of the constraint.
     *
     * @note Should not be called once the tree has been activated once.
     *
     * @param c the constraint
     */
    void add(Constraint* c);

    /**
     * @return whether this subtree is active
     */
    bool isActive() { return active_; }

    /**
     * @return whether this subtree is the current subtree
     */
    bool isCurrent() { return solver_->current_ == this; }

    /**
     * Activate this subtree.
     */
    void activate();

    /**
     * Discard this subtree.
     */
    void discard();

    /**
     * Search for the next solution in the current subtree. No constraint may be
     * added once the search has begun in the subtree. When a subtree is
     * done (this function returns false), it is automatically discarded.
     *
     * @return true if a solution has been found, false if the subtree is done
     */
    bool search();

private:
    /**
     * Create a checkpoint.
     *
     * @param x chosen variable
     */
    void checkpoint(DecisionVariable* x);

    /**
     * Backtrack to the previous checkpoint. Remove the chosen value from the
     * chosen variable and propagate. If the propagation leads to a failure,
     * backtrack to an older checkpoint.
     *
     * @return the chosen variable of the last restored checkpoint
     *         or nullptr if the whole search tree has been explored
     */
    DecisionVariable* backtrack();

private:
    /**
     * Parent solver.
     */
    Solver* solver_;

    /**
     * Is this subtree active?
     */
    bool active_;

    /**
     * Previous active subtree
     */
    Subtree* previous_;

    /**
     * Is this subtree inconsistent?
     */
    bool inconsistent_;

    /**
     * Has the search begun?
     */
    bool started_;

    /**
     * Trailable objects to backtrack.
     */
    std::vector<Trailable*> trailables_;
    /**
     * Decision variables. Subset of trailables_.
     */
    std::vector<DecisionVariable*> vars_;

    /**
     * Posted constraints.
     */
    std::vector<Constraint*> constraints_[Constraint::PRIOR_COUNT];

    /**
     * Trail to backtrack.
     */
    Checkpoint* trail_;

    /**
     * Current index in the trail array.
     */
    int trailIndex_;
};

}
}

#endif // CASTOR_CP_SUBTREE_H
