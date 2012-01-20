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
#include "constraints.h"
#include "config.h"

namespace castor {

////////////////////////////////////////////////////////////////////////////////

StatementConstraint::StatementConstraint(Query *query, StatementPattern &stmt) :
        StatelessConstraint(CASTOR_CONSTRAINTS_STATEMENT_PRIORITY),
        store(query->getStore()), stmt(stmt) {
    if(stmt.subject.isVariable()) {
        subject = query->getVariable(stmt.subject.getVariableId())
                            ->getCPVariable();
        subject->registerBind(this);
    } else {
        subject = NULL;
    }
    if(stmt.predicate.isVariable()) {
        predicate = query->getVariable(stmt.predicate.getVariableId())
                            ->getCPVariable();
        predicate->registerBind(this);
    } else {
        predicate = NULL;
    }
    if(stmt.object.isVariable()) {
        object = query->getVariable(stmt.object.getVariableId())
                            ->getCPVariable();
        object->registerBind(this);
    } else {
        object = NULL;
    }
}

void StatementConstraint::restore() {
    int bound = (subject == NULL || subject->isBound())
              + (predicate == NULL || predicate->isBound())
              + (object == NULL || object->isBound());
    done = (bound >= 2);
}

bool StatementConstraint::propagate() {
    StatelessConstraint::propagate();

    Statement q;
    int bound = 3;
    if(subject == NULL) {
        q.subject = stmt.subject.getValueId();
    } else if(subject->isBound()) {
        q.subject = subject->getValue();
    } else {
        q.subject = 0;
        bound--;
    }
    if(predicate == NULL) {
        q.predicate = stmt.predicate.getValueId();
    } else if(predicate->isBound()) {
        q.predicate = predicate->getValue();
    } else {
        q.predicate = 0;
        bound--;
    }
    if(object == NULL) {
        q.object = stmt.object.getValueId();
    } else if(object->isBound()) {
        q.object = object->getValue();
    } else {
        q.object = 0;
        bound--;
    }

    if(bound == 0)
        // nothing bound, we do not want to check all triples
        return true;

    if(bound >= 2)
        done = true;

    Store::StatementQuery query(*store, q);

    if(bound == 3) {
        // all variables are bound, just check
        return query.next(NULL);
    }

    if(q.subject == 0) subject->clearMarks();
    if(q.predicate == 0) predicate->clearMarks();
    if(q.object == 0) object->clearMarks();
    Statement st;
    while(query.next(&st)) {
        if((q.subject == 0 && !subject->contains(st.subject)) ||
           (q.predicate == 0 && !predicate->contains(st.predicate)) ||
           (q.object == 0 && !object->contains(st.object)))
            continue;
        if(q.subject == 0) subject->mark(st.subject);
        if(q.predicate == 0) predicate->mark(st.predicate);
        if(q.object == 0) object->mark(st.object);
    }
    if(q.subject == 0 && !subject->restrictToMarks()) return false;
    if(q.predicate == 0 && !predicate->restrictToMarks()) return false;
    if(q.object == 0 && !object->restrictToMarks()) return false;
    return true;
}

////////////////////////////////////////////////////////////////////////////////

FilterConstraint::FilterConstraint(Store *store, Expression *expr) :
        StatelessConstraint(CASTOR_CONSTRAINTS_FILTER_PRIORITY),
        store(store), expr(expr) {
    for(Variable* var : expr->getVars())
        var->getCPVariable()->registerBind(this);
}

void FilterConstraint::restore() {
    done = true;
    int unbound = 0;
    for(Variable* var : expr->getVars()) {
        cp::RDFVar *x = var->getCPVariable();
        if(!x->contains(0) && !x->isBound()) {
            unbound++;
            if(unbound > 1) {
                // more than one unbound variable, we are not done
                done = false;
                return;
            }
        }
    }
}

bool FilterConstraint::propagate() {
    StatelessConstraint::propagate();
    Variable* unbound = NULL;
    for(Variable *x : expr->getVars()) {
        if(x->getCPVariable()->contains(0))
            x->setValueId(0);
        else if(x->getCPVariable()->isBound())
            x->setValueId(x->getCPVariable()->getValue());
        else if(unbound)
            return true; // too many unbound variables (> 1)
        else
            unbound = x;
    }
    done = true;
    if(!unbound) {
        // all variables are bound -> check
        return expr->isTrue();
    } else {
        // all variables, except one, are bound -> forward checking
        cp::RDFVar *x = unbound->getCPVariable();
        x->clearMarks();
        unsigned n = x->getSize();
        const Value::id_t* dom = x->getDomain();
        for(unsigned i = 0; i < n; i++) {
            unbound->setValueId(dom[i]);
            if(expr->isTrue())
                x->mark(dom[i]);
        }
        return x->restrictToMarks();
    }
}

////////////////////////////////////////////////////////////////////////////////

SameClassConstraint::SameClassConstraint(Store *store, cp::RDFVar *x1, cp::RDFVar *x2)
        : StatelessConstraint(PRIOR_HIGH), store(store), x1(x1), x2(x2) {
    x1->registerMin(this);
    x1->registerMax(this);
    x2->registerMin(this);
    x2->registerMax(this);
}

void SameClassConstraint::restore() {
    Value::Class clsMin1 = store->getValueClass(x1->getMin());
    Value::Class clsMax1 = store->getValueClass(x1->getMax());
    Value::Class clsMin2 = store->getValueClass(x2->getMin());
    Value::Class clsMax2 = store->getValueClass(x2->getMax());
    Value::Class clsMin = std::max(clsMin1, clsMin2);
    Value::Class clsMax = std::min(clsMax1, clsMax2);
    done = (clsMin == clsMax);
}

bool SameClassConstraint::propagate() {
    StatelessConstraint::propagate();
    Value::Class clsMin1 = store->getValueClass(x1->getMin());
    Value::Class clsMax1 = store->getValueClass(x1->getMax());
    Value::Class clsMin2 = store->getValueClass(x2->getMin());
    Value::Class clsMax2 = store->getValueClass(x2->getMax());
    Value::Class clsMin = std::max(clsMin1, clsMin2);
    Value::Class clsMax = std::min(clsMax1, clsMax2);
    if(clsMin > clsMax)
        return false;
    if(clsMin == clsMax)
        done = true;
    ValueRange allowed = store->getClassValues(clsMin, clsMax);
    if(allowed.empty())
        return false;
    return x1->updateMin(allowed.from) && x1->updateMax(allowed.to) &&
           x2->updateMin(allowed.from) && x2->updateMax(allowed.to);
}

////////////////////////////////////////////////////////////////////////////////

VarDiffConstraint::VarDiffConstraint(Store *store, cp::RDFVar *x1, cp::RDFVar *x2)
        : StatelessConstraint(PRIOR_HIGH), store(store), x1(x1), x2(x2) {
    x1->registerBind(this);
    x2->registerBind(this);
}

void VarDiffConstraint::restore() {
    done = (x1->isBound() || x2->isBound());
}

bool VarDiffConstraint::propagate() {
    StatelessConstraint::propagate();
    // TODO we could start propagating once only equivalent values remain
    if(!x1->isBound() && !x2->isBound())
        return true;
    cp::RDFVar *x1 = this->x1->isBound() ? this->x1 : this->x2;
    cp::RDFVar *x2 = this->x1->isBound() ? this->x2 : this->x1;
    done = true;
    for(Value::id_t id : store->getValueEqClass(x1->getValue())) {
        if(!x2->remove(id))
            return false;
    }
    Value::Class cls = store->getValueClass(x1->getValue());
    if(cls > Value::CLASS_IRI) {
        // Comparing two literals of different class result in type error
        ValueRange rng = store->getClassValues(cls);
        if(!x2->updateMin(rng.from) || !x2->updateMax(rng.to))
            return false;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////

VarEqConstraint::VarEqConstraint(Store *store, cp::RDFVar *x1, cp::RDFVar *x2) :
        Constraint(PRIOR_HIGH), store(store), x1(x1), x2(x2) {
    x1->registerChange(this);
    x2->registerChange(this);
}

void VarEqConstraint::restore() {
    s1 = x1->getSize();
    s2 = x2->getSize();
}

bool VarEqConstraint::post() {
    restore();
    return propagate();
}

bool VarEqConstraint::propagate() {
    cp::RDFVar *x1 = this->x1, *x2 = this->x2;
    int n1 = x1->getSize(), n2 = x2->getSize();
    int oldn1 = s1, oldn2 = s2;
    int removed = (oldn1 - n1) + (oldn2 - n2);
    /* removed is 0 on initial propagation. In such case, we must compute the
     * union of both domains.
     */
    if(removed > 0 && removed < n1 && removed < n2) {
        const Value::id_t *dom = x1->getDomain();
        for(int i = n1; i < oldn1; i++) {
            ValueRange eqClass = store->getValueEqClass(dom[i]);
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
        dom = x2->getDomain();
        for(int i = n2; i < oldn2; i++) {
            ValueRange eqClass = store->getValueEqClass(dom[i]);
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
            x2 = this->x1;
            n1 = n2;
        }
        x2->clearMarks();
        const Value::id_t *dom = x1->getDomain();
        for(int i = 0; i < n1; i++) {
            int v = dom[i];
            ValueRange eqClass = store->getValueEqClass(v);
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
    s1 = x1->getSize();
    s2 = x2->getSize();
    return true;
}

////////////////////////////////////////////////////////////////////////////////

VarLessConstraint::VarLessConstraint(Store *store, cp::RDFVar *x1, cp::RDFVar *x2, bool equality)
        : StatelessConstraint(PRIOR_HIGH), store(store), x1(x1), x2(x2), equality(equality) {
    x1->registerMin(this);
    x1->registerMax(this);
    x2->registerMin(this);
    x2->registerMax(this);
}

void VarLessConstraint::restore() {
    ValueRange eqClass1 = store->getValueEqClass(x1->getMax());
    ValueRange eqClass2 = store->getValueEqClass(x2->getMin());
    done = equality ? eqClass1.to <= eqClass2.to
                    : eqClass1.to < eqClass2.from;
}

bool VarLessConstraint::propagate() {
    StatelessConstraint::propagate();
    ValueRange eqClassMax1 = store->getValueEqClass(x1->getMax());
    ValueRange eqClassMin2 = store->getValueEqClass(x2->getMin());
    done = equality ? eqClassMax1.to <= eqClassMin2.to
                    : eqClassMax1.to < eqClassMin2.from;
    if(done)
        return true;
    ValueRange eqClassMax2 = store->getValueEqClass(x2->getMax());
    if(!x1->updateMax(equality ? eqClassMax2.to : eqClassMax2.from - 1))
        return false;
    ValueRange eqClassMin1 = store->getValueEqClass(x1->getMin());
    return x2->updateMin(equality ? eqClassMin1.from : eqClassMin1.to + 1);
}

////////////////////////////////////////////////////////////////////////////////

VarDiffTermConstraint::VarDiffTermConstraint(cp::RDFVar *x1, cp::RDFVar *x2) :
        StatelessConstraint(PRIOR_HIGH), x1(x1), x2(x2) {
    x1->registerBind(this);
    x2->registerBind(this);
}

void VarDiffTermConstraint::restore() {
    done = (x1->isBound() || x2->isBound());
}

bool VarDiffTermConstraint::propagate() {
    StatelessConstraint::propagate();
    if(x1->isBound()) {
        done = true;
        return x2->remove(x1->getValue());
    } else if(x2->isBound()) {
        done = true;
        return x1->remove(x2->getValue());
    } else {
        return true;
    }
}

////////////////////////////////////////////////////////////////////////////////

VarSameTermConstraint::VarSameTermConstraint(cp::RDFVar *x1, cp::RDFVar *x2) :
        Constraint(PRIOR_HIGH), x1(x1), x2(x2) {
    x1->registerChange(this);
    x2->registerChange(this);
}

void VarSameTermConstraint::restore() {
    s1 = x1->getSize();
    s2 = x2->getSize();
}

bool VarSameTermConstraint::post() {
    restore();
    return propagate();
}

bool VarSameTermConstraint::propagate() {
    cp::RDFVar *x1 = this->x1, *x2 = this->x2;
    int n1 = x1->getSize(), n2 = x2->getSize();
    int oldn1 = s1, oldn2 = s2;
    int removed = (oldn1 - n1) + (oldn2 - n2);
    /* removed is 0 on initial propagation. In such case, we must compute the
     * union of both domains.
     */
    if(removed > 0 && removed < n1 && removed < n2) {
        const Value::id_t *dom = x1->getDomain();
        for(int i = n1; i < oldn1; i++) {
            if(!x2->remove(dom[i]))
                return false;
        }
        dom = x2->getDomain();
        for(int i = n2; i < oldn2; i++) {
            if(!x1->remove(dom[i]))
                return false;
        }
    } else {
        if(n2 < n1) {
            x1 = x2;
            x2 = this->x1;
            n1 = n2;
        }
        x2->clearMarks();
        const Value::id_t *dom = x1->getDomain();
        for(int i = 0; i < n1; i++) {
            int v = dom[i];
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
    s1 = x1->getSize();
    s2 = x2->getSize();
    return true;
}


}
