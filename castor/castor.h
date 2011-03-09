/* This file is part of Castor
 *
 * Author: Vianney le Clément de Saint-Marcq <vianney.leclement@uclouvain.be>
 * Copyright (C) 2010-2011, UCLouvain
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
#ifndef CASTOR_H
#define CASTOR_H

#include "defs.h"
#include "model.h"
#include "store.h"
#include "query.h"

/**
 * Opaque type for Castor instance
 */
typedef struct TCastor Castor;

/**
 * @param store an open store
 * @param query a query
 * @return a Castor instance
 */
Castor* new_castor(Store* store, Query* query);

/**
 * Free a Castor instance
 *
 * @param self a Castor instance
 */
void free_castor(Castor* self);

/**
 * Find the next solution
 *
 * @param self a Castor instance
 * @param false if there are no more solutions, true otherwise
 */
bool castor_next(Castor* self);

#endif // CASTOR_H
