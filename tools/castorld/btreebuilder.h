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
#ifndef CASTOR_TOOLS_CASTORLD_BTREEBUILDER_H
#define CASTOR_TOOLS_CASTORLD_BTREEBUILDER_H

#include <vector>

#include "pagewriter.h"
#include "store/btree.h"

namespace castor {

/**
 * Helper to build a B+-tree.
 *
 * To construct the tree, proceed as follows:
 * 1. create a BTreeBuilder (this will initialize the leaf header)
 * 2. call beginLeaf()
 * 3. fill the page with ordered items
 * 4. call endLeaf() with the last inserted key
 * 5. repeat steps 2-4 until all items have been written
 * 6. call constructTree()
 *
 * @note You must not call PageWriter::flushPage() while building a tree.
 *
 * The key class K must implement these two members
 * - static const unsigned SIZE: the size in bytes of a written key
 * - void write(PageWriter& writer): write the key
 *
 * @see BTree
 */
template<class K>
class BTreeBuilder {
public:
    /**
     * Construct a new B+-tree.
     *
     * @pre writer should be at the beginning of a page
     */
    BTreeBuilder(PageWriter* writer);

    //! Non-copyable
    BTreeBuilder(const BTreeBuilder&) = delete;
    BTreeBuilder& operator=(const BTreeBuilder&) = delete;

    /**
     * Initialize a page to start a new leaf.
     */
    void beginLeaf();

    /**
     * End the current leaf.
     *
     * @param last the last key in the leaf.
     */
    void endLeaf(K last);

    /**
     * @return the page number of the last ended leaf
     */
    unsigned getLastLeaf() { return lastLeaf_; }

    /**
     * Construct the inner nodes of the tree.
     *
     * @return the page number of the root of the tree
     */
    unsigned constructTree();

private:
    PageWriter* writer_; //!< output writer
    std::vector<std::pair<K, unsigned> > boundaries_; //!< level boundaries

    unsigned leaves_; //!< number of leaves so far
    unsigned lastLeaf_; //!< page number of the last ended leaf

    static constexpr unsigned HEADER_SIZE = 4; //!< size of node header
};



// implementation

template<class K>
BTreeBuilder<K>::BTreeBuilder(PageWriter* writer) : writer_(writer) {
    assert(writer->offset() == 0);
    leaves_ = 0;
    lastLeaf_ = 0;
}

template<class K>
void BTreeBuilder<K>::beginLeaf() {
    if(leaves_ > 0) {
        // update header flags and flush previous leaf
        BTreeFlags flags;
        if(leaves_ == 1)
            flags |= BTreeFlags::FIRST_LEAF;
        writer_->writeInt(flags, 0);
        writer_->flush();
    }
    writer_->skip(HEADER_SIZE); // leave room for header
    leaves_++;
}

template<class K>
void BTreeBuilder<K>::endLeaf(K last) {
    boundaries_.push_back(std::pair<K,unsigned>(last, writer_->page()));
    lastLeaf_ = writer_->page();
}

template<class K>
unsigned BTreeBuilder<K>::constructTree() {
    assert(leaves_ > 0);
    assert(leaves_ == boundaries_.size());

    // flush last leaf
    BTreeFlags flags = BTreeFlags::LAST_LEAF;
    if(leaves_ == 1)
        flags |= BTreeFlags::FIRST_LEAF;
    writer_->writeInt(flags, 0);
    writer_->flush();

    // create inner nodes
    bool first = true;
    while(first || boundaries_.size() > 1) {
        first = false;
        writer_->skip(HEADER_SIZE);
        std::vector<std::pair<K, unsigned> > newBoundaries;
        unsigned count = 0;
        K last;
        for(std::pair<K, unsigned> b : boundaries_) {
            // check remaining space
            if(K::SIZE + 4 > writer_->remaining()) {
                // write header, flush page and start a new one
                newBoundaries.push_back(
                            std::pair<K,unsigned>(last, writer_->page()));
                writer_->writeInt(BTreeFlags::INNER_NODE | count, 0);
                writer_->flush();
                count = 0;
                writer_->skip(HEADER_SIZE);
            }

            // write entry
            b.first.write(*writer_);
            writer_->writeInt(b.second);
            count++;

            last = b.first;
        }
        // flush last page
        newBoundaries.push_back(std::pair<K,unsigned>(last, writer_->page()));
        writer_->writeInt(BTreeFlags::INNER_NODE | count, 0);
        writer_->flush();

        std::swap(boundaries_, newBoundaries);
    }

    return writer_->page() - 1;
}


}


#endif // CASTOR_TOOLS_CASTORLD_BTREEBUILDER_H
