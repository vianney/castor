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

    if(query->pattern->type != PATTERN_TYPE_BASIC &&
       !(query->pattern->type == PATTERN_TYPE_FILTER &&
         query->pattern->left->type == PATTERN_TYPE_BASIC)) {
        fprintf(stderr, "castor: only simple queries supported for now\n");
        return NULL;
    }

    self = (Castor*) malloc(sizeof(Castor));
    self->store = store;
    self->query = query;
    self->solver = new_solver(query->nbVars,
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
            post_eq(solver, store, expr->arg1->variable->id,
                    expr->arg2->variable->id);
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
            solver_label(solver, expr->arg1->variable->id, i);
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
            solver_label(solver, expr->arg2->variable->id, i);
            return;
        }
    } else if(expr->op == EXPR_OP_NEQ) {
        if(expr->arg1->op == EXPR_OP_VARIABLE &&
           expr->arg2->op == EXPR_OP_VARIABLE) {
            post_diff(solver, store, expr->arg1->variable->id,
                      expr->arg2->variable->id);
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
            solver_diff(solver, expr->arg1->variable->id, i);
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
            solver_diff(solver, expr->arg2->variable->id, i);
            return;
        }
    }
    // fallback generic implementation
    post_filter(solver, store, expr);
}

bool castor_next(Castor* self) {
    int i, n;
    Pattern *pat;

    if(!self->started) {
        pat = self->query->pattern;

        solver_add_search(self->solver, pat->vars, pat->nbVars);

        for(i = 0; i < pat->nbVars; i++) {
            solver_diff(self->solver, i, 0);
        }

        if(pat->type == PATTERN_TYPE_FILTER) {
            visit_filter(self->solver, self->store, pat->expr);
            pat = pat->left;
        }

        for(i = 0; i < pat->nbTriples; i++) {
            post_statement(self->solver, self->store, &pat->triples[i]);
        }

        self->started = true;
    }

    if(!solver_search(self->solver))
        return false;

    n = self->query->nbVars;
    for(i = 0; i < n; i++) {
        if(solver_var_contains(self->solver, i, 0))
            self->query->vars[i].value = NULL;
        else
            self->query->vars[i].value =
                    store_value_get(self->store,
                                    solver_var_value(self->solver, i));
    }

    return true;
}
