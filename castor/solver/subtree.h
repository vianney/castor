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
#ifndef CASTOR_SUBTREE_H
#define CASTOR_SUBTREE_H

#include <vector>

#include "solver.h"
#include "varint.h"
#include "constraint.h"

namespace castor {

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
     * @param vars array of variables to label in the subtree
     * @param nbVars number of variables to label
     */
    Subtree(Solver *solver, VarInt **vars, int nbVars);

    /**
     * Destructor.
     */
    ~Subtree();

    /**
     * Add a constraint.
     * The subtree takes ownership of the constraint.
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
     * @param v chosen value
     */
    void checkpoint(VarInt *x, int v);

    /**
     * Backtrack to the previous checkpoint. Remove the chosen value from the
     * chosen variable and propagate. If the propagation leads to a failure,
     * backtrack to an older checkpoint.
     *
     * @return the chosen variable of the last restored checkpoint
     *         or null if the whole search tree has been explored
     */
    VarInt* backtrack();

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
     * Variables to label.
     */
    VarInt **vars;
    /**
     * Number of variables to label.
     */
    int nbVars;

    /**
     * Posted constraints.
     */
    std::vector<Constraint*> constraints;

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

#endif // CASTOR_SUBTREE_H
