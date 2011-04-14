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

namespace castor {

////////////////////////////////////////////////////////////////////////////////

StatementConstraint::StatementConstraint(Query *query, StatementPattern &stmt) :
        store(query->getStore()), stmt(stmt) {
#define REGISTER(part) \
    if(stmt.part.isVariable()) { \
        part = query->getVariable(stmt.part.getVariableId())->getCPVariable(); \
        part->registerBind(this); \
    } else { \
        part = NULL; \
    }
    REGISTER(subject);
    REGISTER(predicate);
    REGISTER(object);
#undef REGISTER
}

bool StatementConstraint::propagate() {
    int vs, vp, vo;
#define INIT(p, part) \
    if(part == NULL) \
        v##p = stmt.part.getValueId(); \
    else if(part->isBound()) \
        v##p = part->getValue(); \
    else \
        v##p = -1;
    INIT(s, subject)
    INIT(p, predicate)
    INIT(o, object)
#undef INIT

    if(vs < 0 && vp < 0 && vo < 0) {
        // nothing bound, we do not want to check all triples
        return true;
    }

    store->queryStatements(vs, vp, vo);

    if(vs >= 0 && vp >= 0 && vo >= 0) {
        // all variables are bound, just check
        return store->fetchStatement(NULL);
    }

    if(vs < 0) subject->clearMarks();
    if(vp < 0) predicate->clearMarks();
    if(vo < 0) object->clearMarks();
    Statement st;
    while(store->fetchStatement(&st)) {
        if((vs < 0 && !subject->contains(st.subject)) ||
           (vp < 0 && !predicate->contains(st.predicate)) ||
           (vo < 0 && !object->contains(st.object)))
            continue;
        if(vs < 0) subject->mark(st.subject);
        if(vp < 0) predicate->mark(st.predicate);
        if(vo < 0) object->mark(st.object);
    }
    if(vs < 0 && !subject->restrictToMarks()) return false;
    if(vp < 0 && !predicate->restrictToMarks()) return false;
    if(vo < 0 && !object->restrictToMarks()) return false;
    return true;
}

////////////////////////////////////////////////////////////////////////////////

FilterConstraint::FilterConstraint(Store *store, Expression *expr) :
        store(store), expr(expr) {
    VarInt** vars = expr->getVars().getCPVars();
    for(int i = 0; i < expr->getVars().getSize(); i++)
        vars[i]->registerBind(this);
}

bool FilterConstraint::propagate() {
    VariableSet &vars = expr->getVars();
    Variable* unbound = NULL;
    for(int i = 0; i < vars.getSize(); i++) {
        Variable *x = vars[i];
        if(x->getCPVariable()->contains(0))
            x->setValue(NULL);
        else if(x->getCPVariable()->isBound())
            x->setValue(store->getValue(x->getCPVariable()->getValue()));
        else if(unbound >= 0)
            return true; // too many unbound variables (> 1)
        else
            unbound = x;
    }
    if(!unbound) {
        // all variables are bound -> check
        return expr->isTrue();
    } else {
        // all variables, except one, are bound -> forward checking
        VarInt *x = unbound->getCPVariable();
        x->clearMarks();
        int n = x->getSize();
        const int* dom = x->getDomain();
        for(int i = 0; i < n; i++) {
            unbound->setValue(store->getValue(dom[i]));
            if(expr->isTrue())
                x->mark(dom[i]);
        }
        return x->restrictToMarks();
    }
}

////////////////////////////////////////////////////////////////////////////////

DiffConstraint::DiffConstraint(Query *query, VarVal v1, VarVal v2) :
        query(query), v1(v1), v2(v2) {
    x1 = v1.isVariable() ?
                query->getVariable(v1.getVariableId())->getCPVariable() : NULL;
    x2 = v2.isVariable() ?
                query->getVariable(v2.getVariableId())->getCPVariable() : NULL;
    if(x1 && x2) {
        x1->registerBind(this);
        x2->registerBind(this);
    }
}

bool DiffConstraint::post() {
    if(v1.isUnknown() || v2.isUnknown())
        return false;
    if(x1 && x2)
        return propagate();
    else if(x1)
        return x1->remove(v2.getValueId());
    else if(x2)
        return x2->remove(v1.getValueId());
    else
        return v1.getValueId() != v2.getValueId();
}

bool DiffConstraint::propagate() {
    if(x1->isBound())
        return x2->remove(x1->getValue());
    else if(x2->isBound())
        return x1->remove(x2->getValue());
    else
        return true;
}

////////////////////////////////////////////////////////////////////////////////

EqConstraint::EqConstraint(Query *query, VarVal v1, VarVal v2) :
        query(query), v1(v1), v2(v2) {
    x1 = v1.isVariable() ?
                query->getVariable(v1.getVariableId())->getCPVariable() : NULL;
    x2 = v2.isVariable() ?
                query->getVariable(v2.getVariableId())->getCPVariable() : NULL;
    if(x1 && x2) {
        x1->registerChange(this);
        x2->registerChange(this);
        restore();
    }
}

void EqConstraint::restore() {
    if(x1 && x2) {
        s1 = x1->getSize();
        s2 = x2->getSize();
    }
}

bool EqConstraint::post() {
    if(v1.isUnknown() || v2.isUnknown())
        return false;
    if(x1 && x2)
        return propagate();
    else if(x1)
        return x1->bind(v2.getValueId());
    else if(x2)
        return x2->bind(v2.getValueId());
    else
        return v1.getValueId() == v2.getValueId();
}

bool EqConstraint::propagate() {
    VarInt *x1 = this->x1, *x2 = this->x2;
    int n1 = x1->getSize(), n2 = x2->getSize();
    int oldn1 = s1, oldn2 = s2;
    int removed = (oldn1 - n1) + (oldn2 - n2);
    if(removed < n1 && removed < n2) {
        const int *dom = x1->getDomain();
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
        const int *dom = x1->getDomain();
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
