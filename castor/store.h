/* This file is part of Castor
 *
 * Author: Vianney le Clément de Saint-Marcq <vianney.leclement@uclouvain.be>
 * Copyright (C) 2010-2011, Université catholique de Louvain
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
#ifndef CASTOR_STORE_H
#define CASTOR_STORE_H

#include <string>
#include <exception>
#include "model.h"
#include "store/readutils.h"
#include "store/btree.h"

// magic number of store
#define CASTOR_STORE_MAGIC "\xd0\xd4\xc5\xd8" "Castor"

namespace castor {

/**
 * Store containing triples and values.
 * The triples encoding is modeled after RDF-3x [1].
 *
 * [1] http://www.mpi-inf.mpg.de/~neumann/rdf3x/
 */
class Store {
public:
    static const unsigned VERSION = 3; //!< format version

    /**
     * Open a store.
     *
     * @param fileName location of the store
     * @throws StoreException on error
     */
    Store(const char* fileName);
    ~Store();

    /**
     * Number of values in the store. The ids of the values will always be
     * between 1 and the returned value included.
     *
     * @return the number of values in the store or -1 if error
     */
    unsigned getValueCount() { return nbValues; }

    /**
     * Fetch a value from the store
     *
     * @param id identifier of the value (within range 1..getValueCount())
     * @param[out] val will contain the value
     */
    void fetchValue(Value::id_t id, Value &val);

    /**
     * Search for the id of a value (if id == 0) and replace it if found.
     *
     * @param[in,out] val the value to look for and update
     */
    void lookupId(Value &val);

    unsigned getStatTripleCacheHit() { return statTripleCacheHit; }
    unsigned getStatTripleCacheMiss() { return statTripleCacheMiss; }

private:
    PageReader db;

    unsigned triplesStart[3]; //!< start of triples tables in all orderings
    BTree<TripleKey>* triplesIndex[3]; //!< triples index in all orderings
    unsigned nbValues; //!< number of values
    unsigned valuesStart; //!< start of values table
    unsigned valuesMapping; //!< start of values mapping
    ValueHashTree* valuesIndex; //!< values index (hash->page mapping)

    static const unsigned TRIPLE_CACHE_SIZE = 100;
    static const unsigned TRIPLE_UNCACHED = 0xffffffff; //!< triple not in cache marker
    static const unsigned TRIPLE_CACHE_PAGESIZE = PageReader::PAGE_SIZE; //!< max triples in a page

    TripleKey *triples[TRIPLE_CACHE_SIZE]; //!< triples cache
    unsigned triplesCount[TRIPLE_CACHE_SIZE]; //!< number of triples in cache line
    unsigned triplesPage[TRIPLE_CACHE_SIZE]; //!< page number of the cache line
    unsigned triplesNextPage[TRIPLE_CACHE_SIZE]; //!< next page pointer cache

    unsigned *triplesMap; //!< map from page number to cach line
    unsigned triplesCached; //!< number of triple pages in cache
    unsigned triplesNext[TRIPLE_CACHE_SIZE];
    unsigned triplesPrev[TRIPLE_CACHE_SIZE];
    unsigned triplesHead, triplesTail;

    unsigned statTripleCacheHit;
    unsigned statTripleCacheMiss;

public:
    /**
     * Query the triples store.
     *
     * @note this class should be allocated on the stack instead of the heap
     *       as it contains a heavy triples cache
     */
    class StatementQuery {

        enum TripleOrder {
            SPO = 0, POS = 1, OSP = 2
        };

        Store *store;
        TripleKey key; //!< the key we are looking for
        TripleOrder order; //!< order of components in the key

        unsigned nextPage; //!< next page to read or 0 if no more

        TripleKey *it; //!< current triple
        TripleKey *end; //!< last triple in cache

    public:
        /**
         * Construct a new query
         *
         * @param store the store
         * @param stmt the triple to query, 0 for a component is a wildcard
         */
        StatementQuery(Store &store, Statement &stmt);

        /**
         * Fetch the next result statement
         *
         * @param[out] stmt structure in which to write the result or
         *                  NULL to ignore
         * @return true if stmt contains the next result, false if there are no
         *         more results (further calls to next() are undefined)
         */
        bool next(Statement *stmt);

    private:
        /**
         * Load the next page of triples. Updates nextPage and end.
         * Resets it to the beginning of the cache.
         *
         * While reading the page, skip any triple not matching key.
         *
         * @return false if there are no more pages, true otherwise
         */
        bool readNextPage();
    };
    friend class StatementQuery;
};

}

#endif // CASTOR_STORE_H
