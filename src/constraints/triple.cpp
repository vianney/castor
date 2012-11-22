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
        StatelessConstraint(CASTOR_CONSTRAINTS_STATEMENT_PRIORITY),
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

#ifdef CASTOR_CSTR_TIMING
long TripleConstraint::time[3] = {0, 0, 0};
long TripleConstraint::count[3] = {0, 0, 0};

void TripleConstraint::addtime(int index, rusage &start) {
    rusage stop;
    getrusage(RUSAGE_SELF, &stop);
    count[index]++;
    time[index] += ((long)(stop.ru_utime.tv_sec + stop.ru_stime.tv_sec -
                    start.ru_utime.tv_sec - start.ru_stime.tv_sec) * 1000L +
             (long)(stop.ru_utime.tv_usec + stop.ru_stime.tv_usec -
                    start.ru_utime.tv_usec - start.ru_stime.tv_usec) / 1000L);
}
#endif

void TripleConstraint::restore() {
    int bound = 0;
    for(int i = 0; i < pat_.COMPONENTS; i++)
        bound += (x_[i] == nullptr || x_[i]->bound());
    done_ = (bound >= pat_.COMPONENTS - 1);
}

bool TripleConstraint::propagate() {
    StatelessConstraint::propagate();

#ifdef CASTOR_CSTR_TIMING
    rusage ru;
    getrusage(RUSAGE_SELF, &ru);
#endif

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
#ifdef CASTOR_CSTR_TIMING
        addtime(0, ru);
#endif
        return true;
    }

    if(bound >= pat_.COMPONENTS - 1)
        done_ = true;

    Store::TripleRange q(store_, min, max);

    if(bound == pat_.COMPONENTS) {
        // all variables are bound, just check
        bool result = q.next(nullptr);
#ifdef CASTOR_CSTR_TIMING
        addtime(1, ru);
#endif
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
#ifdef CASTOR_CSTR_TIMING
    addtime(2, ru);
#endif
    return true;
}

}
