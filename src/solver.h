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
typedef struct Solver Solver;

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
     * Propagate callback. It is called when an event this contraint has
     * registered to, has been triggered. It should propagate this event and
     * return true if all went well or false if the propagation failed.
     * Should be set during the posting if required.
     */
    bool (*propagate)(Solver* solver, void* userData);
    /**
     * Free callback. Clean up your structures (in particular user_data) here.
     * If nothing to do, leave as NULL.
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
 * @param maxCstrs indication of the number of constraints that will be posted
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
 * Post a constraint. A new Constraint structure will be initialized and post_cb
 * will be called. This function should initialize whathever structures needed,
 * set the propagate callbacks, register to variable events and perform the
 * initial pruning. If this initial pruning fails, it should return false.
 *
 * No constraints may be posted once the search has begun. They will be ignored.
 *
 * @param self a solver instance
 * @param post the posting callback function
 * @param userData custom data to be set in the Constraint structure
 */
void solver_post(Solver* self,
                 bool (*post)(Constraint* self, Solver* solver),
                 void* userData);

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

#endif // SOLVER_H
