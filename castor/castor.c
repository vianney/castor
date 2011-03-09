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
#include <sys/time.h>
#include <sys/resource.h>

#include "castor.h"
#include "solver.h"
#include "constraints.h"

struct TCastor {
    Store *store;
    Query *query;
    Solver *solver;
    bool started; // FIXME
};

Castor* new_castor(Store* store, Query* query) {
    Castor *self;

    self = (Castor*) malloc(sizeof(Castor));
    self->store = store;
    self->query = query;
    self->solver = new_solver(query_variable_count(query),
                              store_value_count(store) + 1);
    if(self->solver == NULL) {
        free(self);
        return NULL;
    }
    self->started = false;
    return self;
}

void free_castor(Castor* self) {
    free_solver(self->solver);
    free(self);
}

/**
 * Visit a filter, break top-level AND-clauses down and post filter constraints.
 *
 * @param solver the solver
 * @param store the store
 * @param query the query
 * @param expr the filter expression
 */
void visit_filter(Solver* solver, Store* store, Expression* expr) {
    int i;
    Value v;

    if(expr->op == EXPR_OP_AND) {
        visit_filter(solver, store, expr->arg1);
        visit_filter(solver, store, expr->arg2);
        return;
    } else if(expr->op == EXPR_OP_EQ) {
        if(expr->arg1->op == EXPR_OP_VARIABLE &&
           expr->arg2->op == EXPR_OP_VARIABLE) {
            post_eq(solver, store, expr->arg1->variable, expr->arg2->variable);
            return;
        } else if(expr->arg1->op == EXPR_OP_VARIABLE &&
                  expr->arg2->nbVars == 0) {
            if(!expression_evaluate(expr->arg2, &v)) {
                solver_fail(solver);
                return;
            }
            i = v.id >= 0 ? v.id :
                store_value_get_id(store, v.type, v.typeUri, v.lexical,
                                   v.languageTag);
            if(i < 0) {
                solver_fail(solver);
                return;
            }
            solver_label(solver, expr->arg1->variable, i);
            return;
        } else if(expr->arg2->op == EXPR_OP_VARIABLE &&
                  expr->arg1->nbVars == 0) {
            if(!expression_evaluate(expr->arg1, &v)) {
                solver_fail(solver);
                return;
            }
            i = v.id >= 0 ? v.id :
                store_value_get_id(store, v.type, v.typeUri, v.lexical,
                                   v.languageTag);
            if(i < 0) {
                solver_fail(solver);
                return;
            }
            solver_label(solver, expr->arg2->variable, i);
            return;
        }
    } else if(expr->op == EXPR_OP_NEQ) {
        if(expr->arg1->op == EXPR_OP_VARIABLE &&
           expr->arg2->op == EXPR_OP_VARIABLE) {
            post_diff(solver, store, expr->arg1->variable, expr->arg2->variable);
            return;
        } else if(expr->arg1->op == EXPR_OP_VARIABLE &&
                  expr->arg2->nbVars == 0) {
            if(!expression_evaluate(expr->arg2, &v)) {
                solver_fail(solver);
                return;
            }
            i = v.id >= 0 ? v.id :
                store_value_get_id(store, v.type, v.typeUri, v.lexical,
                                   v.languageTag);
            if(i < 0) {
                solver_fail(solver);
                return;
            }
            solver_diff(solver, expr->arg1->variable, i);
            return;
        } else if(expr->arg2->op == EXPR_OP_VARIABLE &&
                  expr->arg1->nbVars == 0) {
            if(!expression_evaluate(expr->arg1, &v)) {
                solver_fail(solver);
                return;
            }
            i = v.id >= 0 ? v.id :
                store_value_get_id(store, v.type, v.typeUri, v.lexical,
                                   v.languageTag);
            if(i < 0) {
                solver_fail(solver);
                return;
            }
            solver_diff(solver, expr->arg2->variable, i);
            return;
        }
    }
    // fallback generic implementation
    post_filter(solver, store, expr);
}

bool castor_next(Castor* self) {
    int i, n;

    if(!self->started) {
        // FIXME
        n = query_variable_count(self->query);
        int *vars = (int*) malloc(n * sizeof(int));
        for(i = 0; i < n; i++)
            vars[i] = i;
        solver_add_search(self->solver, vars, n);

        for(i = 0; i < n; i++) {
            solver_diff(self->solver, i, 0);
        }

        n = query_triple_pattern_count(self->query);
        for(i = 0; i < n; i++) {
            post_statement(self->solver, self->store,
                           query_triple_pattern_get(self->query, i));
        }

        n = query_filter_count(self->query);
        for(i = 0; i < n; i++) {
            visit_filter(self->solver, self->store,
                         query_filter_get(self->query, i));
        }
        self->started = true;
    }

    if(!solver_search(self->solver))
        return false;

    n = query_variable_requested(self->query);
    for(i = 0; i < n; i++) {
        if(solver_var_contains(self->solver, i, 0))
            query_variable_bind(self->query, i, NULL);
        else
            query_variable_bind(self->query, i,
                                store_value_get(self->store,
                                                solver_var_value(self->solver, i)));
    }

    return true;
}
