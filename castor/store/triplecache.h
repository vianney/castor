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
#ifndef CASTOR_STORE_TRIPLECACHE_H
#define CASTOR_STORE_TRIPLECACHE_H

#include "../model.h"
#include "readutils.h"

namespace castor {

struct Triple : public BasicTriple<Value::id_t> {
    Triple() {}
    Triple(const BasicTriple<Value::id_t> &o) : BasicTriple(o[0], o[1], o[2]) {}

    // For use as keys in a B+-tree
    static const unsigned SIZE = 12;

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
        Triple   *triples;  //!< array of triples
        unsigned  count;    //!< number of triples in the line

        unsigned  page;     //!< page number of this line
        unsigned  prevPage; //!< previous page (or 0 if first)
        unsigned  nextPage; //!< next page (or 0 if last)

    private:
        Line     *prev;     //!< previous line in the LRU list
        Line     *next;     //!< next line in the LRU list

        //! Maximum number of triples in a page
        static const unsigned MAX_COUNT = PageReader::PAGE_SIZE;

        friend class TripleCache;
    };

    TripleCache();
    ~TripleCache();

    /**
     * Initialize the cache. This method is not a constructor, so we can call
     * it later in the store constructor.
     *
     * @param db the database
     * @param maxPage the highest page number we will encounter
     */
    void initialize(PageReader *db, unsigned maxPage);

    /**
     * Read and decompress a leaf page in a triples index.
     *
     * @pre initialize() has been called
     * @param page the page number (should contain triples),
     *             minPage <= page <= maxPage
     * @return the cache line with the uncompressed page
     */
    const Line* fetch(unsigned page);

    unsigned getStatHits()   { return statHits; }
    unsigned getStatMisses() { return statMisses; }

private:
    PageReader *db;

    static const unsigned CAPACITY = 100; //!< maximum size of the cache

    Line lines[CAPACITY]; //!< cache lines
    unsigned size; //!< number of pages in cache
    Line *head; //!< head of the LRU list (= most recently used)
    Line *tail; //!< tail of the LRU list (= least recently used)

    Line **map; //!< map from page number to cach line

    unsigned statHits;   //!< number of cache hits
    unsigned statMisses; //!< number of cache misses
};

}

#endif // CASTOR_STORE_TRIPLECACHE_H
