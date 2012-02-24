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
#ifndef CASTOR_STORE_TRIPLECACHE_H
#define CASTOR_STORE_TRIPLECACHE_H

#include "../model.h"
#include "readutils.h"

namespace castor {

struct Triple : public BasicTriple<Value::id_t> {
    Triple() {}
    Triple(const BasicTriple<Value::id_t>& o) : BasicTriple(o) {}

    Triple(const Triple&) = default;
    Triple& operator=(const Triple&) = default;

    // For use as keys in a B+-tree
    static constexpr unsigned SIZE = 12;

    static Triple read(Cursor cur) {
        Triple t;
        for(int i = 0; i < COMPONENTS; i++)
            t[i] = cur.readInt();
        return t;
    }
};

/**
 * Cache of uncompressed triples leaf pages.
 */
class TripleCache {
public:
    /**
     * A cache line
     */
    struct Line {
        Triple*  triples;  //!< array of triples
        unsigned count;    //!< number of triples in the line

        unsigned page;     //!< page number of this line
        bool     first;    //!< whether it is the first triples page
        bool     last;     //!< whether it is the last triples page

    private:
        Line*    prev_;    //!< previous line in the LRU list
        Line*    next_;    //!< next line in the LRU list

        //! Maximum number of triples in a page
        static constexpr unsigned MAX_COUNT = PageReader::PAGE_SIZE;

        friend class TripleCache;
    };

    TripleCache();
    ~TripleCache();

    //! Non-copyable
    TripleCache(const TripleCache&) = delete;
    TripleCache& operator=(const TripleCache&) = delete;

    /**
     * Initialize the cache. This method is not a constructor, so we can call
     * it later in the store constructor.
     *
     * @param db the database
     * @param maxPage the highest page number we will encounter
     */
    void initialize(PageReader* db_, unsigned maxPage);

    /**
     * Read and decompress a leaf page in a triples index.
     *
     * @pre initialize() has been called
     * @param page the page number (should contain triples),
     *             page <= maxPage
     * @return the cache line with the uncompressed page
     */
    const Line* fetch(unsigned page);

    /**
     * Read the header (first or last page) and the first key of a leaf
     * page in a triples index.
     *
     * @param[in] page the page number (should contain triples)
     *                 page <= maxPage
     * @param[out] first whether it is the first page
     * @param[out] last whether it is the last page
     * @param[out] firstKey first key of the page
     */
    void peek(unsigned page, bool& first, bool& last, Triple& firstKey);

    unsigned statHits()   const { return statHits_; }
    unsigned statMisses() const { return statMisses_; }

private:
    PageReader* db_;

    static const unsigned CAPACITY = 100; //!< maximum size of the cache

    Line     lines_[CAPACITY]; //!< cache lines
    unsigned size_;            //!< number of pages in cache
    Line*    head_;            //!< head of the LRU list (= most recently used)
    Line*    tail_;            //!< tail of the LRU list (= least recently used)

    Line**   map_;             //!< map from page number to cach line

    unsigned statHits_;        //!< number of cache hits
    unsigned statMisses_;      //!< number of cache misses
};

}

#endif // CASTOR_STORE_TRIPLECACHE_H
