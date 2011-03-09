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
#ifndef CONSTRAINTS_H
#define CONSTRAINTS_H

#include "defs.h"
#include "solver.h"
#include "store.h"
#include "query.h"

/**
 * Post a statement constraint
 *
 * @param solver a solver instance
 * @param store a store instance
 * @param stmt the statement pattern to match
 */
void post_statement(Solver* solver, Store* store, StatementPattern* stmt);

/**
 * Post a generic filter constraint
 *
 * @param solver a solver instance
 * @param store a store instance
 * @param expr the filter expression
 */
void post_filter(Solver* solver, Store* store, Expression* expr);

/**
 * Post the constraint x1 != x2
 *
 * @param solver a solver instance
 * @param store a store instance
 * @param x1 first variable
 * @param x2 second variable
 */
void post_diff(Solver* solver, Store* store, int x1, int x2);

/**
 * Post the constraint x1 = x2
 *
 * @param solver a solver instance
 * @param store a store instance
 * @param x1 first variable
 * @param x2 second variable
 */
void post_eq(Solver* solver, Store* store, int x1, int x2);

#endif // CONSTRAINTS_H
