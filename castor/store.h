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
#include "store/triplecache.h"

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
    static const unsigned VERSION = 6; //!< format version

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
     * @return range of values of a class in the store
     */
    ValueRange getClassValues(Value::Class cls) {
        ValueRange result = {valuesClassStart[cls],
                             valuesClassStart[cls+1] - 1};
        return result;
    }

    /**
     * @return range of values spanning given classes in the store
     */
    ValueRange getClassValues(Value::Class from, Value::Class to) {
        ValueRange result = {valuesClassStart[from],
                             valuesClassStart[to+1] - 1};
        return result;
    }

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

    /**
     * @param id the identifier of a value in the store
     * @return the equivalence class of that value
     */
    ValueRange getValueEqClass(Value::id_t id);

    /**
     * Get the equivalence class of a value. If val.id > 0, this is equivalent
     * to getValueEqClass(val.id). Otherwise, it finds an equivalence class in
     * the store. If there is no equivalent value in the store, the returned
     * range will be empty (from == to + 1), but still denote the
     * glb (from - 1) and lub (to + 1).
     *
     * To summarize, the following assertions hold (using SPARQL compare):
     * v.id < result.from   <=>  v < val
     * v.id > result.to     <=>  v > val
     * v.id <= result.to    <=>  v <= val
     * v.id >= result.from  <=>  v >= val
     *
     * @pre val.ensureInterpreted()
     */
    ValueRange getValueEqClass(const Value &val);

    /**
     * @param id the identifier of a value in the store
     * @return the class of the value
     */
    Value::Class getValueClass(Value::id_t id);

    unsigned getStatTripleCacheHit()  { return cache.getStatHits(); }
    unsigned getStatTripleCacheMiss() { return cache.getStatMisses(); }

private:
    PageReader db;

    unsigned triplesStart[3]; //!< start of triples tables in all orderings
    BTree<Triple>* triplesIndex[3]; //!< triples index in all orderings
    unsigned nbValues; //!< number of values
    unsigned valuesStart; //!< start of values table
    unsigned valuesMapping; //!< start of values mapping
    ValueHashTree* valuesIndex; //!< values index (hash->page mapping)
    unsigned valuesEqClasses; //!< start of value equivalence classes boundaries
    Value::id_t valuesClassStart[Value::CLASSES_COUNT + 1]; //!< first id of each class

    TripleCache cache; //!< triples cache

public:
    /**
     * Query a range of triples.
     *
     * @note concurrent queries result in undefined behaviour
     */
    class RangeQuery {
    public:
        /**
         * Construct a new query.
         * @param store the store
         * @param from lower bound
         * @param to upper bound
         */
        RangeQuery(Store *store, Triple from, Triple to);

        /**
         * Fetch the next result statement
         *
         * @param[out] t structure in which to write the result or
         *               NULL to ignore
         * @return true if the next result has been found, false if there are
         *         no more results (further calls to next() are undefined)
         */
        bool next(Triple *t);

    private:
        enum TripleOrder {
            SPO = 0, POS = 1, OSP = 2
        };

        Store *store;
        Triple limit; //!< the upper bound
        TripleOrder order; //!< order of components in the key

        unsigned nextPage; //!< next page to read or 0 if no more

        const Triple *it; //!< current triple
        const Triple *end; //!< last triple in current cache line
    };

    friend class RangeQuery;
};

}

#endif // CASTOR_STORE_H
