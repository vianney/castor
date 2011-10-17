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
#include "distinct.h"

#include <cassert>

namespace castor {

template <typename T>
bool DistinctConstraint::LexLess::operator ()(T* a, T* b) const {
    for(unsigned i = 0; i < size; i++) {
        if(i == index)
            continue;
        if(a[i] < b[i])
            return true;
        else if(a[i] > b[i])
            return false;
    }
    return false;
}

DistinctConstraint::DistinctConstraint(Query *query) {
    this->query = query;
    unsigned n = query->getRequestedCount();
    assert(n > 0);
    solutions = new SolSet(LexLess(n));
    indexes = new SolSet*[n];
    for(unsigned i = 0; i < n; i++) {
        indexes[i] = new SolSet(LexLess(n, i));
        query->getVariable(i)->getCPVariable()->registerBind(this);
    }
}

DistinctConstraint::~DistinctConstraint() {
    for(unsigned i = 0; i < query->getRequestedCount(); i++)
        delete indexes[i];
    delete [] indexes;
    for(SolSet::iterator it = solutions->begin(), end = solutions->end();
        it != end; ++it) {
        delete *it;
    }
    delete solutions;
}

void DistinctConstraint::addSolution() {
    unsigned n = query->getRequestedCount();
    Value::id_t *sol = new Value::id_t[n];
    for(unsigned i = 0; i < n; i++)
        sol[i] = query->getVariable(i)->getValueId();
    solutions->insert(sol);
    for(unsigned i = 0; i < n; i++)
        indexes[i]->insert(sol);
    solver->refresh(this);
}

void DistinctConstraint::reset() {
    for(unsigned i = 0; i < query->getRequestedCount(); i++)
        indexes[i]->clear();
    for(SolSet::iterator it = solutions->begin(), end = solutions->end();
        it != end; ++it) {
        delete *it;
    }
    solutions->clear();
}

bool DistinctConstraint::propagate() {
    Value::id_t sol[query->getRequestedCount()];
    int unbound = -1;
    for(unsigned i = 0; i < query->getRequestedCount(); i++) {
        VarInt *x = query->getVariable(i)->getCPVariable();
        if(x->isBound())
            sol[i] = x->getValue();
        else if(unbound != -1)
            return true; // too many unbound variables (> 1)
        else
            unbound = i;
    }
    if(unbound == -1) {
        // all variables are bound -> check
        return solutions->find(sol) == solutions->end();
    } else {
        // all variables, except one, are bound -> forward checking
        VarInt *x = query->getVariable(unbound)->getCPVariable();
        std::pair<SolSet::iterator,SolSet::iterator> range
                = indexes[unbound]->equal_range(sol);
        for(SolSet::iterator it = range.first; it != range.second; ++it) {
            if(!x->remove((*it)[unbound]))
                return false;
        }
        return true;
    }
}

}
