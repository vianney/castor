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

#include "solver.h"

////////////////////////////////////////////////////////////////////////////////
// Constraint x != y + d

typedef struct {
    Constraint cstr;
    int x;
    int y;
    int d;
} DiffConstraint;

bool cstr_diff_propagate(Solver* solver, DiffConstraint* c) {
    if(solver_var_bound(solver, c->x))
        return solver_var_remove(solver, c->y,
                                 solver_var_value(solver, c->x) - c->d);
    else
        return solver_var_remove(solver, c->x,
                                 solver_var_value(solver, c->y) + c->d);
}

void post_diff(Solver* solver, int x, int y, int d) {
    DiffConstraint *c;

    CREATE_CONSTRAINT(c, DiffConstraint);
    c->x = x;
    c->y = y;
    c->d = d;
    c->cstr.propagate = (bool (*)(Solver*, Constraint*)) cstr_diff_propagate;
    if(solver_var_bound(solver, x) || solver_var_bound(solver, y)) {
        c->cstr.initPropagate = c->cstr.propagate;
    } else {
        solver_register_bind(solver, (Constraint*) c, x);
        solver_register_bind(solver, (Constraint*) c, y);
    }
    solver_post(solver, (Constraint*) c);
}


////////////////////////////////////////////////////////////////////////////////
// Program entry point

int comp(const int *a, const int *b) {
    return *b - *a;
}

int main(int argc, const char* argv[]) {
    int n, i, j, nbSols;
    Solver *solver;

    n = 8;
    if(argc >= 2)
        n = atoi(argv[1]);

    solver = new_solver(n, n);
    for(i = 0; i < n - 1; i++) {
        for(j = i + 1; j < n; j++) {
            post_diff(solver, i, j, 0);
            post_diff(solver, i, j, j - i);
            post_diff(solver, i, j, i - j);
        }
    }

    for(i = 0; i < n; i++)
        solver_add_order(solver, i, comp);

    nbSols = 0;
    while(solver_search(solver)) {
        nbSols++;
        printf("[");
        for(i = 0; i < n; i++)
            printf("%s%d", i == 0 ? "" : ", ", solver_var_value(solver, i));
        printf("]\n");
    }
    printf("Found %d solutions.\n", nbSols);
    solver_print_statistics(solver);

    free_solver(solver);
    return 0;
}
