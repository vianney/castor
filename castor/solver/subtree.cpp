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
namespace cp {

/**
 * Checkpoint structure for backtracking
 */
struct Checkpoint {
    /**
     * Checkpoint data of all the variables stored sequentially.
     */
    char *varsData;
    /**
     * Timestamp of static constraints
     */
    int timestamp;
    /**
     * Variable that is being labeled.
     */
    Variable *x;

    ~Checkpoint() {
        delete [] varsData;
    }
};

Subtree::Subtree(Solver *solver) : solver(solver) {
    nbDecision = 0;
    trail = nullptr;
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

void Subtree::add(Variable *x, bool label) {
    if(label)
        vars.insert(vars.begin() + nbDecision++, x);
    else
        vars.push_back(x);
}

void Subtree::add(Constraint *c) {
    c->solver = solver;
    c->parent = this;
    constraints[c->getPriority()].push_back(c);
}

void Subtree::activate() {
    if(isActive())
        throw "Cannot activate active subtree.";
    if(trail == nullptr) {
        // First activiation, allocate trail
        std::size_t size = 0;
        for(Variable *x : vars)
            size += x->getTrailSize();
        // The depth of the subtree is at most vars.size(), +1 for the root
        // checkpoint.
        trail = new Checkpoint[vars.size() + 1];
        for(unsigned i = 0; i <= vars.size(); i++)
            trail[i].varsData = new char[size];
    }
    active = true;
    previous = solver->current;
    solver->statSubtrees++;
    trailIndex = -1;
    checkpoint(nullptr);
    solver->current = nullptr;
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

    Variable *x = nullptr;
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
            // find unbound variable with smallest domain
            x = nullptr;
            unsigned sx;
            for(int i = 0; i < nbDecision; i++) {
                Variable *y = vars[i];
                unsigned sy = y->getSize();
                if(sy > 1 && (!x || sy < sx)) {
                    x = y;
                    sx = sy;
                }
            }
            if(!x) { // we have a solution
                return true;
            }
        }
        // Make a checkpoint and assign a value to the selected variable
        checkpoint(x);
        x->select();
        if(!solver->propagate()) {
            x = backtrack();
            if(!x) {
                discard();
                return false;
            }
        }
    }
}

void Subtree::checkpoint(Variable *x) {
    Checkpoint *chkp = &trail[++trailIndex];
    char *varsData = chkp->varsData;
    for(Variable *y : vars) {
        y->checkpoint(varsData);
        varsData += y->getTrailSize();
    }
    chkp->timestamp = solver->tsCurrent;
    chkp->x = x;
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

Variable* Subtree::backtrack() {
    solver->statBacktracks++;
    if(trailIndex < 0)
        return nullptr;
    // restore domains
    Checkpoint *chkp = &trail[trailIndex--];
    char *varsData = chkp->varsData;
    for(Variable *x : vars) {
        x->restore(varsData);
        varsData += x->getTrailSize();
    }
    solver->tsCurrent = chkp->timestamp;
    // clear propagation queue
    solver->clearQueue();
    if(chkp->x) {
        for(Constraint::Priority p = Constraint::PRIOR_FIRST;
            p <= Constraint::PRIOR_LAST; ++p)
            fireRestore(constraints[p]);
        // remove old (failed) choice
        chkp->x->unselect();
        if(solver->tsCurrent < solver->tsLastConstraint &&
           !solver->postStatic())
            return backtrack();
        if(!solver->propagate())
            return backtrack();
    }
    return chkp->x;
}

}
}
