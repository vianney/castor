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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <rasqal.h>

#include "query.h"

////////////////////////////////////////////////////////////////////////////////
// Shared static information

rasqal_world *world = NULL;
int worldRefCount = 0;

inline void init_shared() {
    if(world == NULL) {
        world = rasqal_new_world();
        rasqal_world_open(world);
    }
    worldRefCount++;
}

inline void free_shared() {
    worldRefCount--;
    if(worldRefCount == 0) {
        rasqal_free_world(world);
        world = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Query interface implementation

struct TQuery {
    /**
     * rasqal query object
     */
    rasqal_query *query;
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
    rasqal_variable** variables;
};

////////////////////////////////////////////////////////////////////////////////
// Constructor and destructor

Query* new_query(char* queryString) {
    Query *self;
    raptor_sequence *seqVars, *seqAnon, *seqBound;
    int nbReal, nbAnon;
    int i, x;
    rasqal_variable *var;

    init_shared();
    self = (Query*) malloc(sizeof(Query));

    self->query = rasqal_new_query(world, "sparql", NULL);
    if(rasqal_query_prepare(self->query, (unsigned char*) queryString, NULL))
        goto cleanquery;

    seqVars = rasqal_query_get_all_variable_sequence(self->query);
    nbReal = raptor_sequence_size(seqVars);

    seqAnon = rasqal_query_get_anonymous_variable_sequence(self->query);
    nbAnon = raptor_sequence_size(seqAnon);

    seqBound = rasqal_query_get_bound_variable_sequence(self->query);
    self->nbRequestedVars = raptor_sequence_size(seqBound);

    self->nbVars = nbReal + nbAnon;
    self->variables = (rasqal_variable**) malloc(self->nbVars *
                                                 sizeof(rasqal_variable*));
    for(x = 0; x < self->nbRequestedVars; x++) {
        var = (rasqal_variable*) raptor_sequence_get_at(seqBound, x);
        self->variables[x] = var;
        var->user_data = &self->variables[x];
    }

    for(i = 0; i < nbReal; i++) {
        var = (rasqal_variable*) raptor_sequence_get_at(seqVars, i);
        if(var->user_data == NULL) {
            self->variables[x] = var;
            var->user_data = &self->variables[x];
            x++;
        }
    }

    for(i = 0; i < nbAnon; i++) {
        var = (rasqal_variable*) raptor_sequence_get_at(seqAnon, i);
        self->variables[x] = var;
        var->user_data = &self->variables[x];
        x++;
    }

    return self;

cleanquery:
    rasqal_free_query(self->query);
    free(self);
    free_shared();
    return NULL;
}

void free_query(Query* self) {
    free(self->variables);
    rasqal_free_query(self->query);
    free(self);
    free_shared();
}

////////////////////////////////////////////////////////////////////////////////
// Variables

int query_variables_count(Query* self) {
    return self->nbVars;
}

int query_variables_requested(Query* self) {
    return self->nbRequestedVars;
}

void query_variable_set(Query* self, int x, Value* v) {
    // TODO
}

////////////////////////////////////////////////////////////////////////////////
// Triple patterns

void query_visit_triple_patterns(Query* self, query_triple_pattern_visit_fn fn,
                                 void* userData) {
    // TODO
}

////////////////////////////////////////////////////////////////////////////////
// Filters

void query_visit_filters(Query* self, query_filter_visit_fn fn, void* userData) {
    // TODO
}

bool query_filter_evaluate(Query* self, Filter filter) {
    // TODO
    return false;
}
