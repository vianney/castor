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
#include "variable.h"

#include "query.h"

namespace castor {

Variable::Variable(Query* query, unsigned id, const char* name) :
    query_(query), id_(id), name_(name),
    var_(query->solver(), 0, query->store()->valuesCount()), val_(0) {
}

void Variable::setFromCP() {
    if(var_.contains(0))
        valueId(0);
    else
        valueId(var_.value());
}

std::ostream& operator<<(std::ostream& out, const Variable& v) {
    out << "?" << v.name() << "_" << v.id();
    return out;
}

std::ostream& operator<<(std::ostream& out, const VarVal& v) {
    if(v.isVariable())
        out << "?" << v.variableId();
    else
        out << ":" << v.valueId();
    return out;
}

VariableSet::VariableSet(unsigned capacity) {
    capacity_ = capacity;
    vars_ = new Variable*[capacity];
    map_ = new bool[capacity];
    memset(map_, 0, capacity * sizeof(bool));
    size_ = 0;
}

VariableSet::VariableSet(Query* query) : VariableSet(query->variables().size()) {
}

VariableSet::VariableSet(const VariableSet& o) {
    capacity_ = o.capacity_;
    size_ = o.size_;
    vars_ = new Variable*[capacity_];
    memcpy(vars_, o.vars_, size_ * sizeof(Variable*));
    map_ = new bool[capacity_];
    memcpy(map_, o.map_, capacity_ * sizeof(bool));
}

VariableSet::~VariableSet() {
    delete [] vars_;
    delete [] map_;
}

VariableSet& VariableSet::operator=(const VariableSet& o) {
    memcpy(vars_, o.vars_, capacity_ * sizeof(Variable*));
    memcpy(map_, o.map_, capacity_ * sizeof(bool));
    size_ = o.size_;
    return *this;
}

VariableSet& VariableSet::operator+=(Variable* v) {
    if(!map_[v->id()]) {
        vars_[size_++] = v;
        map_[v->id()] = true;
    }
    return *this;
}

VariableSet& VariableSet::operator+=(const VariableSet& o) {
    for(unsigned i = 0; i < o.size_; i++)
        *this += o.vars_[i];
    return *this;
}

VariableSet VariableSet::operator*(const VariableSet& o) const {
    VariableSet result(capacity_);
    for(unsigned i = 0; i < size_; i++) {
        if(o.contains(vars_[i]))
            result += vars_[i];
    }
    return result;
}

}
