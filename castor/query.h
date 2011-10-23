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
#ifndef CASTOR_QUERY_H
#define CASTOR_QUERY_H

namespace castor {
    class Variable;
    class Solution;
    class VarVal;
    class VariableSet;
    class Query;
    class Pattern;
    class Expression;
    class DistinctConstraint;
    class BnBOrderConstraint;
}

#include <string>
#include <iostream>
#include <set>
#include "librdfwrapper.h"
#include "store.h"
#include "solver/solver.h"
#include "distinct.h"
#include "bnborder.h"
#include "util.h"

namespace castor {

/**
 * Exception while parsing the query
 */
class QueryParseException : public std::exception {
    std::string msg;

public:
    QueryParseException(std::string msg) : msg(msg) {}
    QueryParseException(const char* msg) : msg(msg) {}
    QueryParseException(const QueryParseException &o) : msg(o.msg) {}
    ~QueryParseException() throw() {}

    const char *what() const throw() { return msg.c_str(); }
};

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
    VarInt* getCPVariable() const { return var; }

    /**
     * Set the value of this variable using an id from the store
     * @param the id of the new value or 0 for unbound
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
    VarInt *var;        //!< CP variable
    Value::id_t val;    //!< value (0 means unbound)

    Variable() : var(NULL), val(0) {}
    ~Variable() {
        if(var != NULL)
            delete var;
    }

    friend class Query;
};

/**
 * A solution is a snapshot of the values assigned to the variables of a query.
 */
class Solution {
    Query *query;
    Value::id_t *values;
public:
    /**
     * Create a snapshot of the values currently assigned to the variables of
     * query.
     */
    Solution(Query *query);
    ~Solution();

    Value::id_t getValueId(int i) const { return values[i]; }
    Value::id_t getValueId(Variable &var) const { return getValueId(var.getId()); }

    /**
     * Assign the stored values to the variables of the query.
     */
    void restore() const;

    /**
     * Compare two solutions following the ordering given in the query.
     * @note this may change the values set in the query
     */
    bool operator<(const Solution &o) const;
    bool operator>(const Solution &o) const { return o < *this; }
};

/**
 * SPARQL query
 */
class Query {
public:
    /**
     * Initialize a new query.
     *
     * @param store a store containing the values
     * @param queryString SPARQL query
     * @throws QueryParseException on parse error
     */
    Query(Store *store, const char *queryString) throw(QueryParseException);
    ~Query();

    /**
     * @return the store associated to this query
     */
    Store* getStore() { return store; }
    /**
     * @return the CP solver
     */
    Solver* getSolver() { return &solver; }
    /**
     * @return the number of variables
     */
    unsigned getVariablesCount() const { return nbVars; }
    /**
     * @return the number of requested variables
     */
    unsigned getRequestedCount() const { return nbRequestedVars; }
    /**
     * @param id id of a variable (within 0..getVariablesCount()-1)
     * @return the variable with identifier id
     */
    Variable* getVariable(unsigned id) const { return &vars[id]; }
    /**
     * @return array of variables
     */
    Variable* getVariables() const { return vars; }
    /**
     * @return the graph pattern
     */
    Pattern* getPattern() const { return pattern; }
    /**
     * @return whether all returned solutions are distinct
     */
    bool isDistinct() const { return distinct; }
    /**
     * @return the limit on the number of solutions to return or
     *         -1 to return all
     */
    int getLimit() const { return limit; }
    /**
     * @return the number of ignored solutions in the beginning
     */
    unsigned getOffset() const { return offset; }

    /**
     * @return the number of ORDER BY expressions.
     */
    unsigned getOrderCount() const { return nbOrder; }
    /**
     * @param i index of an ORDER BY expression (within 0..getOrderCount()-1)
     * @return the ORDERY BY expression at index i
     */
    Expression* getOrder(unsigned i) const { return order[i]; }
    /**
     * @param i index of an ORDER BY expression (within 0..getOrderCount()-1)
     * @return whether ORDERY BY expression at index i should be in descending
     *         direction
     */
    bool isOrderDescending(unsigned i) const { return orderDescending[i]; }

    /**
     * @return the number of solutions found so far
     */
    unsigned getSolutionCount() const { return nbSols; }

    /**
     * Find the next solution
     * @return false if there are no more solutions, true otherwise
     */
    bool next();

    /**
     * Reset the search.
     */
    void reset();

private:
    /**
     * Create a graph pattern from a rasqal_graph_pattern
     *
     * @param gp a rasqal_graph_pattern
     * @return the new pattern
     * @throws QueryParseException on parse error
     */
    Pattern* convertPattern(rasqal_graph_pattern* gp) throw(QueryParseException);

    /**
     * Create an Expression from a rasqal_expression.
     *
     * @param expr the rasqal expression
     * @return the new expression
     * @throws QueryParseException on parse error
     */
    Expression* convertExpression(rasqal_expression* expr) throw(QueryParseException);

    /**
     * Copy a raptor_uri into a new string
     */
    char* convertURI(raptor_uri *uri);

    /**
     * @param literal a rasqal literal
     * @return the variable or value id of the literal in the store
     * @throws QueryParseException on parse error
     */
    VarVal getVarVal(rasqal_literal* literal) throw(QueryParseException);

    /**
     * Find the next solution of the pattern, updating the DISTINCT constraint
     * if appropriate, but without bothering about ORDER BY, LIMIT and OFFSET.
     * @return false if there are no more solutions, true otherwise
     */
    bool nextPatternSolution();

private:
    Store *store; //!< store associated to this query
    Solver solver; //!< CP solver
    unsigned nbVars; //!< number of variables
    unsigned nbRequestedVars; //!< number of requested variables
    /**
     * Array of variables. The requested variables come first.
     */
    Variable *vars;
    /**
     * Graph pattern
     */
    Pattern *pattern;
    /**
     * Should all solutions be distinct?
     */
    bool distinct;
    /**
     * Static distinct constraint or NULL if not needed.
     */
    DistinctConstraint *distinctCstr;
    /**
     * Limit of the number of solutions to return. -1 to return all.
     */
    int limit;
    /**
     * Number of solutions to ignore in the beginning.
     */
    unsigned offset;
    /**
     * Number of ORDER BY expressions.
     */
    unsigned nbOrder;
    /**
     * Array of ORDER BY expressions.
     */
    Expression **order;
    /**
     * Array of order directions. True for descending, false for ascending.
     */
    bool *orderDescending;
    /**
     * Number of solutions found so far.
     */
    unsigned nbSols;

    typedef std::multiset<Solution*,DereferenceLess> SolutionSet;
    /**
     * The solution set if we need to compute it a priori. Otherwise, it is
     * NULL.
     */
    SolutionSet *solutions;
    /**
     * Pointer to the next solution to return.
     */
    SolutionSet::iterator it;

    /**
     * Static constraint for Branch-and-Bound or NULL if not needed.
     */
    BnBOrderConstraint *bnbOrderCstr;
};

std::ostream& operator<<(std::ostream &out, const Query &q);

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

/**
 * Set of variables
 */
class VariableSet {
    void _init(unsigned capacity) {
        // TODO refactor this with C++11
        this->capacity = capacity;
        vars = new Variable*[capacity];
        varMap = new bool[capacity];
        memset(varMap, 0, capacity * sizeof(bool));
        size = 0;
        cpvars = NULL;
    }

public:
    VariableSet(unsigned capacity) {
        _init(capacity);
    }
    VariableSet(Query *query) {
        _init(query->getVariablesCount());
    }
    VariableSet(const VariableSet &o) {
        capacity = o.capacity;
        vars = new Variable*[capacity];
        memcpy(vars, o.vars, capacity * sizeof(Variable*));
        varMap = new bool[capacity];
        memcpy(varMap, o.varMap, capacity * sizeof(bool));
        size = o.size;
        cpvars = NULL;
    }
    ~VariableSet() {
        delete [] vars;
        delete [] varMap;
        if(cpvars)
            delete [] cpvars;
    }

    VariableSet& operator=(const VariableSet &o) {
        memcpy(vars, o.vars, capacity * sizeof(Variable*));
        memcpy(varMap, o.varMap, capacity * sizeof(bool));
        size = o.size;
        if(cpvars) {
            delete [] cpvars;
            cpvars = NULL;
        }
        return *this;
    }

    /**
     * Add a variable to this set
     */
    VariableSet& operator+=(Variable *v) {
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

    /**
     * Union with another set
     */
    VariableSet& operator+=(const VariableSet &o) {
        for(unsigned i = 0; i < o.size; i++)
            *this += o.vars[i];
        return *this;
    }

    /**
     * Intersection with another set
     */
    VariableSet operator*(const VariableSet &o) const {
        VariableSet result(capacity);
        for(unsigned i = 0; i < size; i++) {
            if(o.contains(vars[i]))
                result += vars[i];
        }
        return result;
    }

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
    VarInt** getCPVars() {
        if(!cpvars) {
            cpvars = new VarInt*[size];
            for(unsigned i = 0; i < size; i++)
                cpvars[i] = vars[i]->getCPVariable();
        }
        return cpvars;
    }

private:
    unsigned size; //!< number of variables in the list
    unsigned capacity; //!< maximum number of variables
    Variable **vars; //!< array of variables
    bool *varMap; //!< map of ids
    VarInt **cpvars; //!< List of CP variables. NULL if not yet created.
};

}

#include "pattern.h"

#endif // CASTOR_QUERY_H
