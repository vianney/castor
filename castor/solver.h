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
 * Structure for defining constraints. Implementations may "subclass" this
 * structure (i.e., make a structure with a Constraint as its first member).
 */
typedef struct TConstraint Constraint;
struct TConstraint {
    /**
     * Initial propagation callback. It is called during solver_post if it is
     * not NULL. It should perform the initial propagation and return true if
     * all went well or false if the propagation failed.
     */
    bool (*initPropagate)(Solver* solver, Constraint* c);
    /**
     * Propagate callback. It is called when an event this contraint has
     * registered to, has been triggered. It should propagate this event and
     * return true if all went well or false if the propagation failed.
     */
    bool (*propagate)(Solver* solver, Constraint* c);
    /**
     * Restore callback. It is called after a backtrack, if it is not NULL.
     * This is useful for resetting structures.
     */
    void (*restore)(Solver* solver, Constraint* c);
    /**
     * Free callback called before the whole structure is freed. Clean up your
     * structures here.
     */
    void (*free)(Solver* solver, Constraint* c);
    /**
     * Internal use by solver. No need to initialize.
     * Next constraint in linked list of posted constraints.
     */
    Constraint *next;
    /**
     * Internal use by solver. No need to initialize.
     * Next constraint in propagation queue.
     */
    Constraint *nextPropag;
    /**
     * Internal use by solver. No need to initialize.
     * Next constraint that has a non-NULL restore method.
     */
    Constraint *nextRestore;
};

/**
 * Macro to create and initialize a constraint of a certain subtype.
 */
#define CREATE_CONSTRAINT(var, Type) \
    (var) = (Type*) malloc(sizeof(Type)); \
    ((Constraint*)(var))->initPropagate = NULL; \
    ((Constraint*)(var))->propagate = NULL; \
    ((Constraint*)(var))->restore = NULL; \
    ((Constraint*)(var))->free = NULL;

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
 * Post a constraint. The solver takes ownership of the constraint.
 *
 * @param self a solver instance
 * @param c the constraint
 */
void solver_post(Solver* self, Constraint* c);

/**
 * Register constraint c to the bind event of constraint x. A constraint must
 * not register twice for the same variable.
 * This should be called just before solver_post.
 *
 * @param self a solver instance
 * @param c the constraint
 * @param x variable number
 */
void solver_register_bind(Solver* self, Constraint* c, int x);

/**
 * Register constraint c to the change event of constraint x. A constraint must
 * not register twice for the same variable.
 * This should be called just before solver_post.
 *
 * @param self a solver instance
 * @param c the constraint
 * @param x variable number
 */
void solver_register_change(Solver* self, Constraint* c, int x);

/**
 * Post the constraint x == v.
 * This has no effect once the search has begun.
 * Note that propagators have other means of manipulating the variables and
 * should not use this method.
 *
 * @param self a solver instance
 * @param x variable number
 * @param v value
 */
void solver_label(Solver* self, int x, int v);

/**
 * Post the constraint x != v.
 * This has no effect once the search has begun.
 * Note that propagators have other means of manipulating the variables and
 * should not use this method.
 *
 * @param self a solver instance
 * @param x variable number
 * @param v value
 */
void solver_diff(Solver* self, int x, int v);

/**
 * Post an always-inconsistent constraint.
 * This has no effect once the search has begun.
 * Note that propagators have other means of manipulating the variables and
 * should not use this method.
 *
 * @param self a solver instance
 */
void solver_fail(Solver* self);

////////////////////////////////////////////////////////////////////////////////
// Searching

/**
 * Add a search subtree. Constraints may be added after the call of this
 * function. They will be backtracked once the subtree is done.
 *
 * @param self a solver instance
 * @param vars the variables to label in the subtree
 * @param nbVars number of variables to label
 * @return depth of the new subtree (starting from 1), or 0 if no subtree has
 *         been added due to an inconsistent state
 */
int solver_add_search(Solver* self, int* vars, int nbVars);

/**
 * Discard the current subtree.
 *
 * @param self a solver instance
 * @return the depth of the discarded subtree (starting from 1) or 0 if there
 *         was no subtree
 */
int solver_discard_search(Solver* self);

/**
 * @return the depth of the current search subtree (starting from 1) or 0 if no
 *         subtree has been added or remains
 */
int solver_search_depth(Solver* self);

/**
 * Search for the next solution in the current subtree. No constraint may be
 * posted once the search has begun in the current subtree. When a subtree is
 * done (this function returns false), it is automatically discarded.
 *
 * @param self a solver instance
 * @return true if a solution has been found, false if the subtree is done
 */
bool solver_search(Solver* self);

////////////////////////////////////////////////////////////////////////////////
// Variable domains

// WARNING: no checks are done on the variable number x, only on value v

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
 * Get the domain array. Beware that this is a pointer directly to the internal
 * structure, so you shouldn't mess with it. Removing a value from the domain
 * only affects values after itself in the array. Marking a value only affects
 * values before itself.
 *
 * @param self a solver instance
 * @param x variable number
 * @return pointer to the domain array
 */
const int* solver_var_domain(Solver* self, int x);

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
