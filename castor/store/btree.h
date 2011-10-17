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
#ifndef CASTOR_STORE_BTREE_H
#define CASTOR_STORE_BTREE_H

#include "readutils.h"

namespace castor {

/**
 * Base class for a disk-backed B+-tree. Page 0 must not be a node as it is used
 * to indicate an unknown key.
 *
 * The key class K must provide these members
 * - static const unsigned SIZE: the size in bytes of the key
 * - bool operator<(const K &o) const: comparator
 * - static K read(Cursor cur): read a key
 */
template <class K>
class BTree {
protected:
    PageReader *db; //!< the database
    unsigned rootPage; //!< the page containing the root of the tree

public:
    BTree(PageReader *db, unsigned rootPage) : db(db), rootPage(rootPage) {}

    /**
     * @param a key
     * @return the page of the first leaf that contains keys >= key
     *         or 0 if not found
     */
    unsigned lookupLeaf(K key);
};

/**
 * Key structure for hashed values
 */
struct ValueHashKey {
    uint32_t hash;

    static const unsigned SIZE = 4;

    ValueHashKey(uint32_t hash) : hash(hash) {}

    bool operator <(const ValueHashKey &o) const {
        return hash < o.hash;
    }

    static ValueHashKey read(Cursor cur) {
        return ValueHashKey(cur.readInt());
    }
};

/**
 * B+-tree containing hashed values.
 */
class ValueHashTree : private BTree<ValueHashKey> {
public:
    ValueHashTree(PageReader *db, unsigned rootPage) : BTree(db, rootPage) {}

    /**
     * Lookup a hash key
     *
     * @return pointer to the first (hash, page) entry with this hash
     *         (invalid pointer if not found)
     */
    Cursor lookup(uint32_t hash);
};

/**
 * Key structure for triples index. A component with id 0 is a wildcard.
 * Wildcard components must be last.
 */
struct TripleKey {
    Value::id_t a, b, c;

    static const unsigned SIZE = 12;

    static TripleKey read(Cursor cur) {
        TripleKey k = {cur.readInt(),
                       cur.readInt(),
                       cur.readInt()};
        return k;
    }

    bool operator <(const TripleKey &o) const {
        return a < o.a ||
               (a == o.a && (b < o.b ||
                             (b == o.b && c < o.c)));
    }

    /**
     * @return whether o matches this key, ignoring wildcards in this key
     */
    bool matches(const TripleKey &o) const {
        return a == 0 ||
               (a == o.a && (b == 0 ||
                             (b == o.b && (c == 0 || c == o.c))));
    }
};


// Template implementation

template<class K>
unsigned BTree<K>::lookupLeaf(K key) {
    unsigned page = rootPage;
    while(true) {
        Cursor pageCur = db->getPage(page);
        if(pageCur.readInt() == 0xffffffff) {
            // inner node: perform binary search
            pageCur.skipInt(); // skip "next page" pointer
            unsigned left = 0, right = pageCur.readInt();
            pageCur.skipInt(); // skip padding
            while(left != right) {
                unsigned middle = (left + right) / 2;
                Cursor middleCur = pageCur + middle * (K::SIZE + 4);
                K middleKey = K::read(middleCur);
                middleCur += K::SIZE;
                if(middleKey < key) {
                    left = middle + 1;
                } else if(middle == 0 ||
                          K::read(pageCur + (middle-1) * (K::SIZE + 4)) < key) {
                    page = middleCur.readInt();
                    break;
                } else {
                    right = middle;
                }
            }
            if(left == right) {
                // unsuccessful search
                return 0;
            }
        } else {
            // leaf node: we're done
            return page;
        }
    }
}

}

#endif // CASTOR_STORE_BTREE_H
