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

#include <stdio.h>

typedef struct TVariable Variable;
typedef struct TQuery Query;

#include "defs.h"
#include "model.h"
#include "expression.h"
#include "pattern.h"
#include "store.h"

/**
 * Variable structure
 */
struct TVariable {
    /**
     * Index of the variable
     */
    int id;
    /**
     * Name of the variable or NULL if anonymous
     */
    char *name;
    /**
     * Value bound to the variable or NULL if unbound
     */
    Value *value;
};

/**
 * Structure representing a query
 */
struct TQuery {
    /**
     * Store associated to the query
     */
    Store *store;
    /**
     * Number of variables
     */
    int nbVars;
    /**
     * Number of requested variables
     */
    int nbRequestedVars;
    /**
     * Array of variables. The requested variables come first.
     */
    Variable* vars;
    /**
     * Graph pattern
     */
    Pattern* pattern;
    /**
     * Limit of the number of solutions to return (0 to return all)
     */
    int limit;
};

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
// Debugging

/**
 * Print this query
 *
 * @param self a query instance
 * @param f output file
 */
void query_print(Query* self, FILE* f);

#endif // QUERY_H
