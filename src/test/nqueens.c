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

#include "../solver.h"

////////////////////////////////////////////////////////////////////////////////
// Constraint x != y + d

typedef struct {
    int x;
    int y;
    int d;
} DiffConstraint;

bool cstr_diff_propagate(Solver* solver, void* userData) {
    DiffConstraint *data;

    data = (DiffConstraint*) userData;
    if(solver_var_bound(solver, data->x))
        return solver_var_remove(solver, data->y,
                                 solver_var_value(solver, data->x) - data->d);
    else
        return solver_var_remove(solver, data->x,
                                 solver_var_value(solver, data->y) + data->d);
}

void cstr_diff_free(Solver* solver, void* userData) {
    free(userData);
}

bool cstr_diff_post(Constraint* self, Solver* solver) {
    DiffConstraint *data;

    data = (DiffConstraint*) self->userData;
    self->propagate = cstr_diff_propagate;
    self->free = cstr_diff_free;

    if(solver_var_bound(solver, data->x) || solver_var_bound(solver, data->y)) {
        return cstr_diff_propagate(solver, data);
    } else {
        solver_register_bind(solver, self, data->x);
        solver_register_bind(solver, self, data->y);
        return true;
    }
}

void post_diff(Solver* solver, int x, int y, int d) {
    DiffConstraint *data;

    data = (DiffConstraint*) malloc(sizeof(DiffConstraint));
    data->x = x;
    data->y = y;
    data->d = d;
    solver_post(solver, cstr_diff_post, data);
}


////////////////////////////////////////////////////////////////////////////////
// Program entry point

int main(int argc, const char* argv[]) {
    int n, i, j, nbSols;
    Solver *solver;

    n = 8;
    if(argc >= 2)
        n = atoi(argv[1]);

    solver = new_solver(n, n, 3 * n * (n-1) / 2);
    for(i = 0; i < n - 1; i++) {
        for(j = i + 1; j < n; j++) {
            post_diff(solver, i, j, 0);
            post_diff(solver, i, j, j - i);
            post_diff(solver, i, j, i - j);
        }
    }

    nbSols = 0;
    while(solver_search(solver)) {
        nbSols++;
        printf("[");
        for(i = 0; i < n; i++)
            printf("%s%d", i == 0 ? "" : ", ", solver_var_value(solver, i));
        printf("]\n");
    }
    printf("Found %d solutions.\n", nbSols);

    free_solver(solver);
    return 0;
}
