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
#ifndef CASTOR_VARIABLE_H
#define CASTOR_VARIABLE_H

#include "model.h"
#include "solver/discretevar.h"

namespace castor {

class Query;

namespace cp {
typedef DiscreteVariable<Value::id_t> RDFVar;
}

/**
 * SPARQL variable
 */
class Variable {
public:
    /**
     * @return parent query
     */
    Query* query() const { return query_; }

    /**
     * @return id of the variable
     */
    unsigned id() const { return id_; }

    /**
     * @return the name of the variable (empty for an anonymous variable)
     */
    const std::string& name() const { return name_; }

    /**
     * @return whether the variable is bound
     */
    bool isBound() const { return val_ != 0; }

    /**
     * @return the CP variable corrsponding to this variable.
     */
    cp::RDFVar* cp() { return &var_; }

    /**
     * @return the value id bound to this variable or 0 if unbound
     */
    Value::id_t valueId() const { return val_; }

    /**
     * Set the value of this variable using an id from the store
     * @param id the id of the new value or 0 for unbound
     */
    void valueId(Value::id_t id) { val_ = id; }

    /**
     * Set the value of this variable according to the value of the CP variable
     */
    void setFromCP();

private:
    /**
     * Variables are only meant to be created from Query. Hence, this
     * constructor is private.
     *
     * @param query the query
     * @param id index of this variable
     * @param name name of the variable (empty string for anonymous variables)
     */
    Variable(Query* query, unsigned id, const char* name);

    //! Non-copyable
    Variable(const Variable&) = delete;
    Variable& operator=(const Variable&) = delete;

    friend class Query;

private:
    Query*      query_; //!< Parent query
    unsigned    id_;    //!< Id of the variable
    std::string name_;  //!< Name of the variable
    cp::RDFVar  var_;   //!< CP variable
    Value::id_t val_;   //!< value (0 means unbound)
};

std::ostream& operator<<(std::ostream& out, const Variable& v);

/**
 * Small structure containing a value or variable id.
 */
class VarVal {
public:
    explicit VarVal(Value::id_t valid) : valid_(valid), varid_(0) {}
    VarVal(const Variable& variable) : valid_(0), varid_(variable.id()+1) {}
    VarVal(const Variable* variable) : valid_(0), varid_(variable->id()+1) {}
    VarVal(const Value& value) : valid_(value.id()), varid_(0) {}
    VarVal(const Value* value) : valid_(value->id()), varid_(0) {}

    VarVal(const VarVal&) = default;
    VarVal& operator=(const VarVal&) = default;

    /**
     * @return whether the id refers to a variable
     */
    bool isVariable() const { return varid_ > 0; }
    /**
     * @return whether this refers to an unknown value (id 0)
     */
    bool isUnknown() const { return valid_ == 0 && varid_ == 0; }
    /**
     * @pre !isVariable()
     * @return the value id
     */
    Value::id_t valueId() const { return valid_; }
    /**
     * @pre isVariable()
     * @return the variable id
     */
    unsigned variableId() const { return varid_ - 1; }

    bool operator==(const VarVal& o) const { return valid_ == o.valid_ && varid_ == o.varid_; }
    bool operator!=(const VarVal& o) const { return valid_ != o.valid_ || varid_ != o.varid_; }

private:
    Value::id_t valid_; //!< id of the value or 0 if variable or unknown
    unsigned    varid_; //!< id of the variable + 1 or 0 if value or unknown
};

std::ostream& operator<<(std::ostream& out, const VarVal& v);

/**
 * Set of variables
 */
class VariableSet {
public:
    VariableSet(unsigned capacity);
    VariableSet(Query* query);
    VariableSet(const VariableSet& o);
    ~VariableSet();

    VariableSet& operator=(const VariableSet& o);

    /**
     * Add a variable to this set
     */
    VariableSet& operator+=(Variable* v);

    /**
     * Union with another set
     */
    VariableSet& operator+=(const VariableSet& o);

    /**
     * Intersection with another set
     */
    VariableSet operator*(const VariableSet& o) const;

    unsigned size() const { return size_; }
    Variable* operator[](unsigned i) { return vars_[i]; }

    /**
     * @param v a variable
     * @return whether v is in this set
     */
    bool contains(Variable* v) const { return map_[v->id()]; }

    // C++11 iterators
    Variable** begin() const { return vars_; }
    Variable** end()   const { return vars_ + size_; }

private:
    unsigned   size_;     //!< number of variables in the list
    unsigned   capacity_; //!< maximum number of variables
    Variable** vars_;     //!< array of variables
    bool*      map_;      //!< map of ids
};

}

#endif // CASTOR_VARIABLE_H
