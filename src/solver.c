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
#include <string.h>
#include <stdio.h>

#include "solver.h"

/**
 * Linked list for defining the search order
 */
typedef struct TSearchOrder SearchOrder;
struct TSearchOrder {
    /**
     * Next search order or NULL if end
     */
    SearchOrder* next;
    /**
     * Variable to bind
     */
    int x;
    /**
     * Compare function to sort the domain (casted for qsort)
     */
    int (*compar)(const void*, const void*);
};

/**
 * Checkpoint structure for backtracking
 */
typedef struct TCheckpoint Checkpoint;
struct TCheckpoint {
    /**
     * Next checkpoint in the trail or NULL if end.
     */
    Checkpoint* next;
    /**
     * Size of the domains
     */
    int* domSize;
    /**
     * Variable that has been chosen
     */
    int x;
    /**
     * Value that has been bound to the chosen variable just after the
     * checkpoint
     */
    int v;
    /**
     * Next search order to apply after assigning variable x
     */
    SearchOrder* nextOrder;
};

struct TSolver {
    // Domains
    /**
     * Number of variables
     */
    int nbVars;
    /**
     * Size of the largest domain
     */
    int maxVals;
    /**
     * domSize[x] = size of the domain of variable x
     */
    int* domSize;
    /**
     * domain[x][0..domSize[x]-1] = domain of variable x
     */
    int** domain;
    /**
     * domMap[x][v] = position of value v in domain[x]
     * domMap[x][v] = i <=> domain[x][i] = v
     * value v is in the domain of variable x <=> domMap[x][v] < domSize[x]
     */
    int** domMap;
    /**
     * domMarked[x] = number of marked values of variable x (always < domSize[x])
     * The marked values are domain[x][0..domMarked[x]-1].
     */
    int* domMarked;

    // Constraints
    /**
     * Number of constraints posted
     */
    int nbCstrs;
    /**
     * Maximum number of constraints that the current structures may hold.
     * Posting more constraints require expanding them.
     */
    int maxCstrs;
    /**
     * Array of posted constraints (size: maxCstr).
     */
    Constraint* constraints;
    /**
     * evBind[x] = number of constraints registered to the bind event of
     *     variable x
     */
    int* evBindSize;
    /**
     * evBind[x*maxCstrs..x*maxCstrs+evBind[x]] = constraints registered to the
     *     bind event of variable x
     */
    int* evBind;

    // Propagation
    /**
     * Size of the propagation queue.
     */
    int propagQueueSize;
    /**
     * propagQueue[0..propagQueueSize-1] = propagation queue
     */
    int* propagQueue;
    /**
     * cstrQueued[c] = is constraint c queued for propagation
     * cstrQueued[c] = true <=> ∃ i ∈ 0..propagQueueSize-1 : propagQueue[i] = c
     */
    bool* cstrQueued;

    // Search
    /**
     * Next order to consider or NULL to use minDom heuristic
     */
    SearchOrder* nextOrder;
    /**
     * First order in the list or NULL if no order has been specified.
     * Used for cleaning.
     */
    SearchOrder* firstOrder;
    /**
     * Last order in the list or NULL if no order has been specified.
     * Used to append orders.
     */
    SearchOrder* lastOrder;
    /**
     * true if the model is closed (either the search has begun or one of the
     * initial propagations failed)
     */
    bool closed;
    /**
     * Trail to backtrack
     */
    Checkpoint* trail;

    // Statistics
    /**
     * Number of backtracks so far
     */
    int statBacktracks;
};


////////////////////////////////////////////////////////////////////////////////
// Constructor and destructor

Solver* new_solver(int nbVars, int nbVals) {
    int x, v;
    int maxCstrs;
    Solver *self;
    int *dom, *map;

    self = (Solver*) malloc(sizeof(Solver));

    self->nbVars = nbVars;
    self->maxVals = nbVals;
    self->domSize = (int*) malloc(nbVars * sizeof(int));
    for(x = 0; x < nbVars; x++)
        self->domSize[x] = nbVals;
    self->domain = (int**) malloc(nbVars * sizeof(int*));
    self->domMap = (int**) malloc(nbVars * sizeof(int*));
    // create one big array for all the domains and just store pointers to the
    // relevant parts
    dom = (int*) malloc(nbVars * nbVals * sizeof(int));
    map = (int*) malloc(nbVars * nbVals * sizeof(int));
    self->domain[0] = dom;
    self->domMap[0] = map;
    for(v = 0; v < nbVals; v++) {
        dom[v] = v;
        map[v] = v;
    }
    for(x = 1; x < nbVars; x++) {
        self->domain[x] = &dom[x*nbVals];
        memcpy(self->domain[x], dom, nbVals * sizeof(int));
        self->domMap[x] = &map[x*nbVals];
        memcpy(self->domMap[x], map, nbVals * sizeof(int));
    }
    self->domMarked = (int*) calloc(nbVars, sizeof(int));

    self->nbCstrs = 0;
    maxCstrs = nbVars; // reserve nbVars constraints initially
    self->maxCstrs = maxCstrs;
    self->constraints = (Constraint*) malloc(maxCstrs * sizeof(Constraint));
    self->evBindSize = (int*) calloc(nbVars, sizeof(int));
    self->evBind = (int*) malloc(maxCstrs * nbVars * sizeof(int));

    self->propagQueueSize = 0;
    self->propagQueue = (int*) malloc(maxCstrs * sizeof(int));
    self->cstrQueued = (bool*) calloc(maxCstrs, sizeof(bool));

    self->nextOrder = NULL;
    self->firstOrder = NULL;
    self->lastOrder = NULL;
    self->closed = false;
    self->trail = NULL;

    self->statBacktracks = 0;

    return self;
}

void free_solver(Solver* self) {
    int i;
    Constraint *c;
    Checkpoint *chkp;
    SearchOrder *order;

    while(self->trail != NULL) {
        chkp = self->trail;
        self->trail = chkp->next;
        free(chkp->domSize);
        free(chkp);
    }
    while(self->firstOrder != NULL) {
        order = self->firstOrder;
        self->firstOrder = order->next;
        free(order);
    }
    for(i = 0; i < self->nbCstrs; i++) {
        c = &self->constraints[i];
        if(c->free != NULL)
            c->free(self, c->userData);
    }
    free(self->cstrQueued);
    free(self->propagQueue);
    free(self->evBind);
    free(self->evBindSize);
    free(self->constraints);
    free(self->domMarked);
    free(self->domMap[0]);
    free(self->domMap);
    free(self->domain[0]);
    free(self->domain);
    free(self->domSize);
    free(self);
}

////////////////////////////////////////////////////////////////////////////////
// Propagating

/**
 * Perform propagation of the constraints in the queue. After this call, either
 * the queue is empty and we have reached the fixpoint, or a failure has been
 * detected.
 *
 * @param self a solver instance
 * @return false if there is a failure, true otherwise
 */
bool propagate(Solver* self) {
    int cid;
    Constraint *c;

    while(self->propagQueueSize > 0) {
        self->propagQueueSize--;
        cid = self->propagQueue[self->propagQueueSize];
        c = &self->constraints[cid];
        if(!c->propagate(self, c->userData))
            return false;
        self->cstrQueued[cid] = false;
    }
    return true;
}

/**
 * Queue a constraint for propagation if it is not yet in the queue.
 *
 * @param self a solver instance
 * @param cid constraint id
 */
void queue_constraint(Solver* self, int cid) {
    if(!self->cstrQueued[cid]) {
        self->cstrQueued[cid] = true;
        self->propagQueue[self->propagQueueSize] = cid;
        self->propagQueueSize++;
    }
}

/**
 * Queue all constraints that are registered for the bind event of variable x
 *
 * @param self a solver instance
 * @param x variable number
 */
void queue_bind_event(Solver* self, int x) {
    int i;
    int *cstrs;

    cstrs = &self->evBind[x*self->maxCstrs];
    for(i = 0; i < self->evBindSize[x]; i++)
        queue_constraint(self, cstrs[i]);
}

////////////////////////////////////////////////////////////////////////////////
// Posting constraints

/**
 * Expand the structures to be able to hold more constraints.
 * This assumes the propagation queue is empty.
 *
 * @param self a solver instance
 */
void expand_max_constraints(Solver* self) {
    int x;
    int oldSize, newSize;
    int *evBind;
    Constraint *constraints;

    oldSize = self->maxCstrs;
    newSize = oldSize * 2;
    self->maxCstrs = newSize;

    constraints = (Constraint*) malloc(newSize * sizeof(Constraint));
    memcpy(constraints, self->constraints, oldSize * sizeof(Constraint));
    free(self->constraints);
    self->constraints = constraints;

    evBind = (int*) malloc(newSize * self->nbVars * sizeof(int));
    for(x = 0; x < self->nbVars; x++)
        memcpy(&evBind[x*newSize], &self->evBind[x*oldSize],
               self->evBindSize[x] * sizeof(int));
    free(self->evBind);
    self->evBind = evBind;

    free(self->propagQueue);
    self->propagQueue = (int*) malloc(newSize * sizeof(int));
    free(self->cstrQueued);
    self->cstrQueued = (bool*) calloc(newSize, sizeof(bool));
}

/**
 * Default free callback for new constraints.
 */
void default_free_cstr(Solver* solver, void* userData) {
    if(userData != NULL)
        free(userData);
}

Constraint* solver_create_constraint(Solver* self) {
    int id;
    Constraint *c;

    if(self->closed)
        return NULL;
    if(self->nbCstrs >= self->maxCstrs)
        expand_max_constraints(self);
    id = self->nbCstrs++;
    c = &self->constraints[id];
    c->id = id;
    c->userData = NULL;
    c->initPropagate = NULL;
    c->propagate = NULL;
    c->free = default_free_cstr;
    return c;
}

void solver_post(Solver* self, Constraint* c) {
    if(c->initPropagate != NULL) {
        self->cstrQueued[c->id] = true; // ignore events during initial propagation
        if(!c->initPropagate(self, c->userData)) {
            self->closed = true;
            return;
        }
        self->cstrQueued[c->id] = false;
    }
    if(!propagate(self))
        self->closed = true;
}

void solver_register_bind(Solver* self, Constraint* c, int x) {
    self->evBind[x*self->maxCstrs + self->evBindSize[x]++] = c->id;
}

////////////////////////////////////////////////////////////////////////////////
// Searching

void solver_add_order(Solver* self, int x,
                      int (*compar)(const int*,const int*)) {
    SearchOrder *order;

    if(self->closed)
        return;
    order = (SearchOrder*) malloc(sizeof(SearchOrder));
    order->next = NULL;
    order->x = x;
    order->compar = (int (*)(const void*, const void*)) compar;
    if(self->lastOrder == NULL) {
        self->firstOrder = order;
        self->lastOrder = order;
        self->nextOrder = order;
    } else {
        self->lastOrder->next = order;
        self->lastOrder = order;
    }
}

/**
 * Backtrack to the previous checkpoint, remove the chosen value from the
 * chosen variable and propagate. If the propagation leads to a failure,
 * backtrack to an older checkpoint.
 *
 * @param self a solver instance
 * @return the chosen variable of the last restored checkpoint or -1 if the
 *         whole search tree has been explored
 */
int backtrack(Solver* self) {
    int x, v;
    Checkpoint *chkp;

    self->statBacktracks++;
    if(self->trail == NULL)
        return -1;
    // restore domains
    chkp = self->trail;
    self->trail = chkp->next;
    free(self->domSize);
    self->domSize = chkp->domSize;
    x = chkp->x;
    v = chkp->v;
    self->nextOrder = chkp->nextOrder;
    free(chkp);
    // clear propagation queue
    self->propagQueueSize = 0;
    memset(self->cstrQueued, false, self->nbCstrs * sizeof(bool));
    // remove old (failed) choice
    if(!solver_var_remove(self, x, v)) {
        self->statBacktracks--; // branch finished: does not count as backtrack
        return backtrack(self);
    }
    if(!propagate(self))
        return backtrack(self);
    return x;
}

bool solver_search(Solver* self) {
    int x, y, sx, sy, v, i;
    Checkpoint *chkp;
    SearchOrder *order;

    x = -1;
    if(self->closed) { // the search has started, try to backtrack
        x = backtrack(self);
        if(x == -1)
            return false;
    } else {
        self->closed = true;
    }
    while(true) {
        // search for a variable to bind if needed
        if(x == -1 || solver_var_bound(self, x)) {
            x = -1;
            // first try the user-defined search orders
            while(self->nextOrder != NULL) {
                order = self->nextOrder;
                self->nextOrder = order->next;
                x = order->x;
                sx = self->domSize[x];
                if(sx > 1) {
                    /* Caution: this works only because the only event is bind.
                     * Otherwise, some values may be removed from the domain
                     * while propagating during backtrack and the order is not
                     * guaranteed anymore.
                     */
                    qsort(self->domain[x], sx, sizeof(int), order->compar);
                    for(i = 0; i < sx; i++)
                        self->domMap[x][self->domain[x][i]] = i;
                    break;
                }
                x = -1;
            }
            // then, find unbound variable with smallest domain
            if(x == -1) {
                sx = self->maxVals + 1;
                for(y = 0; y < self->nbVars; y++) {
                    sy = self->domSize[y];
                    if(sy > 1 && sy < sx) {
                        x = y;
                        sx = sy;
                    }
                }
                if(x == -1) // we have a solution
                    return true;
            }
        }
        // Take the last value of the domain
        v = self->domain[x][self->domSize[x]-1];
        // create a checkpoint
        chkp = (Checkpoint*) malloc(sizeof(Checkpoint));
        chkp->domSize = (int*) malloc(self->nbVars * sizeof(int));
        memcpy(chkp->domSize, self->domSize, self->nbVars * sizeof(int));
        chkp->x = x;
        chkp->v = v;
        chkp->nextOrder = self->nextOrder;
        chkp->next = self->trail;
        self->trail = chkp;
        // bind and propagate
        solver_var_bind(self, x, v); // should always return true
        if(!propagate(self)) {
            x = backtrack(self);
            if(x == -1)
                return false;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Variable domains

int solver_var_size(Solver* self, int x) {
    return self->domSize[x];
}

bool solver_var_bound(Solver* self, int x) {
    return self->domSize[x] == 1;
}

int solver_var_value(Solver* self, int x) {
    return self->domain[x][0];
}

const int* solver_var_domain(Solver* self, int x) {
    return self->domain[x];
}

bool solver_var_contains(Solver* self, int x, int v) {
    if(v < 0 || v >= self->maxVals)
        return false;
    return self->domMap[x][v] < self->domSize[x];
}

void solver_var_mark(Solver* self, int x, int v) {
    int i, m, v2;

    if(v < 0 || v >= self->maxVals)
        return;
    i = self->domMap[x][v];
    m = self->domMarked[x];
    if(i >= self->domSize[x] || i < m)
        return;
    if(i != m) {
        v2 = self->domain[x][m];
        self->domain[x][i] = v2;
        self->domain[x][m] = v;
        self->domMap[x][v2] = i;
        self->domMap[x][v] = m;
    }
    self->domMarked[x]++;
}

void solver_var_clear_marks(Solver* self, int x) {
    self->domMarked[x] = 0;
}

bool solver_var_bind(Solver* self, int x, int v) {
    int i, v2;

    solver_var_clear_marks(self, x);
    if(v < 0 || v >= self->maxVals)
        return false;
    i = self->domMap[x][v];
    if(i >= self->domSize[x])
        return false;
    if(self->domSize[x] == 1)
        return true;
    if(i != 0) {
        v2 = self->domain[x][0];
        self->domain[x][i] = v2;
        self->domain[x][0] = v;
        self->domMap[x][v2] = i;
        self->domMap[x][v] = 0;
    }
    self->domSize[x] = 1;
    queue_bind_event(self, x);
    return true;
}

bool solver_var_remove(Solver* self, int x, int v) {
    int i, sz, v2;

    solver_var_clear_marks(self, x);
    if(v < 0 || v >= self->maxVals)
        return true;
    i = self->domMap[x][v];
    sz = self->domSize[x];
    if(i >= sz)
        return true;
    if(sz <= 1)
        return false;
    sz--;
    if(i != sz) {
        v2 = self->domain[x][sz];
        self->domain[x][i] = v2;
        self->domain[x][sz] = v;
        self->domMap[x][v2] = i;
        self->domMap[x][v] = sz;
    }
    self->domSize[x] = sz;
    if(sz == 1)
        queue_bind_event(self, x);
    return true;
}

bool solver_var_restrict_to_marks(Solver* self, int x) {
    int m;

    m = self->domMarked[x];
    solver_var_clear_marks(self, x);
    if(m != self->domSize[x]) {
        self->domSize[x] = m;
        if(m == 0)
            return false;
        if(m == 1)
            queue_bind_event(self, x);
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Debug commands

void solver_print_domains(Solver* self) {
    int x, i;
    int *dom;

    for(x = 0; x < self->nbVars; x++) {
        printf("x%d: {", x);
        dom = self->domain[x];
        for(i = 0; i < self->domSize[x]; i++)
            printf("%s%d", i == 0 ? "" : ", ", dom[i]);
        printf("}\n");
    }
}

void solver_print_statistics(Solver* self) {
    printf("Backtracks: %d\n", self->statBacktracks);
}
