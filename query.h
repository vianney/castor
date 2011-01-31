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

/**
 * Opaque structure representing a query
 */
typedef struct TQuery Query;

#include "defs.h"
#include "model.h"
#include "expression.h"
#include "store.h"

/**
 * A statement pattern has the same structure as a statement, but allows
 * negative ids for designing variables. The variable number is retrieved with
 * -id - 1.
 */
typedef Statement StatementPattern;

/**
 * Query type
 */
typedef enum {
    QUERY_TYPE_SELECT,
    QUERY_TYPE_ASK
} QueryType;

////////////////////////////////////////////////////////////////////////////////
// Constructor and destructor

/**
 * Initialize a new query.
 *
 * @param store a store instance containing the values
 * @param queryString SPARQL query
 * @return the query instance or NULL if error
 */
Query* new_query(Store* store, char* queryString);

/**
 * Free a query instance
 *
 * @param self a query instance
 */
void free_query(Query* self);

////////////////////////////////////////////////////////////////////////////////
// Information

/**
 * @param self a query instance
 * @return the type of the query
 */
QueryType query_get_type(Query* self);

////////////////////////////////////////////////////////////////////////////////
// Variables

/**
 * @param self a query instance
 * @return number of variables in this query
 */
int query_variable_count(Query* self);

/**
 * @param self a query instance
 * @return number of requested variables in this query
 */
int query_variable_requested(Query* self);

/**
 * @param self a query instance
 * @param x variable number starting from 0
 * @return the value bound to variable x or NULL if unbound
 */
Value* query_variable_get(Query* self, int x);

/**
 * Bind a variable to a value for an expression evaluation.
 *
 * @param self a query instance
 * @param x variable number starting from 0
 * @param v value
 */
void query_variable_bind(Query* self, int x, Value* v);

////////////////////////////////////////////////////////////////////////////////
// Triple patterns

/**
 * @param self a query instance
 * @return number of triple patterns in the query
 */
int query_triple_pattern_count(Query* self);

/**
 * @param self a query instance
 * @param i number of a pattern within 0..query_triple_pattern_count(self)-1
 * @return the requested triple pattern
 */
StatementPattern* query_triple_pattern_get(Query* self, int i);

////////////////////////////////////////////////////////////////////////////////
// Filters

/**
 * @param self a query instance
 * @return number of filters in the query
 */
int query_filter_count(Query* self);

/**
 * @param self a query instance
 * @param i number of a pattern within 0..query_filter_count(self)-1
 * @return the requested filter
 */
Expression* query_filter_get(Query* self, int i);

#endif // QUERY_H
