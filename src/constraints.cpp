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
#include "constraints.h"

#include "config.h"

namespace castor {

////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////

FilterConstraint::FilterConstraint(Store* store, Expression* expr) :
        StatelessConstraint(CASTOR_CONSTRAINTS_FILTER_PRIORITY),
        store_(store), expr_(expr) {
    for(Variable* var : expr->variables())
        var->cp()->registerBind(this);
}

void FilterConstraint::restore() {
    done_ = true;
    int unbound = 0;
    for(Variable* var : expr_->variables()) {
        cp::RDFVar* x = var->cp();
        if(!x->contains(0) && !x->bound()) {
            unbound++;
            if(unbound > 1) {
                // more than one unbound variable, we are not done
                done_ = false;
                return;
            }
        }
    }
}

bool FilterConstraint::propagate() {
    StatelessConstraint::propagate();
    Variable* unbound = nullptr;
    for(Variable* var : expr_->variables()) {
        if(var->cp()->contains(0))
            var->valueId(0);
        else if(var->cp()->bound())
            var->valueId(var->cp()->value());
        else if(unbound)
            return true; // too many unbound variables (> 1)
        else
            unbound = var;
    }
    done_ = true;
    if(!unbound) {
        // all variables are bound -> check
        return expr_->isTrue();
    } else {
        // all variables, except one, are bound -> forward checking
        cp::RDFVar* x = unbound->cp();
        x->clearMarks();
        unsigned n = x->size();
        const Value::id_t* dom = x->domain();
        for(unsigned i = 0; i < n; i++) {
            unbound->valueId(dom[i]);
            if(expr_->isTrue())
                x->mark(dom[i]);
        }
        return x->restrictToMarks();
    }
}

////////////////////////////////////////////////////////////////////////////////

SameClassConstraint::SameClassConstraint(Store* store, cp::RDFVar* x1,
                                         cp::RDFVar* x2)
        : StatelessConstraint(PRIOR_HIGH), store_(store), x1_(x1), x2_(x2) {
    x1->registerMin(this);
    x1->registerMax(this);
    x2->registerMin(this);
    x2->registerMax(this);
}

void SameClassConstraint::restore() {
    Value::Category catMin1 = store_->category(x1_->min());
    Value::Category catMax1 = store_->category(x1_->max());
    Value::Category catMin2 = store_->category(x2_->min());
    Value::Category catMax2 = store_->category(x2_->max());
    Value::Category catMin = std::max(catMin1, catMin2);
    Value::Category catMax = std::min(catMax1, catMax2);
    done_ = (catMin == catMax);
}

bool SameClassConstraint::propagate() {
    StatelessConstraint::propagate();
    Value::Category catMin1 = store_->category(x1_->min());
    Value::Category catMax1 = store_->category(x1_->max());
    Value::Category catMin2 = store_->category(x2_->min());
    Value::Category catMax2 = store_->category(x2_->max());
    Value::Category catMin = std::max(catMin1, catMin2);
    Value::Category catMax = std::min(catMax1, catMax2);
    if(catMin > catMax)
        return false;
    if(catMin == catMax)
        done_ = true;
    ValueRange allowed = store_->range(catMin, catMax);
    if(allowed.empty())
        return false;
    return x1_->updateMin(allowed.from) && x1_->updateMax(allowed.to) &&
           x2_->updateMin(allowed.from) && x2_->updateMax(allowed.to);
}

////////////////////////////////////////////////////////////////////////////////

VarDiffConstraint::VarDiffConstraint(Store* store, cp::RDFVar* x1,
                                     cp::RDFVar* x2)
        : StatelessConstraint(PRIOR_HIGH), store_(store), x1_(x1), x2_(x2) {
    x1->registerBind(this);
    x2->registerBind(this);
}

void VarDiffConstraint::restore() {
    done_ = (x1_->bound() || x2_->bound());
}

bool VarDiffConstraint::propagate() {
    StatelessConstraint::propagate();
    // TODO: we could start propagating once only equivalent values remain
    if(!x1_->bound() && !x2_->bound())
        return true;
    cp::RDFVar* x1 = x1_->bound() ? x1_ : x2_;
    cp::RDFVar* x2 = x1_->bound() ? x2_ : x1_;
    done_ = true;
    for(Value::id_t id : store_->eqClass(x1->value())) {
        if(!x2->remove(id))
            return false;
    }
    Value::Category cat = store_->category(x1->value());
    if(cat > Value::CAT_URI) {
        // Comparing two literals of different class result in type error
        ValueRange rng = store_->range(cat);
        if(!x2->updateMin(rng.from) || !x2->updateMax(rng.to))
            return false;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////

VarEqConstraint::VarEqConstraint(Store* store, cp::RDFVar* x1, cp::RDFVar* x2) :
        Constraint(PRIOR_HIGH), store_(store), x1_(x1), x2_(x2) {
    x1->registerChange(this);
    x2->registerChange(this);
}

void VarEqConstraint::restore() {
    s1_ = x1_->size();
    s2_ = x2_->size();
}

bool VarEqConstraint::post() {
    restore();
    return propagate();
}

bool VarEqConstraint::propagate() {
    cp::RDFVar* x1 = x1_;
    cp::RDFVar* x2 = x2_;
    unsigned n1 = x1->size();
    unsigned n2 = x2->size();
    unsigned oldn1 = s1_;
    unsigned oldn2 = s2_;
    unsigned removed = (oldn1 - n1) + (oldn2 - n2);
    /* removed is 0 on initial propagation. In such case, we must compute the
     * union of both domains.
     */
    if(removed > 0 && removed < n1 && removed < n2) {
        const Value::id_t* dom = x1->domain();
        for(unsigned i = n1; i < oldn1; i++) {
            ValueRange eqClass = store_->eqClass(dom[i]);
            bool prune = true;
            for(Value::id_t id : eqClass) {
                if(x1->contains(id)) {
                    prune = false;
                    break;
                }
            }
            if(prune) {
                for(Value::id_t id : eqClass) {
                    if(!x2->remove(id))
                        return false;
                }
            }
        }
        dom = x2->domain();
        for(unsigned i = n2; i < oldn2; i++) {
            ValueRange eqClass = store_->eqClass(dom[i]);
            bool prune = true;
            for(Value::id_t id : eqClass) {
                if(x2->contains(id)) {
                    prune = false;
                    break;
                }
            }
            if(prune) {
                for(Value::id_t id : eqClass) {
                    if(!x1->remove(id))
                        return false;
                }
            }
        }
    } else {
        if(n2 < n1) {
            x1 = x2;
            x2 = x1_;
            n1 = n2;
        }
        x2->clearMarks();
        const Value::id_t* dom = x1->domain();
        for(unsigned i = 0; i < n1; i++) {
            Value::id_t v = dom[i];
            ValueRange eqClass = store_->eqClass(v);
            bool prune = true;
            for(Value::id_t id : eqClass) {
                if(x2->contains(id))
                    prune = false;
                x2->mark(id);
            }
            if(prune) {
                for(Value::id_t id : eqClass) {
                    if(x1->contains(id)) {
                        if(!x1->remove(id))
                            return false;
                        n1--;
                    }
                    i--;
                }
            }
        }
        if(!x2->restrictToMarks())
            return false;
    }
    s1_ = x1_->size();
    s2_ = x2_->size();
    return true;
}

////////////////////////////////////////////////////////////////////////////////

VarLessConstraint::VarLessConstraint(Store* store, cp::RDFVar* x1,
                                     cp::RDFVar* x2, bool equality)
        : StatelessConstraint(PRIOR_HIGH), store_(store), x1_(x1), x2_(x2),
          equality(equality) {
    x1->registerMin(this);
    x1->registerMax(this);
    x2->registerMin(this);
    x2->registerMax(this);
}

void VarLessConstraint::restore() {
    ValueRange eqClass1 = store_->eqClass(x1_->max());
    ValueRange eqClass2 = store_->eqClass(x2_->min());
    done_ = equality ? eqClass1.to <= eqClass2.to
                    : eqClass1.to < eqClass2.from;
}

bool VarLessConstraint::propagate() {
    StatelessConstraint::propagate();
    ValueRange eqClassMax1 = store_->eqClass(x1_->max());
    ValueRange eqClassMin2 = store_->eqClass(x2_->min());
    done_ = equality ? eqClassMax1.to <= eqClassMin2.to
                    : eqClassMax1.to < eqClassMin2.from;
    if(done_)
        return true;
    ValueRange eqClassMax2 = store_->eqClass(x2_->max());
    if(!x1_->updateMax(equality ? eqClassMax2.to : eqClassMax2.from - 1))
        return false;
    ValueRange eqClassMin1 = store_->eqClass(x1_->min());
    return x2_->updateMin(equality ? eqClassMin1.from : eqClassMin1.to + 1);
}

////////////////////////////////////////////////////////////////////////////////

VarDiffTermConstraint::VarDiffTermConstraint(cp::RDFVar* x1, cp::RDFVar* x2) :
        StatelessConstraint(PRIOR_HIGH), x1_(x1), x2_(x2) {
    x1->registerBind(this);
    x2->registerBind(this);
}

void VarDiffTermConstraint::restore() {
    done_ = (x1_->bound() || x2_->bound());
}

bool VarDiffTermConstraint::propagate() {
    StatelessConstraint::propagate();
    if(x1_->bound()) {
        done_ = true;
        return x2_->remove(x1_->value());
    } else if(x2_->bound()) {
        done_ = true;
        return x1_->remove(x2_->value());
    } else {
        return true;
    }
}

////////////////////////////////////////////////////////////////////////////////

VarSameTermConstraint::VarSameTermConstraint(cp::RDFVar* x1, cp::RDFVar* x2) :
        Constraint(PRIOR_HIGH), x1_(x1), x2_(x2) {
    x1->registerChange(this);
    x2->registerChange(this);
}

void VarSameTermConstraint::restore() {
    s1_ = x1_->size();
    s2_ = x2_->size();
}

bool VarSameTermConstraint::post() {
    restore();
    return propagate();
}

bool VarSameTermConstraint::propagate() {
    cp::RDFVar* x1 = x1_;
    cp::RDFVar* x2 = x2_;
    unsigned n1 = x1->size();
    unsigned n2 = x2->size();
    unsigned oldn1 = s1_;
    unsigned oldn2 = s2_;
    unsigned removed = (oldn1 - n1) + (oldn2 - n2);
    /* removed is 0 on initial propagation. In such case, we must compute the
     * union of both domains.
     */
    if(removed > 0 && removed < n1 && removed < n2) {
        const Value::id_t* dom = x1->domain();
        for(unsigned i = n1; i < oldn1; i++) {
            if(!x2->remove(dom[i]))
                return false;
        }
        dom = x2->domain();
        for(unsigned i = n2; i < oldn2; i++) {
            if(!x1->remove(dom[i]))
                return false;
        }
    } else {
        if(n2 < n1) {
            x1 = x2;
            x2 = x1_;
            n1 = n2;
        }
        x2->clearMarks();
        const Value::id_t* dom = x1->domain();
        for(unsigned i = 0; i < n1; i++) {
            Value::id_t v = dom[i];
            if(x2->contains(v)) {
                x2->mark(v);
            } else {
                if(!x1->remove(v))
                    return false;
                n1--;
                i--;
            }
        }
        if(!x2->restrictToMarks())
            return false;
    }
    s1_ = x1_->size();
    s2_ = x2_->size();
    return true;
}


}
