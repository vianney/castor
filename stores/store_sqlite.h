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
#ifndef SQLITE_H
#define SQLITE_H

/* Note: the newly added values and statements are effective only after the
 * store has been closed and reopened.
 */

#include "defs.h"
#include "store.h"

/**
 * Create a new sqlite store
 *
 * @param filename where to write the store
 * @return the new store or NULL if error
 */
Store* sqlite_store_create(const char* filename);

/**
 * Open an existing sqlite store
 *
 * @param filename location of the store
 * @return the new store or NULL if error
 */
Store* sqlite_store_open(const char* filename);

#endif // SQLITE_H
