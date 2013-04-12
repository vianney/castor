/* This file is part of Castor
 *
 * Author: Vianney le Clément de Saint-Marcq <vianney.leclement@uclouvain.be>
 * Copyright (C) 2010-2013, Université catholique de Louvain
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

#include <vector>
#include <string>
#include <exception>
#include <cassert>

#include "util.h"
#include "model.h"
#include "store/btree.h"
#include "store/triplecache.h"
#include "variable.h"

namespace castor {

/**
 * Utility class to lookup strings (i.e., point str to the right place
 * according to the id).
 */
class StringMapper {
public:

    /**
     * Construct a mapper.
     *
     * @param strings beginning of the strings table
     * @param map beginning of the string map, a sequence of 64-bits offsets in
     *            strings, ordered by id
     */
    StringMapper(Cursor strings = nullptr, Cursor map = nullptr)
        : strings_(strings), map_(map) {}

    /**
     * Lookup a string.
     *
     * @param id a valid id
     * @return the string pointed to by id
     */
    String lookupString(String::id_t id) const;

protected:
    Cursor strings_;
    Cursor map_;
};

/**
 * Store containing triples and values.
 * The triples encoding is modeled after RDF-3x [1].
 *
 * [1] http://www.mpi-inf.mpg.de/~neumann/rdf3x/
 */
class Store : public StringMapper {
public:
    static constexpr unsigned      VERSION = 11; //!< format version
    static const     unsigned char MAGIC[10];    //!< magic number

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
     * Number of strings in the store. The ids of the strings will always be
     * between 1 and the returned value included.
     *
     * @return the number of strings in the store
     */
    unsigned stringsCount() const { return strings_.count; }

    /**
     * Number of values in the store. The ids of the values will always be
     * between 1 and the returned value included.
     *
     * @return the number of values in the store
     */
    unsigned valuesCount() const { return values_.count; }

    /**
     * @return range of values of a category in the store
     */
    ValueRange range(Value::Category cat) const {
        ValueRange result = {values_.categories[cat],
                             values_.categories[cat+1] - 1};
        return result;
    }

    /**
     * @return range of values spanning given categories in the store
     */
    ValueRange range(Value::Category from, Value::Category to) const {
        ValueRange result = {values_.categories[from],
                             values_.categories[to+1] - 1};
        return result;
    }

    /**
     * Lookup a value from the store
     *
     * @param id identifier of the value (within range 1..valuesCount())
     * @return the value
     */
    Value lookupValue(Value::id_t id) const;

    /**
     * Search for the id of a string (if id == UNKNOWN_ID) and replace it if
     * found.
     *
     * @param[in,out] str the string to look for and update
     */
    void resolve(String& str) const;

    /**
     * Search for the id of a value (if id == UNKNOWN_ID) and replace it if found.
     *
     * @param[in,out] val the value to look for and update
     */
    void resolve(Value& val) const;

    /**
     * @param id the identifier of a value in the store
     * @return the equivalence class of that value
     */
    ValueRange eqClass(Value::id_t id) const;

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
    ValueRange eqClass(const Value& val) const;

    /**
     * @param id the identifier of a value in the store
     * @return the category of the value
     */
    Value::Category category(Value::id_t id) const;

    /**
     * Get a variable from the cache or create a new one if needed.
     * The variable shall be returned with release() to be reused or cleaned.
     *
     * @param solver the solver to (re)initialize the variable with
     * @return a variable whose domain ranges from 0 to valuesCount()
     */
    cp::RDFVar* variable(cp::Solver* solver);

    /**
     * Release a variable received by variable().
     *
     * @param x the variable
     */
    void release(cp::RDFVar* x);

    /**
     * @return the number of triples
     */
    unsigned triplesCount() const { return triplesCount_; }

    /**
     * Get the number of triples of specified pattern. Components with value
     * 0 are wildcards. Other components are matched exactly.
     *
     * @param pattern the pattern to look for
     * @return the number of triples matching the pattern
     */
    unsigned triplesCount(Triple pattern);

    /**
     * @param index
     * @return the triple at index index
     */
    Triple triple(unsigned index) const {
        assert(index >= 0 && index < triplesCount_);
        return Triple::read(db_.page(triplesTable_) + index * Triple::SIZE);
    }

    unsigned statTripleCacheHits()   const { return cache_.statHits();   }
    unsigned statTripleCacheMisses() const { return cache_.statMisses(); }

    /**
     * Query a range of triples.
     */
    class TripleRange {
    public:
        /**
         * Construct a new query.
         * @param store the store
         * @param from lower bound
         * @param to upper bound
         * @param order which index to use
         */
        TripleRange(Store* store, Triple from, Triple to,
                    TripleOrder order=TRIPLE_ORDER_AUTO);
        ~TripleRange();

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
        Store*        store_;
        Triple        limit_;     //!< the upper bound
        TripleOrder   order_;     //!< order of components in the key
        int           direction_; //!< +1=forward search, -1=backward search

        unsigned      nextPage_;  //!< next page to read or 0 if no more

        const Triple* it_;        //!< current triple
        const Triple* end_;       //!< last triple in current cache line

        const TripleCache::Line* line_; //!< current cache line
    };

private:
    PageReader db_;

    /**
     * Number of triples
     */
    unsigned triplesCount_;

    /**
     * Raw triples table
     */
    unsigned triplesTable_;

    /**
     * Triple indexes. Each index has a different ordering.
     */
    struct {
        unsigned begin;       //!< first page of table
        unsigned end;         //!< last page of table
        BTree<Triple>* index; //!< index
        BTree<AggregatedTriple>* aggregated; //!< index of aggregated triples
    } triples_[TRIPLE_ORDERS];

    /**
     * Index of fully aggregated triples
     */
    BTree<FullyAggregatedTriple>* fullyAggregated_[Triple::COMPONENTS];

    /**
     * Strings
     */
    struct {
        unsigned       count;     //!< number of strings
        unsigned       begin;     //!< first page of table
        unsigned       map;       //!< first page of map
        HashTree<8>*   index;     //!< index (hash->offset mapping)
    } strings_;

    /**
     * Values
     */
    struct {
        unsigned       count;     //!< number of values
        unsigned       begin;     //!< first page of table
        HashTree<4>*   index;     //!< index (hash->page mapping)
        unsigned       eqClasses; //!< first page of equivalence classes boundaries

        //! first id of each category
        Value::id_t categories[Value::CATEGORIES + 1];
    } values_;


    TripleCache cache_; //!< triples cache

    std::vector<cp::RDFVar*> varcache_; //!< variables cache

    friend class TripleRange;
};

}

#endif // CASTOR_STORE_H
