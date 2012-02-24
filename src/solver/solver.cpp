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
#include "solver.h"

#include <cstdlib>
#include <iostream>

namespace castor {
namespace cp {

Solver::Solver() {
    for(Constraint::Priority p = Constraint::PRIOR_FIRST;
        p <= Constraint::PRIOR_LAST; ++p)
        propagQueue_[p] = nullptr;
    current_ = nullptr;
    tsCurrent_ = 0;
    tsLastConstraint_ = 0;
    statBacktracks_ = 0;
    statSubtrees_ = 0;
    statPost_ = 0;
    statPropagate_ = 0;
}

Solver::~Solver() {
    for(Constraint* c : constraints_)
        delete c;
}

void Solver::add(Constraint* c) {
    c->solver_ = this;
    c->parent_ = nullptr;
    c->nextPropag_ = unqueued();
    c->timestamp_ = ++tsLastConstraint_;
    constraints_.push_back(c);
}

void Solver::refresh(Constraint* c) {
    c->timestamp_ = ++tsLastConstraint_;
}

void Solver::enqueue(std::vector<Constraint*>& constraints) {
    for(Constraint* c : constraints) {
        if(!c->done_ && c->nextPropag_ == unqueued() &&
                ((c->parent_ == nullptr && c->timestamp_ <= tsCurrent_) ||
                 (current_ != nullptr && c->parent_ == current_))) {
            Constraint::Priority p = c->priority();
            c->nextPropag_ = propagQueue_[p];
            propagQueue_[p] = c;
        }
    }
}

bool Solver::postStatic() {
    Constraint::timestamp_t ts = tsCurrent_;
    tsCurrent_ = tsLastConstraint_;
    // mark all to-be reposted constraints as propagating or unqueued if they
    // are stateless
    for(Constraint* c : constraints_) {
        if(c->timestamp_ > ts) {
            c->nextPropag_ = dynamic_cast<StatelessConstraint*>(c) ?
                        unqueued() : nullptr;
            c->init();
        }
    }
    // call initial propagation
    for(Constraint* c : constraints_) {
        if(c->timestamp_ > ts) {
            statPost_++;
            if(!c->post()) {
                /* Beware that some constraints are left in "propagating" state
                 * while they are not in queue. As we are inconsistent, we will
                 * backtrack, so tsCurrent will be restored and it will not
                 * matter.
                 */
                return false;
            }
            c->nextPropag_ = unqueued();
        }
    }
    // propagate
    return propagate();
}

bool Solver::post(std::vector<Constraint *>* constraints) {
    // mark all constraints as propagating or unqueued if they are stateless
    for(Constraint::Priority p = Constraint::PRIOR_FIRST;
        p <= Constraint::PRIOR_LAST; ++p) {
        for(Constraint* c : constraints[p]) {
            c->nextPropag_ = dynamic_cast<StatelessConstraint*>(c) ?
                        unqueued() : nullptr;
            c->init();
        }
    }
    for(Constraint::Priority p = Constraint::PRIOR_FIRST;
        p <= Constraint::PRIOR_LAST; ++p) {
        // mark constraints as propagating
        for(Constraint* c : constraints[p])
            c->nextPropag_ = nullptr;
        // call initial propagation
        for(Constraint* c : constraints[p]) {
            statPost_++;
            if(!c->post())
                /* Beware that some constraints are left in "propagating" state
                 * while they are not in queue. As we are in initial propagation,
                 * the subtree will be inconsistent and be discarded.
                 */
                return false;
            c->nextPropag_ = unqueued();
        }
        // propagate
        if(!propagate())
            return false;
    }
    return true;
}

bool Solver::propagate() {
    for(Constraint::Priority p = Constraint::PRIOR_FIRST;
        p <= Constraint::PRIOR_LAST; ++p) {
        if(propagQueue_[p] != nullptr) {
            Constraint* c = propagQueue_[p];
            propagQueue_[p] = c->nextPropag_;
            statPropagate_++;
            if(!c->propagate()) {
                c->nextPropag_ = unqueued();
                return false;
            }
            c->nextPropag_ = unqueued();
            return propagate();
        }
    }
    return true;
}

void Solver::clearQueue() {
    for(Constraint::Priority p = Constraint::PRIOR_FIRST;
        p <= Constraint::PRIOR_LAST; ++p) {
        while(propagQueue_[p]) {
            Constraint* c = propagQueue_[p];
            propagQueue_[p] = c->nextPropag_;
            c->nextPropag_ = unqueued();
        }
    }
}

}
}
