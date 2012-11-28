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
#include "triple.h"

namespace castor {

TripleConstraint::TripleConstraint(Query* query, TriplePattern pat) :
        Constraint(query->solver(), CASTOR_CONSTRAINTS_STATEMENT_PRIORITY),
        store_(query->store()), pat_(pat) {
    for(int i = 0; i < pat.COMPONENTS; i++) {
        if(pat[i].isVariable()) {
            x_[i] = query->variable(pat[i])->cp();
            x_[i]->registerBind(this);
        } else {
            x_[i] = nullptr;
        }
    }
}

bool TripleConstraint::propagate() {
    Triple min, max;
    int bound = pat_.COMPONENTS;
    for(int i = 0; i < pat_.COMPONENTS; i++) {
        if(x_[i] == nullptr) {
            min[i] = max[i] = pat_[i].valueId();
        } else {
            min[i] = x_[i]->min();
            max[i] = x_[i]->max();
            bound -= x_[i]->bound() ? 0 : 1;
        }
    }

    if(bound == 0) {
        // nothing bound, we do not want to check all triples
        return true;
    }

    if(bound >= pat_.COMPONENTS - 1)
        done_ = true;

    Store::TripleRange q(store_, min, max);

    if(bound == pat_.COMPONENTS) {
        // all variables are bound, just check
        bool result = q.next(nullptr);
        return result;
    }

    for(int i = 0; i < pat_.COMPONENTS; i++)
        if(min[i] != max[i]) x_[i]->clearMarks();
    Triple t;
    while(q.next(&t)) {
        for(int i = 0; i < pat_.COMPONENTS; i++)
            if(min[i] != max[i] && !x_[i]->contains(t[i])) goto nextTriple;
        for(int i = 0; i < pat_.COMPONENTS; i++)
            if(min[i] != max[i]) x_[i]->mark(t[i]);
    nextTriple:
        ;
    }
    for(int i = 0; i < pat_.COMPONENTS; i++)
        if(min[i] != max[i] && !x_[i]->restrictToMarks()) return false;
    return true;
}

}
