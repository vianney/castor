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
#include "triple.h"

namespace castor {

FCTripleConstraint::FCTripleConstraint(Query* query, RDFVarTriple triple) :
        Constraint(query->solver(), PRIOR_MEDIUM),
        store_(query->store()), triple_(triple) {
    for(int i = 0; i < triple_.COMPONENTS; i++)
        triple_[i]->registerBind(this);
}

bool FCTripleConstraint::propagate() {
    Triple min, max;
    int unbound = -1;
    for(int i = 0; i < triple_.COMPONENTS; i++) {
        if(!triple_[i]->bound()) {
            if(unbound == -1)
                unbound = i;
            else
                return true; // too many unbound variables (> 1)
        }
        min[i] = triple_[i]->min();
        max[i] = triple_[i]->max();
    }

    Store::TripleRange q(store_, min, max);

    if(unbound == -1) {
        // all variables are bound, just check
        if(!q.next(nullptr))
            return false;
        done_ = true;
        return true;
    }

    triple_[unbound]->clearMarks();
    Triple t;
    while(q.next(&t))
        triple_[unbound]->mark(t[unbound]);
    domcheck(triple_[unbound]->restrictToMarks());
    done_ = true;
    return true;
}

////////////////////////////////////////////////////////////////////////////////

ExtraTripleConstraint::ExtraTripleConstraint(Query* query, RDFVarTriple triple) :
        Constraint(query->solver(), PRIOR_LOW),
        store_(query->store()), triple_(triple) {
    for(int i = 0; i < triple_.COMPONENTS; i++)
        triple_[i]->registerBind(this);
}

bool ExtraTripleConstraint::propagate() {
    Triple min, max;
    int a = -1, b = -1;
    for(int i = 0; i < triple_.COMPONENTS; i++) {
        if(!triple_[i]->bound()) {
            if(a == -1)
                a = i;
            else if(b == -1)
                b = i;
            else
                return true; // too many unbound variables (> 2)
        }
        min[i] = triple_[i]->min();
        max[i] = triple_[i]->max();
    }

    if(a == -1 || b == -1) {
        // too few unbound variables (< 2), let FCTripleConstraint handle it
        done_ = true;
        return true;
    }

    Store::TripleRange q(store_, min, max);

    triple_[a]->clearMarks();
    triple_[b]->clearMarks();
    Triple t;
    while(q.next(&t)) {
        if(triple_[a]->contains(t[a]) && triple_[b]->contains(t[b])) {
            triple_[a]->mark(t[a]);
            triple_[b]->mark(t[b]);
        }
    }
    domcheck(triple_[a]->restrictToMarks());
    domcheck(triple_[b]->restrictToMarks());
    done_ = true;
    return true;
}

////////////////////////////////////////////////////////////////////////////////

STRTripleConstraint::STRTripleConstraint(Query *query, RDFVarTriple triple) :
    Constraint(query->solver(), PRIOR_LOW),
    store_(query->store()), triple_(triple),
    supports_(query->solver()->trail(), 0, query->store()->triplesCount() - 1) {
    for(int i = 0; i < triple_.COMPONENTS; i++)
        triple_[i]->registerChange(this);
}

bool STRTripleConstraint::propagate() {
    for(int j = 0; j < triple_.COMPONENTS; j++)
        triple_[j]->clearMarks();
    for(unsigned i = 0; i < supports_.size(); i++) {
        Triple t = store_->triple(supports_[i]);
        bool valid = true;
        for(int j = 0; j < triple_.COMPONENTS; j++) {
            if(!triple_[j]->contains(t[j])) {
                valid = false;
                break;
            }
        }
        if(valid) {
            int fullyMarked = 0;
            for(int j = 0; j < triple_.COMPONENTS; j++) {
                triple_[j]->mark(t[j]);
                if(triple_[j]->marked() == triple_[j]->size())
                    ++fullyMarked;
            }
            if(fullyMarked == triple_.COMPONENTS)
                return true;
        } else {
            supports_.remove(i);
            i--;
        }
    }
    int bound = 0;
    for(int j = 0; j < triple_.COMPONENTS; j++) {
        domcheck(triple_[j]->restrictToMarks());
        if(triple_[j]->bound())
            ++bound;
    }
    if(bound >= triple_.COMPONENTS - 1)
        done_ = true;
    return true;
}

}
