/* This file is part of Castor
 *
 * Author: Vianney le Clément de Saint-Marcq <vianney.leclement@uclouvain.be>
 * Copyright (C) 2010 - UCLouvain
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
#ifndef QUERY_H
#define QUERY_H

#include "defs.h"
#include "model.h"

/**
 * Opaque structure representing a query
 */
typedef struct TQuery Query;

/**
 * Opaque pointer to a filter
 */
typedef void* Filter;

////////////////////////////////////////////////////////////////////////////////
// Constructor and destructor

/**
 * Initialize a new query.
 *
 * @param queryString SPARQL query
 * @return the query instance or NULL if error
 */
Query* new_query(char* queryString);

/**
 * Free a query instance
 *
 * @param self a query instance
 */
void free_query(Query* self);

////////////////////////////////////////////////////////////////////////////////
// Variables

/**
 * @param self a query instance
 * @return number of variables in this query
 */
int query_variables_count(Query* self);

/**
 * @param self a query instance
 * @return number of requested variables in this query
 */
int query_variables_requested(Query* self);

/**
 * Bind a variable to a value for a filter evaluation.
 *
 * @param self a query instance
 * @param x variable number starting from 0
 * @param v value
 */
void query_variable_set(Query* self, int x, Value* v);

////////////////////////////////////////////////////////////////////////////////
// Triple patterns

/**
 * Triple pattern visit callback.
 *
 * @param stmt statement, variables have negative indices
 * @param userData custom data passed to query_visit_triple_patterns
 */
typedef void (*query_triple_pattern_visit_fn)(Statement stmt, void* userData);

/**
 * Visit all the triple patterns in the query.
 *
 * @param self a query instance
 * @param fn callback function to be called for each triple pattern
 * @param userData custom data to pass on to the callback function
 */
void query_visit_triple_patterns(Query* self, query_triple_pattern_visit_fn fn,
                                 void* userData);

////////////////////////////////////////////////////////////////////////////////
// Filters

/**
 * Filter visit callback.
 *
 * @param filter pointer to the current filter
 * @param nbVars number of variables the filter uses
 * @param vars array of nbVars elements with the indices of the variables the
 *             filter uses
 * @param userData custom data passed to query_visit_filters
 */
typedef void (*query_filter_visit_fn)(Filter filter, int nbVars, int vars[],
                                      void* userData);

/**
 * Visit all the filters in the query.
 *
 * @param self a query instance
 * @param fn callback function to be called for each filter
 * @param userData custom data to pass on to the callback function
 */
void query_visit_filters(Query* self, query_filter_visit_fn fn, void* userData);

/**
 * Evaluate a filter with the current variable bindings.
 *
 * @param self a query instance
 * @param filter pointer to the filter to evaluate
 * @return the result of the filter evaluation
 */
bool query_filter_evaluate(Query* self, Filter filter);


#endif // QUERY_H
