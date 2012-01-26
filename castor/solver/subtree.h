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
    Subtree(Solver *solver);

    /**
     * Destructor.
     */
    ~Subtree();

    /**
     * Add a variable. Make sure to add all variables that need to be
     * backtracked.
     *
     * The subtree does NOT take ownership of the variable.
     *
     * @note Should not be called once the tree has been activated once.
     *
     * @param x the variable
     * @param label true if x is a decision variable that should be labeled on
     *              search, false if x is an auxiliary variable
     */
    void add(Variable *x, bool label = false);

    /**
     * Add a constraint.
     * The subtree takes ownership of the constraint.
     *
     * @note Should not be called once the tree has been activated once.
     *
     * @param c the constraint
     */
    void add(Constraint *c);

    /**
     * @return whether this subtree is active
     */
    bool isActive() { return active; }

    /**
     * @return whether this subtree is the current subtree
     */
    bool isCurrent() { return solver->current == this; }

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
    void checkpoint(Variable *x);

    /**
     * Backtrack to the previous checkpoint. Remove the chosen value from the
     * chosen variable and propagate. If the propagation leads to a failure,
     * backtrack to an older checkpoint.
     *
     * @return the chosen variable of the last restored checkpoint
     *         or nullptr if the whole search tree has been explored
     */
    Variable* backtrack();

private:
    /**
     * Parent solver.
     */
    Solver *solver;

    /**
     * Is this subtree active?
     */
    bool active;

    /**
     * Previous active subtree
     */
    Subtree *previous;

    /**
     * Is this subtree inconsistent?
     */
    bool inconsistent;

    /**
     * Has the search begun?
     */
    bool started;

    /**
     * Variables.
     */
    std::vector<Variable*> vars;
    /**
     * Number of decision variables to label. The decision variables appear
     * first in the vars array.
     */
    int nbDecision;

    /**
     * Posted constraints.
     */
    std::vector<Constraint*> constraints[Constraint::PRIOR_COUNT];

    /**
     * Trail to backtrack.
     */
    Checkpoint *trail;

    /**
     * Current index in the trail array.
     */
    int trailIndex;
};

}
}

#endif // CASTOR_CP_SUBTREE_H
