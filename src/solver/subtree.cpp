/* This file is part of Castor
 *
 * Author: Vianney le Clément de Saint-Marcq <vianney.leclement@uclouvain.be>
 * Copyright (C) 2010-2013, Université catholique de Louvain
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
#include "subtree.h"

#include <cstdlib>

#include "util.h"

namespace castor {
namespace cp {

/**
 * Checkpoint structure for backtracking
 */
struct Subtree::Checkpoint {
    /**
     * Checkpoint in the trail.
     */
    Trail::checkpoint_t trailpoint;
    /**
     * Timestamp of static constraints
     */
    Constraint::timestamp_t timestamp;
    /**
     * Variable that is being labeled.
     */
    DecisionVariable* x;
};

Subtree::Subtree(Solver* solver) : solver_(solver) {
    trail_ = nullptr;
    active_ = false;
}

Subtree::~Subtree() {
    // delete constraints
    for(Constraint::Priority p = Constraint::PRIOR_FIRST;
        p <= Constraint::PRIOR_LAST; ++p) {
        for(Constraint* c : constraints_[p])
            delete c;
    }
    // delete trail
    delete [] trail_;
}

void Subtree::add(DecisionVariable* x) {
    vars_.push_back(x);
}

void Subtree::add(Constraint* c) {
    c->solver_ = solver_;
    c->parent_ = this;
    constraints_[c->priority()].push_back(c);
}

void Subtree::activate() {
    if(isActive())
        throw CastorException() << "Cannot activate active subtree.";
    if(trail_ == nullptr) {
        // First activation, allocate trail
        // The depth of the subtree is at most vars_.size(), +1 for the root
        // checkpoint.
        trail_ = new Checkpoint[vars_.size() + 1];
    }
    active_ = true;
    previous_ = solver_->current_;
    solver_->statSubtrees_++;
    trailIndex_ = -1;
    checkpoint(nullptr);
    solver_->current_ = nullptr;
    if(solver_->tsCurrent_ < solver_->tsLastConstraint_)
        inconsistent_ = !solver_->postStatic();
    else
        inconsistent_ = false;
    solver_->current_ = this;
    inconsistent_ = inconsistent_ || !solver_->post(constraints_);
    started_ = false;
}

void Subtree::discard() {
    if(!isCurrent())
        throw CastorException()
            << "Only current active subtree can be discarded.";
    if(trailIndex_ >= 0) {
        // backtrack root checkpoint
        trailIndex_ = 0;
        backtrack();
    }
    solver_->current_ = previous_;
    active_ = false;
}

bool Subtree::search() {
    if(!isCurrent())
        throw CastorException()
            << "Only current active subtree can be searched.";

    if(inconsistent_) {
        discard();
        return false;
    }

    DecisionVariable* x = nullptr;
    if(started_) { // the search has started, try to backtrack
        x = backtrack();
        if(!x) {
            discard();
            return false;
        }
    } else {
        started_ = true;
    }
    while(true) {
        // search for a variable to bind if needed
        if(!x || x->bound()) {
            // find unbound variable with smallest domain
            x = nullptr;
            double sx;
            for(DecisionVariable* y : vars_) {
                if(y->bound())
                    continue;
#if CASTOR_SEARCH == CASTOR_SEARCH_dom
                double sy = y->size();
#elif CASTOR_SEARCH == CASTOR_SEARCH_deg
                double sy = -static_cast<double>(y->degree());
#elif CASTOR_SEARCH == CASTOR_SEARCH_ddeg
                double sy = -static_cast<double>(y->dyndegree());
#elif CASTOR_SEARCH == CASTOR_SEARCH_domdeg
                double sy = static_cast<double>(y->size()) / y->degree();
#elif CASTOR_SEARCH == CASTOR_SEARCH_domddeg
                double sy = static_cast<double>(y->size()) / y->dyndegree();
#elif CASTOR_SEARCH == CASTOR_SEARCH_random
                double sy = std::rand();
#else
                static_assert(false, "Please select a valid search heuristic.");
#endif
                if(!x || sy < sx) {
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
        if(!x->label() || !solver_->propagate()) {
            x = backtrack();
            if(!x) {
                discard();
                return false;
            }
        } else {
            assert(x->bound());
        }
    }
}

void Subtree::checkpoint(DecisionVariable* x) {
    ++trailIndex_;
    assert(trailIndex_ < static_cast<int>(vars_.size() + 1));
    Checkpoint* chkp = &trail_[trailIndex_];
    chkp->trailpoint = solver_->trail().checkpoint();
    chkp->timestamp = solver_->tsCurrent_;
    chkp->x = x;
}

DecisionVariable* Subtree::backtrack() {
    solver_->statBacktracks_++;
    if(trailIndex_ < 0)
        return nullptr;
    // restore domains
    Checkpoint* chkp = &trail_[trailIndex_--];
    solver_->trail().restore(chkp->trailpoint);
    solver_->tsCurrent_ = chkp->timestamp;
    // clear propagation queue
    solver_->clearQueue();
    if(chkp->x) {
        // remove old (failed) choice
        if(!chkp->x->unlabel())
            return backtrack();
        if(solver_->tsCurrent_ < solver_->tsLastConstraint_ &&
           !solver_->postStatic())
            return backtrack();
        if(!solver_->propagate())
            return backtrack();
    }
    return chkp->x;
}

}
}
