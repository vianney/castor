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
#ifndef CASTOR_STORE_BTREE_H
#define CASTOR_STORE_BTREE_H

#include "util.h"
#include "model.h"

namespace castor {

/**
 * Base class for a disk-backed B+-tree. Page 0 must not be a node as it is used
 * to indicate an unknown key.
 *
 * A B+-tree is encoded as follows:
 *
 * Leaves:
 * +-------+-------------------------------------------------+
 * | flags | data                                            |
 * +-------+-------------------------------------------------+
 * 0   |   4                                                end
 *     |
 *     +-> bit 0: set if first leaf, unset otherwise
 *         bit 1: set if last leaf, unset otherwise
 *         bit 31: unset to indicate a leaf
 *
 * All leaf pages are written sequentially.
 *
 * Inner nodes:
 * +-------------+-------------------------------------------+
 * | flags/count | children                                  |
 * +-------------+-------------------------------------------+
 * 0     |       4    |                                     end
 *       |            |
 *       |            +-> a child is a key followed by the page number of lower
 *       |                level containing up to that key
 *       |
 *       +-> bits 0-30: count (number of direct children)
 *           bit 31: set to indicate an inner node
 *
 * The key class K must provide these members
 * - static const unsigned SIZE: the size in bytes of the key
 * - bool operator<(const K& o) const: comparator
 * - static K read(Cursor cur): read a key
 */
template <class K>
class BTree {
public:
    BTree(PageReader* db, unsigned rootPage) : db_(db), rootPage_(rootPage) {}

    /**
     * @param key a key
     * @return the page of the first leaf that contains keys >= key
     *         or 0 if not found
     */
    unsigned lookupLeaf(K key) const;

protected:
    PageReader* db_; //!< the database
    unsigned rootPage_; //!< the page containing the root of the tree
};

/**
 * Wrapper class around the flags field of the nodes.
 *
 * @see BTree
 */
class BTreeFlags {
public:
    BTreeFlags() : flags_(0) {}
    constexpr BTreeFlags(unsigned flags) : flags_(flags) {}
    constexpr BTreeFlags(const BTreeFlags&) = default;

    static constexpr unsigned INNER_NODE = 1 << 31;
    static constexpr unsigned FIRST_LEAF = 1 << 0;
    static constexpr unsigned LAST_LEAF  = 1 << 1;

    BTreeFlags& operator=(const BTreeFlags&) = default;
    operator unsigned() const { return flags_; }

    BTreeFlags operator|(const BTreeFlags& o) const { return flags_ | o.flags_; }
    BTreeFlags& operator|=(const BTreeFlags& o) { flags_ |= o.flags_; return *this; }

    /**
     * @return whether the node is an inner node
     */
    bool inner    () const { return flags_ & INNER_NODE; }
    /**
     * @pre !inner()
     * @return whether the node is the first leaf
     */
    bool firstLeaf() const { return flags_ & FIRST_LEAF; }
    /**
     * @pre !inner()
     * @return whether the node is the last leaf
     */
    bool lastLeaf () const { return flags_ & LAST_LEAF; }
    /**
     * @pre inner()
     * @return the number of direct children of the inner node
     */
    unsigned count() const { return flags_ & (INNER_NODE - 1); }

private:
    unsigned flags_;
};

/**
 * Key structure for hashed values
 */
struct HashKey {
    Hash::hash_t hash;

    static constexpr unsigned SIZE = 4;

    HashKey() = default;
    HashKey(Hash::hash_t hash) : hash(hash) {}

    bool operator<(const HashKey& o) const {
        return hash < o.hash;
    }

    static HashKey read(Cursor cur) {
        return HashKey(cur.readInt());
    }
};

/**
 * B+-tree containing hashed values.
 *
 * Leaves start with a count (4 bytes), followed by the (hash, value) pairs.
 * Hashes are 4 bytes long. The length of the values is specified by VALUE_SIZE.
 */
template<std::size_t VALUE_SIZE>
class HashTree : private BTree<HashKey> {
public:
    HashTree(PageReader* db, unsigned rootPage) : BTree(db, rootPage) {}

    /**
     * Lookup a hash key
     *
     * @return pointer to the first (hash, value) entry with this hash
     *         (invalid pointer if not found)
     */
    Cursor lookup(Hash::hash_t hash) const;
};


// Template implementation

template<class K>
unsigned BTree<K>::lookupLeaf(K key) const {
    unsigned page = rootPage_;
    while(true) {
        Cursor pageCur = db_->page(page);
        BTreeFlags flags = pageCur.readInt();
        if(flags.inner()) {
            // inner node: perform binary search
            unsigned left = 0, right = flags.count();
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

template<std::size_t VALUE_SIZE>
Cursor HashTree<VALUE_SIZE>::lookup(Hash::hash_t hash) const {
    unsigned page = lookupLeaf(HashKey(hash));
    if(page == 0)
        return Cursor(nullptr);
    Cursor pageCur = db_->page(page);
    pageCur.skipInt(); // skip flags
    // binary search
    constexpr std::size_t SIZE = 4 + VALUE_SIZE;
    unsigned left = 0, right = pageCur.readInt();
    while(left != right) {
        unsigned middle = (left + right) / 2;
        Cursor middleCur = pageCur + middle * SIZE;
        Hash::hash_t middleHash = middleCur.readInt();
        if(middleHash < hash) {
            left = middle + 1;
        } else if(middleHash > hash) {
            right = middle;
        } else {
            // match: find first in collision list
            while(middle > 0 && (pageCur + (middle-1) * SIZE).readInt() == hash)
                middle--;
            return pageCur + middle * SIZE;
        }
    }
    // not found
    return Cursor(nullptr);
}

}

#endif // CASTOR_STORE_BTREE_H
