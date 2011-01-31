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
    Constraint cstr;
    Store *store;
    StatementPattern stmt;
} StatementConstraint;

bool cstr_statement_propagate(Solver* solver, StatementConstraint* c) {
    int xs, xp, xo;
    int vs, vp, vo;
    Statement stmt;
    bool result;

#define INIT(p, part) \
    if(c->stmt.part == 0) { \
        return false; \
    } else if(c->stmt.part > 0) { \
        x ## p = -1; \
        v ## p = c->stmt.part; \
    } else { \
        x ## p = -c->stmt.part - 1; \
        v ## p = solver_var_bound(solver, x ## p) ? \
                 solver_var_value(solver, x ## p) : -1; \
    }

    INIT(s, subject)
    INIT(p, predicate)
    INIT(o, object)
#undef INIT

    if(vs < 0 && vp < 0 && vo < 0) {
        // nothing bound, we do not want to check all triples
        return true;
    }

    if(!store_statement_query(c->store, vs, vp, vo)) goto error;

    if(vs >= 0 && vp >= 0 && vo >= 0) {
        // all variables are bound
        result = store_statement_fetch(c->store, NULL);
        if(!store_statement_finalize(c->store)) goto error;
        return result;
    }

    if(vs < 0) solver_var_clear_marks(solver, xs);
    if(vp < 0) solver_var_clear_marks(solver, xp);
    if(vo < 0) solver_var_clear_marks(solver, xo);
    while(store_statement_fetch(c->store, &stmt)) {
        if((vs < 0 && !solver_var_contains(solver, xs, stmt.subject)) ||
           (vp < 0 && !solver_var_contains(solver, xp, stmt.predicate)) ||
           (vo < 0 && !solver_var_contains(solver, xo, stmt.object)))
            continue;
        if(vs < 0) solver_var_mark(solver, xs, stmt.subject);
        if(vp < 0) solver_var_mark(solver, xp, stmt.predicate);
        if(vo < 0) solver_var_mark(solver, xo, stmt.object);
    }
    if(!store_statement_finalize(c->store)) goto error;
    if(vs < 0 && !solver_var_restrict_to_marks(solver, xs)) return false;
    if(vp < 0 && !solver_var_restrict_to_marks(solver, xp)) return false;
    if(vo < 0 && !solver_var_restrict_to_marks(solver, xo)) return false;
    return true;

error:
    fprintf(stderr, "castor statement constraint: error in store query\n");
    return false;
}

void post_statement(Solver* solver, Store* store, StatementPattern* stmt) {
    StatementConstraint *c;

    CREATE_CONSTRAINT(c, StatementConstraint);
    c->store = store;
    c->stmt = *stmt;
    c->cstr.propagate = (bool (*)(Solver*, Constraint*)) cstr_statement_propagate;
    c->cstr.initPropagate = c->cstr.propagate;
    if(stmt->subject < 0)
        solver_register_bind(solver, (Constraint*) c, -stmt->subject - 1);
    if(stmt->predicate < 0)
        solver_register_bind(solver, (Constraint*) c, -stmt->predicate - 1);
    if(stmt->object < 0)
        solver_register_bind(solver, (Constraint*) c, -stmt->object - 1);
    solver_post(solver, (Constraint*) c);
}

////////////////////////////////////////////////////////////////////////////////
// Generic filter constraint

typedef struct {
    Constraint cstr;
    Store *store;
    Query *query;
    Expression* expr;
    int nbVars;
    int* vars;
} FilterConstraint;

bool cstr_filter_propagate(Solver* solver, FilterConstraint* c) {
    int unbound, x, i, n;
    const int *dom;

    unbound = -1;
    for(i = 0; i < c->nbVars; i++) {
        x = c->vars[i];
        if(solver_var_bound(solver, x)) {
            query_variable_bind(c->query, x,
                                store_value_get(c->store,
                                                solver_var_value(solver, x)));
        } else if(unbound >= 0) {
            return true; // too many unbound variables (> 1)
        } else {
            unbound = x;
        }
    }
    if(unbound < 0) {
        // all variables are bound -> check
        return expression_is_true(c->query, c->expr);
    } else {
        // all variables, except one, are bound -> forward checking
        solver_var_clear_marks(solver, unbound);
        n = solver_var_size(solver, unbound);
        dom = solver_var_domain(solver, unbound);
        for(i = 0; i < n; i++) {
            query_variable_bind(c->query, unbound,
                                store_value_get(c->store, dom[i]));
            if(expression_is_true(c->query, c->expr))
                solver_var_mark(solver, unbound, dom[i]);
        }
        return solver_var_restrict_to_marks(solver, unbound);
    }
}

void cstr_filter_free(Solver* UNUSED(solver), FilterConstraint* c) {
    free(c->vars);
}

void post_filter(Solver* solver, Store* store, Query* query, Expression* expr) {
    FilterConstraint *c;
    int i;

    CREATE_CONSTRAINT(c, FilterConstraint);
    c->store = store;
    c->query = query;
    c->expr = expr;
    c->vars = (int*) malloc(query_variable_count(query) * sizeof(int));
    c->nbVars = expression_get_variables(query, expr, c->vars);
    c->vars = (int*) realloc(c->vars, c->nbVars * sizeof(int));
    c->cstr.free = (void (*)(Solver*, Constraint*)) cstr_filter_free;
    c->cstr.propagate = (bool (*)(Solver*, Constraint*)) cstr_filter_propagate;
    c->cstr.initPropagate = c->cstr.propagate;
    for(i = 0; i < c->nbVars; i++)
        solver_register_bind(solver, (Constraint*) c, c->vars[i]);
    solver_post(solver, (Constraint*) c);
}

////////////////////////////////////////////////////////////////////////////////
// Diff constraint

typedef struct {
    Constraint cstr;
    Store *store;
    int x1;
    int x2;
} DiffConstraint;

bool cstr_diff_propagate(Solver* solver, DiffConstraint* c) {
    register int x1, x2;

    x1 = c->x1;
    x2 = c->x2;
    if(solver_var_bound(solver, x1)) {
        if(!solver_var_remove(solver, x2, solver_var_value(solver, x1)))
            return false;
    } else {
        if(!solver_var_remove(solver, x1, solver_var_value(solver, x2)))
            return false;
    }
    return true;
}

void post_diff(Solver* solver, Store* store, int x1, int x2) {
    DiffConstraint *c;

    if(solver_var_bound(solver, x1)) {
        solver_diff(solver, x2, solver_var_value(solver, x1));
    } else if(solver_var_bound(solver, x2)) {
        solver_diff(solver, x1, solver_var_value(solver, x2));
    } else {
        CREATE_CONSTRAINT(c, DiffConstraint);
        c->store = store;
        c->x1 = x1;
        c->x2 = x2;
        c->cstr.propagate = (bool (*)(Solver*, Constraint*)) cstr_diff_propagate;
        solver_register_bind(solver, (Constraint*) c, x1);
        solver_register_bind(solver, (Constraint*) c, x2);
        solver_post(solver, (Constraint*) c);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Eq constraint

typedef struct {
    Constraint cstr;
    Store *store;
    int x1;
    int x2;
} EqConstraint;

bool cstr_eq_propagate(Solver* solver, EqConstraint* c) {
    register int x1, x2;

    x1 = c->x1;
    x2 = c->x2;
    if(solver_var_bound(solver, x1)) {
        if(!solver_var_bind(solver, x2, solver_var_value(solver, x1)))
            return false;
    } else {
        if(!solver_var_bind(solver, x1, solver_var_value(solver, x2)))
            return false;
    }
    return true;
}

void post_eq(Solver* solver, Store* store, int x1, int x2) {
    EqConstraint *c;

    if(solver_var_bound(solver, x1)) {
        solver_label(solver, x2, solver_var_value(solver, x1));
    } else if(solver_var_bound(solver, x2)) {
        solver_label(solver, x1, solver_var_value(solver, x2));
    } else {
        CREATE_CONSTRAINT(c, EqConstraint);
        c->store = store;
        c->x1 = x1;
        c->x2 = x2;
        c->cstr.propagate = (bool (*)(Solver*, Constraint*)) cstr_eq_propagate;
        solver_register_bind(solver, (Constraint*) c, x1);
        solver_register_bind(solver, (Constraint*) c, x2);
        solver_post(solver, (Constraint*) c);
    }
}
