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
 * @param stmt the statement to match
 */
void post_statement(Solver* solver, Store* store, Statement stmt);

/**
 * Post a generic filter constraint
 *
 * @param solver a solver instance
 * @param store a store instance
 * @param query a query instance
 * @param filter the filter
 * @param nbVars number of variables occuring in filter
 * @param vars the variable numbers occuring in filter
 */
void post_filter(Solver* solver, Store* store, Query* query, Filter filter,
                 int nbVars, int vars[]);

#endif // CONSTRAINTS_H
