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
#ifndef CASTOR_QUERY_H
#define CASTOR_QUERY_H

#include <string>
#include <iostream>
#include <set>
#include <vector>

#include "util.h"
#include "librdfwrapper.h"
#include "store.h"
#include "solver/solver.h"
#include "variable.h"

namespace castor {

class Pattern;
class Expression;
class DistinctConstraint;
class BnBOrderConstraint;

/**
 * A solution is a snapshot of the values assigned to the variables of a query.
 */
class Solution {
public:
    /**
     * Create a snapshot of the values currently assigned to the variables of
     * query.
     */
    Solution(Query* query);
    ~Solution();

    //! Non-copyable
    Solution(const Solution&) = delete;
    Solution& operator=(const Solution&) = delete;

    Value::id_t operator[](unsigned i)    const { return values_[i]; }
    Value::id_t operator[](Variable& var) const { return values_[var.id()]; }
    Value::id_t operator[](Variable* var) const { return values_[var->id()]; }

    /**
     * Assign the stored values to the variables of the query.
     */
    void restore() const;

    /**
     * Compare two solutions following the ordering given in the query.
     * @pre this->query == o.query
     * @note this may change the values set in the query
     */
    bool operator<(const Solution& o) const;
    bool operator>(const Solution& o) const { return o < *this; }

private:
    Query*       query_;
    Value::id_t* values_;
};

/**
 * Ordering clause in a query.
 */
class Order {
public:
    Order(Expression* expression, bool descending) :
        expression_(expression), descending_(descending) {}

    Order(const Order&) = default;
    Order& operator=(const Order&) = default;

    bool        isAscending()  const { return !descending_; }
    bool        isDescending() const { return  descending_; }
    Expression* expression()   const { return  expression_; }

private:
    Expression* expression_;
    bool        descending_;
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
     * @throws CastorException on parse error
     */
    Query(Store* store, const char* queryString);
    ~Query();

    //! Non-copyable
    Query(const Query&) = delete;
    Query& operator=(const Query&) = delete;

    Store*      store()  { return store_; }   //!< @return the associated store
    cp::Solver* solver() { return &solver_; } //!< @return the CP solver

    /**
     * @param id the id of a variable
     * @return the variable
     */
    Variable* variable(unsigned id) const { return vars_[id]; }
    /**
     * @param v the identifier of a variable
     * @pre v.isVariable()
     * @return the variable referred to by v
     */
    Variable* variable(VarVal v) const {
        assert(v.isVariable());
        return variable(v.variableId());
    }
    /**
     * @return the variables (requested variables first)
     */
    const std::vector<Variable*> variables() const { return vars_; }
    /**
     * @return the number of requested variables
     */
    unsigned requested() const { return requested_; }

    /**
     * @return the graph pattern
     */
    Pattern* pattern() const { return pattern_; }

    /**
     * @return whether all returned solutions are distinct
     */
    bool isDistinct() const { return distinct_; }
    /**
     * @return the limit on the number of solutions to return or
     *         -1 to return all
     */
    int limit() const { return limit_; }
    /**
     * @return the number of ignored solutions in the beginning
     */
    unsigned offset() const { return offset_; }

    /**
     * @return the ORDER BY clauses
     */
    const std::vector<Order> orders() const { return orders_; }

    /**
     * @return the number of solutions found so far
     */
    unsigned count() const { return nbSols_; }

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
     * @throws CastorException on parse error
     */
    Pattern* convert(rasqal_graph_pattern* gp);

    /**
     * Create an Expression from a rasqal_expression.
     *
     * @param expr the rasqal expression
     * @return the new expression
     * @throws CastorException on parse error
     */
    Expression* convert(rasqal_expression* expr);

    /**
     * @param literal a rasqal literal
     * @return the variable or value id of the literal in the store
     * @throws CastorException on parse error
     */
    VarVal lookup(rasqal_literal* literal);

    /**
     * Find the next solution of the pattern, updating the DISTINCT constraint
     * if appropriate, but without bothering about ORDER BY, LIMIT and OFFSET.
     * @return false if there are no more solutions, true otherwise
     */
    bool nextPatternSolution();

private:
    Store*     store_;  //!< store associated to this query
    cp::Solver solver_; //!< CP solver
    /**
     * Array of variables. The requested variables come first.
     */
    std::vector<Variable*> vars_;
    unsigned requested_; //!< number of requested variables
    /**
     * Graph pattern
     */
    Pattern* pattern_;
    /**
     * Should all solutions be distinct?
     */
    bool distinct_;
    /**
     * Static distinct constraint or nullptr if not needed.
     */
    DistinctConstraint* distinctCstr_;
    /**
     * Limit of the number of solutions to return. -1 to return all.
     */
    int limit_;
    /**
     * Number of solutions to ignore in the beginning.
     */
    unsigned offset_;
    /**
     * ORDER BY clauses
     */
    std::vector<Order> orders_;
    /**
     * Number of solutions found so far.
     */
    unsigned nbSols_;

    typedef std::multiset<Solution*,DereferenceLess> SolutionSet;
    /**
     * The solution set if we need to compute it a priori. Otherwise, it is
     * nullptr.
     */
    SolutionSet* solutions_;
    /**
     * Pointer to the next solution to return.
     */
    SolutionSet::iterator it_;

    /**
     * Static constraint for Branch-and-Bound or nullptr if not needed.
     */
    BnBOrderConstraint* bnbOrderCstr_;
};

std::ostream& operator<<(std::ostream& out, const Query& q);

}

#endif // CASTOR_QUERY_H
