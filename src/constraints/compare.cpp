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
#include "compare.h"

namespace castor {

SameClassConstraint::SameClassConstraint(Query* query, cp::RDFVar* x1,
                                         cp::RDFVar* x2) :
        Constraint(query->solver(), PRIOR_HIGH),
        store_(query->store()), x1_(x1), x2_(x2) {
    x1->registerMin(this);
    x1->registerMax(this);
    x2->registerMin(this);
    x2->registerMax(this);
}

bool SameClassConstraint::propagate() {
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

VarDiffConstraint::VarDiffConstraint(Query* query, cp::RDFVar* x1,
                                     cp::RDFVar* x2) :
        Constraint(query->solver(), PRIOR_HIGH),
        store_(query->store()), x1_(x1), x2_(x2) {
    x1->registerBind(this);
    x2->registerBind(this);
}

bool VarDiffConstraint::propagate() {
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

VarEqConstraint::VarEqConstraint(Query* query, cp::RDFVar* x1, cp::RDFVar* x2) :
        Constraint(query->solver(), PRIOR_HIGH),
        store_(query->store()), x1_(x1), x2_(x2) {
    x1->registerChange(this);
    x2->registerChange(this);
    x1->registerRestored(this);
    x2->registerRestored(this);
}

void VarEqConstraint::restored(cp::Trailable*) {
    s1_ = x1_->size();
    s2_ = x2_->size();
}

bool VarEqConstraint::post() {
    restored(nullptr);
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

VarLessConstraint::VarLessConstraint(Query* query, cp::RDFVar* x1,
                                     cp::RDFVar* x2, bool equality) :
        Constraint(query->solver(), PRIOR_HIGH),
        store_(query->store()), x1_(x1), x2_(x2), equality(equality) {
    x1->registerMin(this);
    x1->registerMax(this);
    x2->registerMin(this);
    x2->registerMax(this);
}

bool VarLessConstraint::propagate() {
    ValueRange eqClassMax2 = store_->eqClass(x2_->max());
    if(!x1_->updateMax(equality ? eqClassMax2.to : eqClassMax2.from - 1))
        return false;
    ValueRange eqClassMin1 = store_->eqClass(x1_->min());
    if(!x2_->updateMin(equality ? eqClassMin1.from : eqClassMin1.to + 1))
        return false;
    // Check entailment
    ValueRange eqClassMax1 = store_->eqClass(x1_->max());
    ValueRange eqClassMin2 = store_->eqClass(x2_->min());
    if(( equality && eqClassMax1.to <= eqClassMin2.to) ||
       (!equality && eqClassMax1.to <  eqClassMin2.from))
        done_ = true;
    return true;
}

////////////////////////////////////////////////////////////////////////////////

VarDiffTermConstraint::VarDiffTermConstraint(Query *query, cp::RDFVar* x1,
                                             cp::RDFVar* x2) :
        Constraint(query->solver(), PRIOR_HIGH), x1_(x1), x2_(x2) {
    x1->registerBind(this);
    x2->registerBind(this);
}

bool VarDiffTermConstraint::propagate() {
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

VarSameTermConstraint::VarSameTermConstraint(Query *query, cp::RDFVar* x1,
                                             cp::RDFVar* x2) :
        Constraint(query->solver(), PRIOR_HIGH), x1_(x1), x2_(x2) {
    x1->registerChange(this);
    x2->registerChange(this);
    x1->registerRestored(this);
    x2->registerRestored(this);
}

void VarSameTermConstraint::restored(cp::Trailable*) {
    s1_ = x1_->size();
    s2_ = x2_->size();
}

bool VarSameTermConstraint::post() {
    restored(nullptr);
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
