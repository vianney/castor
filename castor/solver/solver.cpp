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
#include "solver.h"

namespace castor {

Solver::Solver() {
    for(ConstraintPriority p = CSTR_PRIOR_FIRST; p <= CSTR_PRIOR_LAST; ++p)
        propagQueue[p] = NULL;
    current = NULL;
    tsCurrent = 0;
    tsLastConstraint = 0;
    statBacktracks = 0;
    statSubtrees = 0;
    statPost = 0;
    statPropagate = 0;
}

Solver::~Solver() {
    for(std::vector<Constraint*>::iterator it = constraints.begin(),
        end = constraints.end(); it != end; ++it)
        delete *it;
}

/**
 * Dummy non-null pointer to indicate a constraint is not currently queued for
 * for propagation.
 * This value is used in the nextPropag field. We use the address of the solver
 * object, so we are sure no Constraint object will have this address.
 */
#define CSTR_UNQUEUED reinterpret_cast<Constraint*>(this)

void Solver::add(Constraint *c) {
    c->solver = this;
    c->parent = NULL;
    c->nextPropag = CSTR_UNQUEUED;
    c->timestamp = ++tsLastConstraint;
    constraints.push_back(c);
}

void Solver::refresh(Constraint *c) {
    c->timestamp = ++tsLastConstraint;
}

void Solver::enqueue(std::vector<Constraint*> &constraints) {
    for(std::vector<Constraint*>::iterator it = constraints.begin(),
        end = constraints.end(); it != end; ++it) {
        Constraint *c = *it;
        if(!c->done && c->nextPropag == CSTR_UNQUEUED &&
                ((c->parent == NULL && c->timestamp <= tsCurrent) ||
                 (current != NULL && c->parent == current))) {
            ConstraintPriority p = c->getPriority();
            c->nextPropag = propagQueue[p];
            propagQueue[p] = c;
        }
    }
}

bool Solver::postStatic() {
    int ts = tsCurrent;
    tsCurrent = tsLastConstraint;
    // mark all to-be reposted constraints as propagating or unqueued if they
    // are stateless
    for(std::vector<Constraint*>::iterator it = constraints.begin(),
        end = constraints.end(); it != end; ++it) {
        Constraint *c = *it;
        if(c->timestamp > ts) {
            c->nextPropag = dynamic_cast<StatelessConstraint*>(c) ?
                        CSTR_UNQUEUED : NULL;
            c->init();
        }
    }
    // call initial propagation
    for(std::vector<Constraint*>::iterator it = constraints.begin(),
        end = constraints.end(); it != end; ++it) {
        Constraint *c = *it;
        if(c->timestamp > ts) {
            statPost++;
            if(!c->post()) {
                /* Beware that some constraints are left in "propagating" state
                 * while they are not in queue. As we are inconsistent, we will
                 * backtrack, so tsCurrent will be restored and it will not
                 * matter.
                 */
                return false;
            }
            c->nextPropag = CSTR_UNQUEUED;
        }
    }
    // propagate
    return propagate();
}

bool Solver::post(std::vector<Constraint *> *constraints) {
    // mark all constraints as propagating or unqueued if they are stateless
    for(ConstraintPriority p = CSTR_PRIOR_FIRST; p <= CSTR_PRIOR_LAST; ++p) {
        for(std::vector<Constraint*>::iterator it = constraints[p].begin(),
            end = constraints[p].end(); it != end; ++it) {
            Constraint *c = *it;
            c->nextPropag = dynamic_cast<StatelessConstraint*>(c) ?
                        CSTR_UNQUEUED : NULL;
            c->init();
        }
    }
    for(ConstraintPriority p = CSTR_PRIOR_FIRST; p <= CSTR_PRIOR_LAST; ++p) {
        // mark constraints as propagating
        for(std::vector<Constraint*>::iterator it = constraints[p].begin(),
            end = constraints[p].end(); it != end; ++it)
            (*it)->nextPropag = NULL;
        // call initial propagation
        for(std::vector<Constraint*>::iterator it = constraints[p].begin(),
            end = constraints[p].end(); it != end; ++it) {
            Constraint *c = *it;
            statPost++;
            if(!c->post())
                /* Beware that some constraints are left in "propagating" state
                 * while they are not in queue. As we are in initial propagation,
                 * the subtree will be inconsistent and be discarded.
                 */
                return false;
            c->nextPropag = CSTR_UNQUEUED;
        }
        // propagate
        if(!propagate())
            return false;
    }
    return true;
}

bool Solver::propagate() {
    for(ConstraintPriority p = CSTR_PRIOR_FIRST; p <= CSTR_PRIOR_LAST; ++p) {
        if(propagQueue[p] != NULL) {
            Constraint *c = propagQueue[p];
            propagQueue[p] = c->nextPropag;
            statPropagate++;
            if(!c->propagate()) {
                c->nextPropag = CSTR_UNQUEUED;
                return false;
            }
            c->nextPropag = CSTR_UNQUEUED;
            return propagate();
        }
    }
    return true;
}

void Solver::clearQueue() {
    for(ConstraintPriority p = CSTR_PRIOR_FIRST; p <= CSTR_PRIOR_LAST; ++p) {
        while(propagQueue[p]) {
            Constraint *c = propagQueue[p];
            propagQueue[p] = c->nextPropag;
            c->nextPropag = CSTR_UNQUEUED;
        }
    }
}

}
