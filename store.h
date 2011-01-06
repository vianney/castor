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
#ifndef STORE_H
#define STORE_H

#include "defs.h"
#include "model.h"

/**
 * Store interface. Implementations may extend this structure.
 */
typedef struct TStore Store;
struct TStore {
    /**
     * Destructor
     */
    void (*close)(Store* self);

    // Values
    /**
     * Number of values in the store. The ids of the values will always be
     * between 0 and the returned value minus 1 included.
     *
     * @param self a store instance
     * @return the number of values in the store or -1 if error
     */
    int (*value_count)(Store* self);
    /**
     * Get a value from the store
     *
     * @param self a store instance
     * @param id identifier of the value (within range 0..value_count(self)-1)
     * @return the value corresponding to id or NULL if error
     */
    Value* (*value_get)(Store* self, int id);
//    /**
//     * Get a value from the store. If it does not exist within the store, create
//     * a new one with id -1. Such a value with id -1 should be freed by the
//     * caller when it's not needed anymore.
//     *
//     * @param self a store instance
//     * @param type datatype of the value
//     * @param typeUri URI of the datatype if type is VALUE_TYPE_UNKOWN
//     * @param lexical lexical form
//     * @param language language tag or NULL if none
//     * @return the value or NULL if error
//     */
//    Value* (*value_create)(Store* self, ValueType type, char* typeUri,
//                           char* lexical, char* language);

    // Statements
//    /**
//     * Add a statement to the store
//     *
//     * @param self a store instance
//     * @param source source URI (or blank node)
//     * @param predicate predicate URI
//     * @param object object value
//     * @return true if all went well, false if error
//     */
//    bool (*statement_add)(Store* self,
//                          Value* source, Value* predicate, Value* object);
    /**
     * Query the store for statements. Use statement_fetch to get the results.
     * Once the results are not needed anymore, statement_finalize should be
     * called.
     *
     * @param self a store instance
     * @param subject subject id or -1 for wildcard
     * @param predicate predicate id or -1 for wildcard
     * @param object object id or -1 for wildcard
     * @return true if all went well, false if error
     */
    bool (*statement_query)(Store* self, int subject, int predicate, int object);
    /**
     * Fetch the next result statement from the query initiated by
     * statement_query.
     *
     * @param self a store instance
     * @param stmt structure in which to write the result or NULL to ignore
     * @return true if stmt contains the next result, false if there are no more
     *         results
     */
    bool (*statement_fetch)(Store* self, Statement* stmt);
    /**
     * Finalize the query. This should always be called after statement_query
     * and after the potential calls to statement_fetch.
     *
     * @param self a store instance
     * @return true if all went well, false if error
     */
    bool (*statement_finalize)(Store* self);
};

// convenience shortcuts
inline void store_close(Store* self);
inline int store_value_count(Store* self);
inline Value* store_value_get(Store* self, int id);
//inline Value* store_value_create(Store* self, ValueType type, char* typeUri,
//                                 char* lexical, char* language);
//inline bool store_statement_add(Store* self, Value* source, Value* predicate,
//                                Value* object);
inline bool store_statement_query(Store* self, int source, int predicate,
                                  int object);
inline bool store_statement_fetch(Store* self, Statement* stmt);
inline bool store_statement_finalize(Store* self);

#endif // STORE_H
