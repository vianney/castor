#ifndef SQLITE_H
#define SQLITE_H

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
