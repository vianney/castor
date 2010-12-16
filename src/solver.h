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
#ifndef SOLVER_H
#define SOLVER_H

#include "defs.h"

/**
 * Opaque structure representing a solver instance
 */
typedef struct TSolver Solver;

/**
 * Structure for defining constraints.
 */
typedef struct {
    /**
     * Identifier of this constraint. Should not be modified.
     */
    int id;
    /**
     * User data associated to this constraint. This is set by solver_post to
     * the data that has been passed.
     */
    void* userData;
    /**
     * Initial propagation callback. It is called during solver_post if it is
     * not NULL. It should perform the initial propagation and return true if
     * all went well or false if the propagation failed.
     */
    bool (*initPropagate)(Solver* solver, void* userData);
    /**
     * Propagate callback. It is called when an event this contraint has
     * registered to, has been triggered. It should propagate this event and
     * return true if all went well or false if the propagation failed.
     */
    bool (*propagate)(Solver* solver, void* userData);
    /**
     * Free callback. Clean up your structures (in particular user_data) here.
     * The default is the free userData if it is not NULL.
     */
    void (*free)(Solver* solver, void* userData);
} Constraint;

////////////////////////////////////////////////////////////////////////////////
// Constructor and destructor

/**
 * Initialize a new solver.
 *
 * @param nbVars the number of variables
 * @param nbVals the size of the domains
 * @return the solver instance
 */
Solver* new_solver(int nbVars, int nbVals);

/**
 * Free a solver instance.
 *
 * @param self a solver instance
 */
void free_solver(Solver* self);

////////////////////////////////////////////////////////////////////////////////
// Posting constraints

/**
 * Create a new constraint. After this call, the user should complement the
 * structure by defining the callbacks and optionnaly setting userData. He
 * should also register to variable events. Finally, he should call solver_post
 * before any other constraint creation, posting or searching occurs.
 *
 * No constraints may be created once the search has begun.
 *
 * @param self a solver instance
 * @return constraint structure or NULL if the search has begun
 */
Constraint* solver_create_constraint(Solver* self);

/**
 * Post a constraint. This should be called after solver_create_constraint.
 *
 * @param self a solver instance
 * @param c the constraint
 */
void solver_post(Solver* self, Constraint* c);

/**
 * Register constraint c to the bind event of constraint x. A constraint must
 * not register twice for the same variable.
 * This should only be called when posting a constraint.
 *
 * @param self a solver instance
 * @param c the constraint
 * @param x variable number
 */
void solver_register_bind(Solver* self, Constraint* c, int x);

////////////////////////////////////////////////////////////////////////////////
// Searching

/**
 * Add an order to follow when searching. Beware that the values will be taken
 * in reverse order, from greater to smaller.
 *
 * No order can be added once the search has begun. They will be ignored.
 *
 * @param self a solver instance
 * @param x variable number to choose
 * @param compar comparison function for sorting the domain (see qsort)
 */
void solver_add_order(Solver* self, int x,
                      int (*compar)(const int*,const int*));

/**
 * Search for the next solution.
 *
 * @param self a solver instance
 * @return true if a solution has been found, false if the search is done
 */
bool solver_search(Solver* self);

////////////////////////////////////////////////////////////////////////////////
// Variable domains

// WARNING: no checks are done on the variable number x

/**
 * @param self a solver instance
 * @param x variable number
 * @return the current size of the domain of variable x
 */
int solver_var_size(Solver* self, int x);

/**
 * @param self a solver instance
 * @param x variable number
 * @return whether variable x is bound
 */
bool solver_var_bound(Solver* self, int x);

/**
 * @pre solver_var_bound(self, x) == true
 * @param self a solver instance
 * @param x variable number
 * @return the value bound to variable x
 */
int solver_var_value(Solver* self, int x);

/**
 * @param self a solver instance
 * @param x variable number
 * @return pointer to the domain array
 */
int* solver_var_domain(Solver* self, int x);

/**
 * @param self a solver instance
 * @param x variable number
 * @param v a value
 * @return whether value v is in the domain of variable x
 */
bool solver_var_contains(Solver* self, int x, int v);

/**
 * Mark a value in the domain of a variable. Do nothing if the value is not in
 * the domain.
 *
 * @param self a solver instance
 * @param x variable number
 * @param v the value to be marked
 */
void solver_var_mark(Solver* self, int x, int v);

/**
 * Clear marks of a variable.
 *
 * @param self a solver instance
 * @param x variable number
 */
void solver_var_clear_marks(Solver* self, int x);

/**
 * Bind a value to a variable. This also clear the marks of the variable.
 *
 * Should only be called during constraint propagation.
 *
 * @param self a solver instance
 * @param x variable number
 * @param v the value
 * @return false if the domain becomes empty, true otherwise
 */
bool solver_var_bind(Solver* self, int x, int v);

/**
 * Remove a value from the domain of a variable. This also clear the marks of
 * the variable.
 *
 * Should only be called during constraint propagation.
 *
 * @param self a solver instance
 * @param x variable number
 * @param v the value to remove
 * @return false if the domain becomes empty, true otherwise
 */
bool solver_var_remove(Solver* self, int x, int v);

/**
 * Restrict the domain of a variable to its marked values only. This also clear
 * the marks of the variable afterwards.
 *
 * Should only be called during constraint propagation.
 *
 * @param self a solver instance
 * @param x variable number
 * @return false if the domain becomes empty, true otherwise
 */
bool solver_var_restrict_to_marks(Solver* self, int x);

////////////////////////////////////////////////////////////////////////////////
// Debug commands

/**
 * Print the domains on the standard output
 *
 * @param self a solver instance
 */
void solver_print_domains(Solver* self);

/**
 * Print statistics of the solver on the standard output
 */
void solver_print_statistics(Solver* self);

#endif // SOLVER_H
