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

VarEqConstraint::VarEqConstraint(Query* query, cp::RDFVar* x1, cp::RDFVar* x2,
                                 cp::TriStateVar *b) :
        Constraint(query->solver(), PRIOR_HIGH),
        store_(query->store()), x1_(x1), x2_(x2), b_(b) {
    x1->registerChange(this);
    x2->registerChange(this);
    x1->registerRestored(this);
    x2->registerRestored(this);
    b->registerChange(this);
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
    if(x1->bound() && x2->bound()) {
        Value::id_t v1 = x1->value();
        Value::id_t v2 = x2->value();
        Value::Category cat1 = store_->category(v1);
        Value::Category cat2 = store_->category(v2);
        if(store_->eqClass(v1).contains(v2)) {
            domcheck(b_->bind(RDF_TRUE));
        } else if(cat1 <= Value::CAT_URI || cat2 <= Value::CAT_URI ||
                (cat1 == cat2 && cat1 <= Value::CAT_DATETIME)) {
            domcheck(b_->bind(RDF_FALSE));
        } else {
            domcheck(b_->bind(RDF_ERROR));
        }
    } else if(b_->bound() && b_->value() == RDF_TRUE) {
        unsigned n1 = x1->size();
        unsigned n2 = x2->size();
        unsigned oldn1 = s1_;
        unsigned oldn2 = s2_;
        unsigned removed = (oldn1 - n1) + (oldn2 - n2);
        /* removed is 0 on initial propagation. In such case, we must compute
         * the union of both domains.
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
                    for(Value::id_t id : eqClass)
                        domcheck(x2->remove(id));
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
                    for(Value::id_t id : eqClass)
                        domcheck(x1->remove(id));
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
                            domcheck(x1->remove(id));
                            n1--;
                        }
                        i--;
                    }
                }
            }
            domcheck(x2->restrictToMarks());
        }
        s1_ = x1_->size();
        s2_ = x2_->size();
    } else if(!b_->contains(RDF_TRUE)) {
        if(!b_->contains(RDF_ERROR)) { // b_ is RDF_FALSE
            /* Custom literals and plain literals with language tags cannot be
             * compared. They are either equal (thus b should be RDF_TRUE) or
             * the comparison produces a type error (thus b should be
             * RDF_ERROR).
             */
            Value::id_t upper = store_->range(Value::CAT_DATETIME).to;
            domcheck(x1->updateMax(upper));
            domcheck(x2->updateMax(upper));
        } else if(!b_->contains(RDF_FALSE)) { // b_ is RDF_ERROR
            // Type errors only occur with two literals
            Value::id_t lower = store_->range(Value::CAT_SIMPLE_LITERAL).from;
            domcheck(x1->updateMin(lower));
            domcheck(x2->updateMin(lower));
        }
        // For the remaining propagation, we need to know at least one category
        Value::Category catmin = store_->category(x1->min());
        Value::Category catmax = store_->category(x1->max());
        if(catmin != catmax) {
            x1 = x2_;
            x2 = x1_;
            catmin = store_->category(x1->min());
            catmax = store_->category(x1->max());
            if(catmin != catmax)
                return true; // x1 is not bound either, so don't continue
        }
        if(!b_->contains(RDF_ERROR)) { // b_ is RDF_FALSE
            // If x1 is a literal, x2 must be in the same category or not a
            // literal, or there would be a type error
            if(catmin >= Value::CAT_SIMPLE_LITERAL) {
                ValueRange rng = store_->range(catmin);
                domcheck(x2->updateMax(rng.to));
                for(Value::id_t id = store_->range(Value::CAT_SIMPLE_LITERAL).from;
                    id < rng.from; ++id)
                    domcheck(x2->remove(id));
            }
        } else if(!b_->contains(RDF_FALSE)) { // b_ is RDF_ERROR
            // Type errors occur with different categories or within
            // CAT_PLAIN_PLAIN or CAT_OTHER
            if(catmin <= Value::CAT_DATETIME) {
                for(Value::id_t id : store_->range(catmin))
                    domcheck(x2->remove(id));
                done_ = true;
                return true; // no need to do forward checking
            }
        }
        // The remaining propagation is Forward Checking only
        // TODO: we could start propagating once only equivalent values remain
        if(!x1_->bound() && !x2_->bound())
            return true;
        if(!x1_->bound()) {
            x1 = x2_;
            x2 = x1_;
        }
        for(Value::id_t id : store_->eqClass(x1->value()))
            domcheck(x2->remove(id));
        done_ = true;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////

VarLessConstraint::VarLessConstraint(Query* query, cp::RDFVar* x1,
                                     cp::RDFVar* x2, cp::TriStateVar* b,
                                     bool equality) :
        Constraint(query->solver(), PRIOR_HIGH),
        store_(query->store()), x1_(x1), x2_(x2), b_(b), equality_(equality) {
    x1->registerBounds(this);
    x2->registerBounds(this);
    b_->registerChange(this);
}

bool VarLessConstraint::propagate() {
    if(!b_->contains(RDF_ERROR)) {
        // x1_ and x2_ must be in comparable categories
        ValueRange comparable = store_->range(Value::CAT_SIMPLE_LITERAL,
                                              Value::CAT_DATETIME);
        domcheck(x1_->updateMin(comparable.from));
        domcheck(x2_->updateMax(comparable.to));
        // x1_ and x_2 must be in the same category
        Value::Category catMin1 = store_->category(x1_->min());
        Value::Category catMax1 = store_->category(x1_->max());
        Value::Category catMin2 = store_->category(x2_->min());
        Value::Category catMax2 = store_->category(x2_->max());
        Value::Category catMin = std::max(catMin1, catMin2);
        Value::Category catMax = std::min(catMax1, catMax2);
        if(catMin > catMax)
            return false;
        ValueRange allowed = store_->range(catMin, catMax);
        domcheck(x1_->updateMin(allowed.from));
        domcheck(x1_->updateMax(allowed.to));
        domcheck(x2_->updateMin(allowed.from));
        domcheck(x2_->updateMax(allowed.to));
    } else {
        Value::Category catMin1 = store_->category(x1_->min());
        Value::Category catMax1 = store_->category(x1_->max());
        Value::Category catMin2 = store_->category(x2_->min());
        Value::Category catMax2 = store_->category(x2_->max());
        Value::Category catMin = std::max(catMin1, catMin2);
        Value::Category catMax = std::min(catMax1, catMax2);
        if(catMax < Value::CAT_SIMPLE_LITERAL || catMin > Value::CAT_DATETIME ||
                catMin > catMax) {
            domcheck(b_->bind(RDF_ERROR));
            done_ = true;
            return true;
        }
        if(catMin1 == catMax1 && catMin2 == catMax2 && catMin1 == catMin2 &&
                catMin1 >= Value::CAT_SIMPLE_LITERAL &&
                catMin1 <= Value::CAT_DATETIME)
            domcheck(b_->remove(RDF_ERROR));
    }
    // For the remaining propagation, we need to know b_
    if(!b_->bound())
        return true;
    if(b_->value() == RDF_ERROR) {
        // We need to know at least one category
        cp::RDFVar* x1 = x1_;
        cp::RDFVar* x2 = x2_;
        Value::Category catmin = store_->category(x1->min());
        Value::Category catmax = store_->category(x1->max());
        if(catmin != catmax) {
            x1 = x2_;
            x2 = x1_;
            catmin = store_->category(x1->min());
            catmax = store_->category(x1->max());
            if(catmin != catmax)
                return true;
        }
        // A type error occurs if x1 and x2 are in different categories, or
        // if either x1 or x2 is in an incomparable category
        if(catmin >= Value::CAT_SIMPLE_LITERAL && catmin <= Value::CAT_DATETIME) {
            for(Value::id_t id : store_->range(catmin))
                domcheck(x2->remove(id));
        }
        done_ = true;
    } else {
        cp::RDFVar* x1 = x1_;
        cp::RDFVar* x2 = x2_;
        bool equality = equality_;
        if(b_->value() == RDF_FALSE) {
            x1 = x2_;
            x2 = x1_;
            equality = !equality_;
        }
        ValueRange eqClassMax2 = store_->eqClass(x2->max());
        domcheck(x1->updateMax(equality ? eqClassMax2.to : eqClassMax2.from - 1));
        ValueRange eqClassMin1 = store_->eqClass(x1->min());
        domcheck(x2_->updateMin(equality ? eqClassMin1.from : eqClassMin1.to + 1));
        // Check entailment
        ValueRange eqClassMax1 = store_->eqClass(x1->max());
        ValueRange eqClassMin2 = store_->eqClass(x2->min());
        if(( equality && eqClassMax1.to <= eqClassMin2.to) ||
           (!equality && eqClassMax1.to <  eqClassMin2.from))
            done_ = true;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////

VarSameTermConstraint::VarSameTermConstraint(Query *query, cp::RDFVar* x1,
                                             cp::RDFVar* x2, cp::TriStateVar *b) :
    Constraint(query->solver(), PRIOR_HIGH), x1_(x1), x2_(x2), b_(b) {
    x1->registerChange(this);
    x2->registerChange(this);
    x1->registerRestored(this);
    x2->registerRestored(this);
    b->registerChange(this);
}

void VarSameTermConstraint::restored(cp::Trailable*) {
    s1_ = x1_->size();
    s2_ = x2_->size();
}

bool VarSameTermConstraint::post() {
    domcheck(b_->remove(RDF_ERROR));
    restored(nullptr);
    return propagate();
}

bool VarSameTermConstraint::propagate() {
    cp::RDFVar* x1 = x1_;
    cp::RDFVar* x2 = x2_;
    if(x1->bound() && x2->bound()) {
        domcheck(b_->bind(x1->value() == x2->value() ? RDF_TRUE : RDF_FALSE));
    } else if(!b_->contains(RDF_FALSE)) { // b_ is RDF_TRUE
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
            for(unsigned i = n1; i < oldn1; i++)
                domcheck(x2->remove(dom[i]));
            dom = x2->domain();
            for(unsigned i = n2; i < oldn2; i++)
                domcheck(x1->remove(dom[i]));
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
                    domcheck(x1->remove(v));
                    n1--;
                    i--;
                }
            }
            domcheck(x2->restrictToMarks());
        }
        s1_ = x1_->size();
        s2_ = x2_->size();
    } else if(!b_->contains(RDF_TRUE)) { // b_ is RDF_FALSE
        if(x1_->bound()) {
            domcheck(x2_->remove(x1_->value()));
            done_ = true;
        } else if(x2_->bound()) {
            domcheck(x1_->remove(x2_->value()));
            done_ = true;
        }
    }
    return true;
}


}
