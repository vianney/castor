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

#include "constraints.h"

////////////////////////////////////////////////////////////////////////////////
// Statement constraint

typedef struct {
    Store *store;
    StatementPattern stmt;
} StatementConstraint;

bool cstr_statement_propagate(Solver* solver, StatementConstraint* data) {
    int xs, xp, xo;
    int vs, vp, vo;
    Statement stmt;
    bool result;

#define INIT(p, part) \
    if(data->stmt.part >= 0) { \
        x ## p = -1; \
        v ## p = data->stmt.part; \
    } else { \
        x ## p = -data->stmt.part - 1; \
        v ## p = solver_var_bound(solver, x ## p) ? \
                 solver_var_value(solver, x ## p) : -1; \
    }

    INIT(s, subject)
    INIT(p, predicate)
    INIT(o, object)
#undef INIT

    if(!store_statement_query(data->store, vs, vp, vo)) goto error;

    if(vs >= 0 && vp >= 0 && vo >= 0) {
        // all variables are bound
        result = store_statement_fetch(data->store, NULL);
        if(!store_statement_finalize(data->store)) goto error;
        return result;
    }

    if(vs < 0) solver_var_clear_marks(solver, xs);
    if(vp < 0) solver_var_clear_marks(solver, xp);
    if(vo < 0) solver_var_clear_marks(solver, xo);
    while(store_statement_fetch(data->store, &stmt)) {
        if((vs < 0 && !solver_var_contains(solver, xs, stmt.subject)) ||
           (vp < 0 && !solver_var_contains(solver, xp, stmt.predicate)) ||
           (vo < 0 && !solver_var_contains(solver, xo, stmt.object)))
            continue;
        if(vs < 0) solver_var_mark(solver, xs, stmt.subject);
        if(vp < 0) solver_var_mark(solver, xp, stmt.predicate);
        if(vo < 0) solver_var_mark(solver, xo, stmt.object);
    }
    if(!store_statement_finalize(data->store)) goto error;
    if(vs < 0 && !solver_var_restrict_to_marks(solver, xs)) return false;
    if(vp < 0 && !solver_var_restrict_to_marks(solver, xp)) return false;
    if(vo < 0 && !solver_var_restrict_to_marks(solver, xo)) return false;
    return true;

error:
    fprintf(stderr, "castor statement constraint: error in store query\n");
    return false;
}

void post_statement(Solver* solver, Store* store, StatementPattern* stmt) {
    Constraint *c;
    StatementConstraint *data;

    c = solver_create_constraint(solver);
    data = (StatementConstraint*) malloc(sizeof(StatementConstraint));
    data->store = store;
    data->stmt = *stmt;
    c->userData = data;
    c->propagate = (bool (*)(Solver*, void*)) cstr_statement_propagate;
    c->initPropagate = c->propagate;
    if(stmt->subject < 0)
        solver_register_bind(solver, c, -stmt->subject - 1);
    if(stmt->predicate < 0)
        solver_register_bind(solver, c, -stmt->predicate - 1);
    if(stmt->object < 0)
        solver_register_bind(solver, c, -stmt->object - 1);
    solver_post(solver, c);
}

////////////////////////////////////////////////////////////////////////////////
// Generic filter constraint

typedef struct {
    Store *store;
    Query *query;
    Expression* expr;
    int nbVars;
    int* vars;
} FilterConstraint;

bool cstr_filter_propagate(Solver* solver, FilterConstraint* data) {
    int unbound, x, i, n;
    const int *dom;

    unbound = -1;
    for(i = 0; i < data->nbVars; i++) {
        x = data->vars[i];
        if(solver_var_bound(solver, x)) {
            query_variable_bind(data->query, x,
                                store_value_get(data->store,
                                                solver_var_value(solver, x)));
        } else if(unbound >= 0) {
            return true; // too many unbound variables (> 1)
        } else {
            unbound = x;
        }
    }
    if(unbound < 0) {
        // all variables are bound -> check
        return expression_is_true(data->query, data->expr);
    } else {
        // all variables, except one, are bound -> forward checking
        solver_var_clear_marks(solver, unbound);
        n = solver_var_size(solver, unbound);
        dom = solver_var_domain(solver, unbound);
        for(i = 0; i < n; i++) {
            query_variable_bind(data->query, unbound,
                                store_value_get(data->store, dom[i]));
            if(expression_is_true(data->query, data->expr))
                solver_var_mark(solver, unbound, dom[i]);
        }
        return solver_var_restrict_to_marks(solver, unbound);
    }
}

void cstr_filter_free(Solver* UNUSED(solver), FilterConstraint* data) {
    free(data->vars);
    free(data);
}

void post_filter(Solver* solver, Store* store, Query* query, Expression* expr) {
    Constraint *c;
    FilterConstraint *data;
    int i;

    c = solver_create_constraint(solver);
    data = (FilterConstraint*) malloc(sizeof(FilterConstraint));
    data->store = store;
    data->query = query;
    data->expr = expr;
    data->vars = (int*) malloc(query_variable_count(query) * sizeof(int));
    data->nbVars = expression_get_variables(query, expr, data->vars);
    data->vars = (int*) realloc(data->vars, data->nbVars * sizeof(int));
    c->userData = data;
    c->propagate = (bool (*)(Solver*, void*)) cstr_filter_propagate;
    c->initPropagate = c->propagate;
    c->free = (void (*)(Solver*, void*)) cstr_filter_free;
    for(i = 0; i < data->nbVars; i++)
        solver_register_bind(solver, c, data->vars[i]);
    solver_post(solver, c);
}

////////////////////////////////////////////////////////////////////////////////
// Diff constraint

typedef struct {
    Store *store;
    int x1;
    int x2;
} DiffConstraint;

bool cstr_diff_propagate(Solver* solver, DiffConstraint* data) {
    register int x1, x2, v1, v2;

    x1 = data->x1;
    x2 = data->x2;
    v1 = solver_var_bound(solver, x1) ? solver_var_value(solver, x1) : -1;
    v2 = solver_var_bound(solver, x2) ? solver_var_value(solver, x2) : -1;
    if(v1 >= 0 && v2 >= 0) {
        if(model_value_equal(store_value_get(data->store, v1),
                             store_value_get(data->store, v2)) == 1)
            return false;
    } else if(v1 >= 0) {
        if(!solver_var_remove(solver, x2, v1))
            return false;
    } else if(v2 >= 0) {
        if(!solver_var_remove(solver, x1, v2))
            return false;
    }
    return true;
}

void post_diff(Solver* solver, Store* store, int x1, int x2) {
    Constraint *c;
    DiffConstraint *data;

    c = solver_create_constraint(solver);
    data = (DiffConstraint*) malloc(sizeof(DiffConstraint));
    data->store = store;
    data->x1 = x1;
    data->x2 = x2;
    c->userData = data;
    c->propagate = (bool (*)(Solver*, void*)) cstr_diff_propagate;
    c->initPropagate = c->propagate;
    solver_register_bind(solver, c, x1);
    solver_register_bind(solver, c, x2);
    solver_post(solver, c);
}
