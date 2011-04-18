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
    class VarVal;
    class VariableSet;
    class Query;
    class Pattern;
    class Expression;
}

#include <string>
#include <iostream>
#include <rasqal.h>
#include "store.h"
#include "solver/solver.h"

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
    int getId() const { return id; }

    /**
     * @return the value bound to this variable or NULL if unbound
     */
    Value* getValue() const { return value; }

    /**
     * @return whether the variable is bound
     */
    bool isBound() const { return value != NULL; }

    /**
     * @return the CP variable corrsponding to this variable.
     */
    VarInt* getCPVariable() const { return var; }

    /**
     * Set the value of this variable.
     * @param value the new value or NULL for unbound
     */
    void setValue(Value *value) { this->value = value; }

    /**
     * Set the value of this variable according to the value of the CP variable
     */
    void setValueFromCP();

private:
    Query* query;       //!< Parent query
    int id;             //!< Id of the variable
    std::string name;   //!< Name of the variable
    VarInt *var;        //!< CP variable
    Value *value;       //!< Value

    Variable() : var(NULL), value(NULL) {}

    friend class Query;
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
    Query(Store *store, char *queryString) throw(QueryParseException);
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
    int getVariablesCount() const { return nbVars; }
    /**
     * @return the number of requested variables
     */
    int getRequestedCount() const { return nbRequestedVars; }
    /**
     * @param id id of a variable (within 0..getVariablesCount())
     * @return the variable with identifier id
     */
    Variable* getVariable(int id) const { return &vars[id]; }
    /**
     * @return array of variables
     */
    Variable* getVariables() const { return vars; }
    /**
     * @return the graph pattern
     */
    Pattern* getPattern() const { return pattern; }
    /**
     * @return the limit on the number of solutions to return or
     *         -1 to return all
     */
    int getLimit() const { return limit; }

    /**
     * @return the number of solutions found so far
     */
    int getSolutionCount() const { return nbSols; }

    /**
     * Find the next solution
     * @param false if there are no more solutions, true otherwise
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
     * @return the variable or value id of the literal in the store. Id is 0 if
     *         unknown.
     * @throws QueryParseException on parse error
     */
    VarVal getVarVal(rasqal_literal* literal) throw(QueryParseException);

private:
    Store *store; //!< store associated to this query
    Solver solver; //!< CP solver
    int nbVars; //!< number of variables
    int nbRequestedVars; //!< number of requested variables
    /**
     * Array of variables. The requested variables come first.
     */
    Variable *vars;
    /**
     * Graph pattern
     */
    Pattern *pattern;
    /**
     * Limit of the number of solutions to return. -1 to return all.
     */
    int limit;
    /**
     * Number of solutions found so far.
     */
    int nbSols;
};

std::ostream& operator<<(std::ostream &out, const Query &q);

/**
 * Small structure containing a value or variable id.
 */
struct VarVal {
    /**
     * The id. Negative ids design variables. The variable number is retrieved
     * with -id - 1.
     */
    int id;

    explicit VarVal(int id) : id(id) {}
    VarVal(const Variable &variable) : id(-variable.getId()-1) {}
    VarVal(const Variable *variable) : id(-variable->getId()-1) {}
    VarVal(const Value &value) : id(value.id) {}
    VarVal(const Value *value) : id(value->id) {}

    /**
     * @return whether the id refers to a variable
     */
    bool isVariable() const { return id < 0; }
    /**
     * @return whether this refers to an unknown value (id 0)
     */
    bool isUnknown() const { return id == 0; }
    /**
     * @pre !isVariable()
     * @return the value id
     */
    int getValueId() const { return id; }
    /**
     * @pre isVariable()
     * @return the variable id
     */
    int getVariableId() const { return -id-1; }

    bool operator==(const VarVal &o) { return id == o.id; }
    bool operator!=(const VarVal &o) { return id != o.id; }
};

/**
 * Set of variables
 */
class VariableSet {
public:
    VariableSet(Query *query) {
        capacity = query->getVariablesCount();
        vars = new Variable*[capacity];
        varMap = new bool[capacity];
        memset(varMap, 0, query->getVariablesCount() * sizeof(bool));
        size = 0;
        cpvars = NULL;
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

    VariableSet& operator=(VariableSet &o) {
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
    VariableSet& operator+=(VariableSet &o) {
        for(int i = 0; i < o.size; i++)
            *this += o.vars[i];
        return *this;
    }

    int getSize() const { return size; }
    Variable* operator[](int i) { return vars[i]; }

    /**
     * @param v a variable
     * @return whether v is in this set
     */
    bool contains(Variable *v) const { return varMap[v->getId()]; }

    /**
     * @return the list of variables
     */
    const Variable** getList() const { return (const Variable**) vars; }

    /**
     * @return the list of CP variables corresponding to the variables in the
     *         set
     */
    VarInt** getCPVars() {
        if(!cpvars) {
            cpvars = new VarInt*[size];
            for(int i = 0; i < size; i++)
                cpvars[i] = vars[i]->getCPVariable();
        }
        return cpvars;
    }

private:
    int size; //!< number of variables in the list
    int capacity; //!< maximum number of variables
    Variable **vars; //!< array of variables
    bool *varMap; //!< map of ids
    VarInt **cpvars; //!< List of CP variables. NULL if not yet created.
};

}

#include "pattern.h"

#endif // CASTOR_QUERY_H
