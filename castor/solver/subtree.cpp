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

#include <cstdlib>
#include "subtree.h"

namespace castor {

/**
 * Checkpoint structure for backtracking
 */
struct Checkpoint {
    /**
     * Size of the domains.
     */
    int *domSizes;
    /**
     * Lower bounds
     */
    int *min;
    /**
     * Upper bounds
     */
    int *max;
    /**
     * Timestamp of static constraints
     */
    int timestamp;
    /**
     * Variable that has been chosen.
     */
    VarInt *x;
    /**
     * Value that has been bound to the chosen variable just after the
     * checkpoint.
     */
    int v;

    ~Checkpoint() {
        delete [] domSizes;
        delete [] min;
        delete [] max;
    }
};

Subtree::Subtree(Solver *solver, VarInt **vars, int nbVars,
                 int nbDecision, int nbOrder) :
        solver(solver),
        vars(vars),
        nbVars(nbVars),
        nbDecision(nbDecision),
        nbOrder(nbOrder) {
    if(nbDecision == 0)
        this->nbDecision = nbVars;
    // The depth of the subtree is at most vars.size(). Allocate trail.
    // +1 for the root checkpoint
    trail = new Checkpoint[nbVars + 1];
    for(int i = 0; i <= nbVars; i++) {
        trail[i].domSizes = new int[nbVars];
        trail[i].min = new int[nbVars];
        trail[i].max = new int[nbVars];
    }
    active = false;
}

Subtree::~Subtree() {
    // delete constraints
    for(Constraint::Priority p = Constraint::PRIOR_FIRST;
        p <= Constraint::PRIOR_LAST; ++p)
        for(std::vector<Constraint*>::iterator it = constraints[p].begin(),
            end = constraints[p].end(); it != end; ++it)
            delete *it;
    // delete trail
    delete [] trail;
}

void Subtree::add(Constraint *c) {
    c->solver = solver;
    c->parent = this;
    constraints[c->getPriority()].push_back(c);
}

void Subtree::activate() {
    if(isActive())
        throw "Cannot activate active subtree.";
    active = true;
    previous = solver->current;
    solver->statSubtrees++;
    trailIndex = -1;
    checkpoint(NULL, -1);
    solver->current = NULL;
    if(solver->tsCurrent < solver->tsLastConstraint)
        inconsistent = !solver->postStatic();
    else
        inconsistent = false;
    solver->current = this;
    inconsistent = inconsistent || !solver->post(constraints);
    started = false;
}

void Subtree::discard() {
    if(!isCurrent())
        throw "Only current active subtree can be discarded.";
    if(trailIndex >= 0) {
        // backtrack root checkpoint
        trailIndex = 0;
        backtrack();
    }
    solver->current = previous;
    active = false;
}

bool Subtree::search() {
    if(!isCurrent())
        throw "Only current active subtree can be searched.";

    if(inconsistent) {
        discard();
        return false;
    }

    VarInt *x = NULL;
    if(started) { // the search has started, try to backtrack
        x = backtrack();
        if(!x) {
            discard();
            return false;
        }
    } else {
        started = true;
    }
    while(true) {
        // search for a variable to bind if needed
        if(!x || x->isBound()) {
            x = NULL;
            // first label the nbOrder first variables
            for(int i = 0; i < nbOrder; i++) {
                if(!vars[i]->isBound()) {
                    x = vars[i];
                    break;
                }
            }
            // find unbound variable with smallest domain
            if(!x) {
                int sx = -1;
                for(int i = nbOrder; i < nbDecision; i++) {
                    VarInt *y = vars[i];
                    int sy = y->getSize();
                    if(sy > 1 && (!x || sy < sx)) {
                        x = y;
                        sx = sy;
                    }
                }
                if(!x) { // we have a solution for vars
                    return true;
                }
            }
        }
        // Take the last value of the domain
        int v = x->select();
        checkpoint(x, v);
        x->bind(v); // should always return true
        if(!solver->propagate()) {
            x = backtrack();
            if(!x) {
                discard();
                return false;
            }
        }
    }
}

void Subtree::checkpoint(VarInt *x, int v) {
    Checkpoint *chkp = &trail[++trailIndex];
    for(int i = 0; i < nbVars; i++) {
        chkp->domSizes[i] = vars[i]->size;
        chkp->min[i] = vars[i]->min;
        chkp->max[i] = vars[i]->max;
    }
    chkp->timestamp = solver->tsCurrent;
    chkp->x = x;
    chkp->v = v;
}

/**
 * Call the restore for every constraint.
 *
 * @param constraints the list of constraints to restore
 */
inline void fireRestore(std::vector<Constraint*> &constraints) {
    for(std::vector<Constraint*>::iterator it = constraints.begin(),
        end = constraints.end(); it != end; ++it)
        (*it)->restore();
}

VarInt* Subtree::backtrack() {
    solver->statBacktracks++;
    if(trailIndex < 0)
        return NULL;
    // restore domains
    Checkpoint *chkp = &trail[trailIndex--];
    for(int i = 0; i < nbVars; i++) {
        vars[i]->size = chkp->domSizes[i];
        vars[i]->min = chkp->min[i];
        vars[i]->max = chkp->max[i];
    }
    solver->tsCurrent = chkp->timestamp;
    // clear propagation queue
    solver->clearQueue();
    if(chkp->x) {
        for(Constraint::Priority p = Constraint::PRIOR_FIRST;
            p <= Constraint::PRIOR_LAST; ++p)
            fireRestore(constraints[p]);
        // remove old (failed) choice
        if(!chkp->x->remove(chkp->v)) {
            // branch finished: does not count as backtrack
            solver->statBacktracks--;
            return backtrack();
        }
        if(solver->tsCurrent < solver->tsLastConstraint &&
           !solver->postStatic())
            return backtrack();
        if(!solver->propagate())
            return backtrack();
    }
    return chkp->x;
}

}
