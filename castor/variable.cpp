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

#include "variable.h"
#include "query.h"

namespace castor {

void Variable::setValueFromCP() {
    if(var->contains(0))
        setValueId(0);
    else
        setValueId(var->getValue());
}

std::ostream& operator<<(std::ostream &out, const Variable &v) {
    out << "?" << v.getName() << "_" << v.getId();
    return out;
}

std::ostream& operator<<(std::ostream &out, const VarVal &v) {
    if(v.isVariable())
        out << "?" << v.getVariableId();
    else
        out << ":" << v.getValueId();
    return out;
}

void VariableSet::_init(unsigned capacity) {
    // TODO refactor this with C++11
    this->capacity = capacity;
    vars = new Variable*[capacity];
    varMap = new bool[capacity];
    memset(varMap, 0, capacity * sizeof(bool));
    size = 0;
    cpvars = NULL;
}

VariableSet::VariableSet(Query *query) {
    _init(query->getVariablesCount());
}

VariableSet::VariableSet(const VariableSet &o) {
    capacity = o.capacity;
    vars = new Variable*[capacity];
    memcpy(vars, o.vars, capacity * sizeof(Variable*));
    varMap = new bool[capacity];
    memcpy(varMap, o.varMap, capacity * sizeof(bool));
    size = o.size;
    cpvars = NULL;
}

VariableSet::~VariableSet() {
    delete [] vars;
    delete [] varMap;
    if(cpvars)
        delete [] cpvars;
}

VariableSet& VariableSet::operator =(const VariableSet &o) {
    memcpy(vars, o.vars, capacity * sizeof(Variable*));
    memcpy(varMap, o.varMap, capacity * sizeof(bool));
    size = o.size;
    if(cpvars) {
        delete [] cpvars;
        cpvars = NULL;
    }
    return *this;
}

VariableSet& VariableSet::operator+=(Variable *v) {
    if(!varMap[v->getId()]) {
        vars[size++] = v;
        varMap[v->getId()] = true;
        if(cpvars) {
            delete [] cpvars;
            cpvars = NULL;
        }
    }
    return *this;
}

VariableSet& VariableSet::operator+=(const VariableSet &o) {
    for(unsigned i = 0; i < o.size; i++)
        *this += o.vars[i];
    return *this;
}

VariableSet VariableSet::operator*(const VariableSet &o) const {
    VariableSet result(capacity);
    for(unsigned i = 0; i < size; i++) {
        if(o.contains(vars[i]))
            result += vars[i];
    }
    return result;
}

cp::RDFVar** VariableSet::getCPVars() {
    if(!cpvars) {
        cpvars = new cp::RDFVar*[size];
        for(unsigned i = 0; i < size; i++)
            cpvars[i] = vars[i]->getCPVariable();
    }
    return cpvars;
}

}
