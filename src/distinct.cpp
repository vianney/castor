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
#include "distinct.h"

#include <cassert>

namespace castor {

template <typename T>
bool DistinctConstraint::LexLess::operator()(T* a, T* b) const {
    for(unsigned i = 0; i < size_; i++) {
        if(i == index_)
            continue;
        if(a[i] < b[i])
            return true;
        else if(a[i] > b[i])
            return false;
    }
    return false;
}

DistinctConstraint::DistinctConstraint(Query* query) {
    query_ = query;
    unsigned n = query->requested();
    assert(n > 0);
    solutions_ = new SolSet(LexLess(n));
    indexes_ = new SolSet*[n];
    for(unsigned i = 0; i < n; i++) {
        indexes_[i] = new SolSet(LexLess(n, i));
        query->variable(i)->cp()->registerBind(this);
    }
}

DistinctConstraint::~DistinctConstraint() {
    for(unsigned i = 0; i < query_->requested(); i++)
        delete indexes_[i];
    delete [] indexes_;
    for(Value::id_t* sol : *solutions_)
        delete [] sol;
    delete solutions_;
}

void DistinctConstraint::addSolution() {
    unsigned n = query_->requested();
    Value::id_t* sol = new Value::id_t[n];
    for(unsigned i = 0; i < n; i++)
        sol[i] = query_->variable(i)->valueId();
    solutions_->insert(sol);
    for(unsigned i = 0; i < n; i++)
        indexes_[i]->insert(sol);
    solver_->refresh(this);
}

void DistinctConstraint::reset() {
    for(unsigned i = 0; i < query_->requested(); i++)
        indexes_[i]->clear();
    for(Value::id_t* sol : *solutions_)
        delete [] sol;
    solutions_->clear();
}

bool DistinctConstraint::propagate() {
    Value::id_t sol[query_->requested()];
    int unbound = -1;
    for(unsigned i = 0; i < query_->requested(); i++) {
        cp::RDFVar* x = query_->variable(i)->cp();
        if(x->isBound())
            sol[i] = x->value();
        else if(unbound != -1)
            return true; // too many unbound variables (> 1)
        else
            unbound = i;
    }
    if(unbound == -1) {
        // all variables are bound -> check
        return solutions_->find(sol) == solutions_->end();
    } else {
        // all variables, except one, are bound -> forward checking
        cp::RDFVar* x = query_->variable(unbound)->cp();
        std::pair<SolSet::iterator,SolSet::iterator> range
                = indexes_[unbound]->equal_range(sol);
        for(SolSet::iterator it = range.first; it != range.second; ++it) {
            if(!x->remove((*it)[unbound]))
                return false;
        }
        return true;
    }
}

}
