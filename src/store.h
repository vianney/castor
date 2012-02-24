/* This file is part of Castor
 *
 * Author: Vianney le Clément de Saint-Marcq <vianney.leclement@uclouvain.be>
 * Copyright (C) 2010-2012, Université catholique de Louvain
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef CASTOR_STORE_H
#define CASTOR_STORE_H

#include <string>
#include <exception>

#include "model.h"
#include "store/readutils.h"
#include "store/btree.h"
#include "store/triplecache.h"

namespace castor {

/**
 * Store containing triples and values.
 * The triples encoding is modeled after RDF-3x [1].
 *
 * [1] http://www.mpi-inf.mpg.de/~neumann/rdf3x/
 */
class Store {
public:
    static constexpr unsigned VERSION = 6; //!< format version
    static const     char     MAGIC[10];   //!< magic number

    enum TripleOrder {
        SPO = 0, POS = 1, OSP = 2
    };
    static constexpr int ORDERS = 3;

    /**
     * Open a store.
     *
     * @param fileName location of the store
     * @throws CastorException on error
     */
    Store(const char* fileName);
    ~Store();

    //! Non-copyable
    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;

    /**
     * Number of values in the store. The ids of the values will always be
     * between 1 and the returned value included.
     *
     * @return the number of values in the store or -1 if error
     */
    unsigned valuesCount() { return values_.count; }

    /**
     * @return range of values of a category in the store
     */
    ValueRange range(Value::Category cat) {
        ValueRange result = {values_.categories[cat],
                             values_.categories[cat+1] - 1};
        return result;
    }

    /**
     * @return range of values spanning given categories in the store
     */
    ValueRange range(Value::Category from, Value::Category to) {
        ValueRange result = {values_.categories[from],
                             values_.categories[to+1] - 1};
        return result;
    }

    /**
     * Fetch a value from the store
     *
     * @param id identifier of the value (within range 1..getValueCount())
     * @param[out] val will contain the value
     */
    void fetch(Value::id_t id, Value& val);

    /**
     * Search for the id of a value (if id == 0) and replace it if found.
     *
     * @param[in,out] val the value to look for and update
     */
    void lookup(Value& val);

    /**
     * @param id the identifier of a value in the store
     * @return the equivalence class of that value
     */
    ValueRange eqClass(Value::id_t id);

    /**
     * Get the equivalence class of a value. If val.id > 0, this is equivalent
     * to eqClass(val.id). Otherwise, it finds an equivalence class in
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
    ValueRange eqClass(const Value& val);

    /**
     * @param id the identifier of a value in the store
     * @return the category of the value
     */
    Value::Category category(Value::id_t id);

    unsigned statTripleCacheHits()   { return cache_.statHits();   }
    unsigned statTripleCacheMisses() { return cache_.statMisses(); }


    /**
     * Query a range of triples.
     *
     * @note concurrent queries result in undefined behaviour
     */
    class TripleRange {
    public:
        /**
         * Construct a new query.
         * @param store the store
         * @param from lower bound
         * @param to upper bound
         */
        TripleRange(Store* store, Triple from, Triple to);

        //! Non-copyable
        TripleRange(const TripleRange&) = delete;
        TripleRange& operator=(const TripleRange&) = delete;

        /**
         * Fetch the next result statement
         *
         * @param[out] t structure in which to write the result or
         *               nullptr to ignore
         * @return true if the next result has been found, false if there are
         *         no more results (further calls to next() are undefined)
         */
        bool next(Triple* t);

    private:
        Store* store_;
        Triple limit_; //!< the upper bound
        TripleOrder order_; //!< order of components in the key

        unsigned nextPage_; //!< next page to read or 0 if no more

        const Triple* it_; //!< current triple
        const Triple* end_; //!< last triple in current cache line
    };

private:
    PageReader db_;

    /**
     * Triple indexes. Each index has a different ordering.
     */
    struct {
        unsigned begin;       //!< first page of table
        BTree<Triple>* index; //!< index
    } triples_[ORDERS];

    /**
     * Values
     */
    struct {
        unsigned       count;     //!< number of values
        unsigned       begin;     //!< first page of table
        unsigned       mapping;   //!< first page of mapping
        ValueHashTree* index;     //!< index (hash->page mapping)
        unsigned       eqClasses; //!< first page of equivalence classes boundaries

        //! first id of each category
        Value::id_t categories[Value::CATEGORIES + 1];
    } values_;


    TripleCache cache_; //!< triples cache

    friend class TripleRange;
};

}

#endif // CASTOR_STORE_H
