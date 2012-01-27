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
    Query* getQuery() const { return query; }

    /**
     * @return id of the variable
     */
    unsigned getId() const { return id; }

    /**
     * @return the name of the variable (empty for an anonymous variable)
     */
    const std::string& getName() const { return name; }

    /**
     * @return the value id bound to this variable or 0 if unbound
     */
    Value::id_t getValueId() const { return val; }

    /**
     * @return whether the variable is bound
     */
    bool isBound() const { return val != 0; }

    /**
     * @return the CP variable corrsponding to this variable.
     */
    cp::RDFVar* getCPVariable() const { return var; }

    /**
     * Set the value of this variable using an id from the store
     * @param id the id of the new value or 0 for unbound
     */
    void setValueId(Value::id_t id) { val = id; }

    /**
     * Set the value of this variable according to the value of the CP variable
     */
    void setValueFromCP();

private:
    Query* query;       //!< Parent query
    unsigned id;        //!< Id of the variable
    std::string name;   //!< Name of the variable
    cp::RDFVar *var;   //!< CP variable
    Value::id_t val;    //!< value (0 means unbound)

    Variable() : var(nullptr), val(0) {}
    ~Variable() {
        if(var != nullptr)
            delete var;
    }

    friend class Query;
};

std::ostream& operator<<(std::ostream &out, const Variable &v);

/**
 * Small structure containing a value or variable id.
 */
class VarVal {
    Value::id_t valid; //!< id of the value or 0 if variable or unknown
    unsigned varid; //!< id of the variable + 1 or 0 if value or unknown

public:

    explicit VarVal(Value::id_t valid) : valid(valid), varid(0) {}
    VarVal(const Variable &variable) : valid(0), varid(variable.getId()+1) {}
    VarVal(const Variable *variable) : valid(0), varid(variable->getId()+1) {}
    VarVal(const Value &value) : valid(value.id), varid(0) {}
    VarVal(const Value *value) : valid(value->id), varid(0) {}

    /**
     * @return whether the id refers to a variable
     */
    bool isVariable() const { return varid > 0; }
    /**
     * @return whether this refers to an unknown value (id 0)
     */
    bool isUnknown() const { return valid == 0 && varid == 0; }
    /**
     * @pre !isVariable()
     * @return the value id
     */
    Value::id_t getValueId() const { return valid; }
    /**
     * @pre isVariable()
     * @return the variable id
     */
    unsigned getVariableId() const { return varid - 1; }

    bool operator==(const VarVal &o) const { return valid == o.valid && varid == o.varid; }
    bool operator!=(const VarVal &o) const { return valid != o.valid || varid != o.varid; }
};

std::ostream& operator<<(std::ostream &out, const VarVal &v);

/**
 * Set of variables
 */
class VariableSet {
    /**
     * Initialize the set.
     *
     * @param capacity maximum number of variables in this set
     *
     * @todo this could be refactored using C++11
     */
    void _init(unsigned capacity);

public:
    VariableSet(unsigned capacity) { _init(capacity); }
    VariableSet(Query *query);
    VariableSet(const VariableSet &o);
    ~VariableSet();

    VariableSet& operator=(const VariableSet &o);

    /**
     * Add a variable to this set
     */
    VariableSet& operator+=(Variable *v);

    /**
     * Union with another set
     */
    VariableSet& operator+=(const VariableSet &o);

    /**
     * Intersection with another set
     */
    VariableSet operator*(const VariableSet &o) const;

    unsigned getSize() const { return size; }
    Variable* operator[](unsigned i) { return vars[i]; }

    /**
     * @param v a variable
     * @return whether v is in this set
     */
    bool contains(Variable *v) const { return varMap[v->getId()]; }

    /**
     * @return the list of variables
     */
    const Variable** getList() const {
        return const_cast<const Variable**>(vars);
    }

    /**
     * @return the list of CP variables corresponding to the variables in the
     *         set
     */
    cp::RDFVar** getCPVars();

    // C++11 iterators
    Variable** begin() { return vars; }
    Variable** end() { return vars + size; }

private:
    unsigned size; //!< number of variables in the list
    unsigned capacity; //!< maximum number of variables
    Variable **vars; //!< array of variables
    bool *varMap; //!< map of ids
    cp::RDFVar **cpvars; //!< List of CP variables. nullptr if not yet created.
};

}

#endif // CASTOR_VARIABLE_H
