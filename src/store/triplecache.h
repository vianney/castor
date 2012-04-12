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
#include "btree.h"

#include <algorithm>

namespace castor {

enum class TripleOrder {
    SPO, SOP, PSO, POS, OSP, OPS
};
static constexpr int TRIPLE_ORDERS = 6;
//! Virtual ordering indicating automatic selection of the order
static constexpr TripleOrder TRIPLE_ORDER_AUTO = static_cast<TripleOrder>(-1);


/**
 * A real triple from the store
 */
struct Triple : public BasicTriple<Value::id_t> {
    Triple() {}
    Triple(const BasicTriple<Value::id_t>& o) : BasicTriple(o) {}

    Triple(const Triple&) = default;
    Triple& operator=(const Triple&) = default;

    /**
     * Convert this SPO triple to an ordered triple
     *
     * @param order ordering of the returned triple
     * @return the reordered triple
     */
    Triple toOrdered(TripleOrder order) const {
        switch(order) {
        case TripleOrder::SPO: return *this;
        case TripleOrder::SOP: return reorder<0,2,1>();
        case TripleOrder::PSO: return reorder<1,0,2>();
        case TripleOrder::POS: return reorder<1,2,0>();
        case TripleOrder::OSP: return reorder<2,0,1>();
        case TripleOrder::OPS: return reorder<2,1,0>();
        }
        // should not happen
        assert(false);
        return *this;
    }

    /**
     * Convert this triple to a real SPO triple
     *
     * @param order the ordering of this triple
     * @return the SPO triple
     */
    Triple toSPO(TripleOrder order) const {
        switch(order) {
        case TripleOrder::SPO: return *this;
        case TripleOrder::SOP: return reorder<0,2,1>();
        case TripleOrder::PSO: return reorder<1,0,2>();
        case TripleOrder::POS: return reorder<2,0,1>();
        case TripleOrder::OSP: return reorder<1,2,0>();
        case TripleOrder::OPS: return reorder<2,1,0>();
        }
        // should not happen
        assert(false);
        return *this;
    }

    static constexpr unsigned SIZE = 12;
    static Triple read(Cursor cur) {
        Triple t;
        for(int i = 0; i < COMPONENTS; i++)
            t[i] = cur.readInt();
        return t;
    }

    /**
     * Read a leaf page
     *
     * @param cur start of the leaf page contents (without header)
     * @param end end of the page
     * @param[out] triples array that will contain the triples
     * @return the number of triples read
     */
    static unsigned readPage(Cursor cur, Cursor end, Triple* triples);
};

/**
 * An aggregated triple. The last component is the count of the triples with
 * the first two components.
 */
struct AggregatedTriple : public Triple {
    AggregatedTriple() {}
    AggregatedTriple(const BasicTriple<Value::id_t>& o) : Triple(o) {}

    AggregatedTriple(const AggregatedTriple&) = default;
    AggregatedTriple& operator=(const AggregatedTriple&) = default;

    unsigned count() const { return c[COMPONENTS - 1]; }

    /**
     * The comparator ignores the last component (i.e., the count).
     */
    bool operator<(const AggregatedTriple& o) const {
        return c[0] < o.c[0] ||
                (c[0] == o.c[0] && c[1] < o.c[1]);
    }

    static constexpr unsigned SIZE = 8;
    static AggregatedTriple read(Cursor cur) {
        AggregatedTriple t;
        for(int i = 0; i < COMPONENTS - 1; i++)
            t[i] = cur.readInt();
        return t;
    }

    static unsigned readPage(Cursor cur, Cursor end, Triple* triples);
};

/**
 * A fully aggregated triple. The second component is the count of the triples
 * with the first component. The third component is ignored.
 */
struct FullyAggregatedTriple : public Triple {
    FullyAggregatedTriple() {}
    FullyAggregatedTriple(const BasicTriple<Value::id_t>& o) : Triple(o) {}

    FullyAggregatedTriple(const FullyAggregatedTriple&) = default;
    FullyAggregatedTriple& operator=(const FullyAggregatedTriple&) = default;

    unsigned count() const { return c[1]; }

    /**
     * The comparator ignores the second component (i.e., the count).
     */
    bool operator<(const FullyAggregatedTriple& o) const {
        return c[0] < o.c[0];
    }

    static constexpr int COMPONENTS = 2;
    static constexpr unsigned SIZE = 4;
    static FullyAggregatedTriple read(Cursor cur) {
        FullyAggregatedTriple t;
        for(int i = 0; i < COMPONENTS - 1; i++)
            t[i] = cur.readInt();
        return t;
    }

    static unsigned readPage(Cursor cur, Cursor end, Triple* triples);
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

        /**
         * @return iterator to the first triple
         */
        template<class T=Triple>
        const T* begin() const {
            return reinterpret_cast<const T*>(triples);
        }

        /**
         * @return iterator to the element following the last triple
         */
        template<class T=Triple>
        const T* end() const {
            return reinterpret_cast<const T*>(triples + count);
        }

        /**
         * Perform a binary search.
         *
         * @param key triple to look for
         * @return pointer to the first triple that is not less than key, or
         *         end() if not found
         */
        template<class T=Triple>
        const T* findLower(const T& key) const {
            return std::lower_bound(begin<T>(), end<T>(), key);
        }

        /**
         * Perform a binary search.
         *
         * @param key triple to look for
         * @return pointer to the first triple that is greater than key, or
         *         end() if not found
         */
        template<class T=Triple>
        const T* findUpper(const T& key) const {
            return std::upper_bound(begin<T>(), end<T>(), key);
        }

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
     * @param T type of triple to read
     * @param page the page number (should contain triples),
     *             page <= maxPage
     * @return the cache line with the uncompressed page
     */
    template<class T=Triple>
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



// Template implementation
template<class T>
const TripleCache::Line* TripleCache::fetch(unsigned page) {
    assert(page > 0);

    // lookup page in cache
    Line* line = map_[page];
    if(line != nullptr) {
        ++statHits_;
        // move cache line to head of list
        if(head_ != line) {
            line->prev_->next_ = line->next_;
            if(tail_ == line)
                tail_ = line->prev_;
            else
                line->next_->prev_ = line->prev_;
            head_->prev_ = line;
            line->next_ = head_;
            line->prev_ = nullptr;
            head_ = line;
        }
        return line;
    }

    ++statMisses_;
    // find free cache line
    if(size_ < CAPACITY) {
        // intialize new line
        line = &lines_[size_++];
        line->triples = new Triple[Line::MAX_COUNT];
        if(head_ == nullptr) {
            head_ = tail_ = line;
            line->prev_ = nullptr;
            line->next_ = nullptr;
        } else {
            head_->prev_ = line;
            line->next_ = head_;
            line->prev_ = nullptr;
            head_ = line;
        }
    } else {
        // evict least recently used line
        line = tail_;
        tail_ = line->prev_;
        tail_->next_ = nullptr;
        head_->prev_ = line;
        line->next_ = head_;
        line->prev_ = nullptr;
        head_ = line;
        map_[line->page] = nullptr;
    }

    map_[page] = line;

    // read page and interpret header
    Cursor cur = db_->page(page);
    Cursor end = cur + PageReader::PAGE_SIZE;
    line->page = page;
    BTreeFlags flags = cur.readInt();
    assert(!flags.inner());
    line->first = flags.firstLeaf();
    line->last = flags.lastLeaf();

    // unpack triples
    line->count = T::readPage(cur, end, line->triples);

    return line;
}

}

#endif // CASTOR_STORE_TRIPLECACHE_H
