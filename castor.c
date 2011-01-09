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

#include "defs.h"
#include "model.h"
#include "store.h"
#include "query.h"
#include "solver.h"
#include "constraints.h"

#include "stores/store_sqlite.h"

#define BUFFER_SIZE 1024

/**
 * Visit a filter, break top-level AND-clauses down and post filter constraints.
 *
 * @param solver the solver
 * @param store the store
 * @param query the query
 * @param expr the filter expression
 */
void visit_filter(Solver* solver, Store* store, Query* query, Expression* expr) {
    if(expr->op == EXPR_OP_AND) {
        visit_filter(solver, store, query, expr->arg1);
        visit_filter(solver, store, query, expr->arg2);
    } else if(expr->op == EXPR_OP_NEQ &&
              expr->arg1->op == EXPR_OP_VARIABLE &&
              expr->arg2->op == EXPR_OP_VARIABLE) {
        post_diff(solver, store, expr->arg1->variable, expr->arg2->variable);
    } else {
        post_filter(solver, store, query, expr);
    }
}

int main(int argc, char* argv[]) {
    char *dbpath, *rqpath;
    FILE *f;
    char *queryString, buffer[BUFFER_SIZE];
    size_t read;
    int queryLen;
    Store *store;
    Query *query;
    Solver *solver;
    int nbSols, i, n;
    char *str;

    if(argc < 2 || argc > 3) {
        printf("Usage: %s DB [QUERY]\n", argv[0]);
        return 1;
    }
    dbpath = argv[1];
    rqpath = argc > 2 ? argv[2] : NULL;

    queryString = NULL;
    queryLen = 0;
    f = rqpath == NULL ? stdin : fopen(rqpath, "r");
    if(f == NULL) {
        perror("castor");
        return 2;
    }
    while((read = fread(buffer, sizeof(char), BUFFER_SIZE, f)) > 0) {
        queryString = (char*) realloc(queryString, (queryLen + read + 1) * sizeof(char));
        memcpy(queryString + queryLen, buffer, read);
        queryLen += read;
    }
    if(ferror(f)) {
        perror("castor");
        fclose(f);
        return 2;
    }
    fclose(f);
    if(queryString == NULL) {
        fprintf(stderr, "Empty query\n");
        return 0;
    }
    queryString[queryLen] = '\0';

    store = sqlite_store_open(dbpath);
    if(store == NULL) {
        fprintf(stderr, "Unable to open %s\n", dbpath);
        goto error;
    }

    query = new_query(store, queryString);
    if(query == NULL) {
        fprintf(stderr, "Unable to parse query\n");
        goto cleanstore;
    }

    solver = new_solver(query_variable_count(query), store_value_count(store));
    if(solver == NULL) {
        fprintf(stderr, "Unable to initialize solver\n");
        goto cleanquery;
    }

    n = query_triple_pattern_count(query);
    for(i = 0; i < n; i++) {
        post_statement(solver, store, query_triple_pattern_get(query, i));
    }

    n = query_filter_count(query);
    for(i = 0; i < n; i++) {
        visit_filter(solver, store, query, query_filter_get(query, i));
    }

    nbSols = 0;
    while(solver_search(solver)) {
        nbSols++;
        n = query_variable_requested(query);
        for(i = 0; i < n; i++) {
            str = model_value_string(
                    store_value_get(store, solver_var_value(solver, i)));
            printf("%s ", str);
            free(str);
        }
        printf("\n");
    }
    printf("Found %d solutions\n", nbSols);
    solver_print_statistics(solver);

    free_solver(solver);
    free_query(query);
    store_close(store);
    free(queryString);
    return 0;

cleanquery:
    free_query(query);
cleanstore:
    store_close(store);
error:
    free(queryString);
    return 2;
}
