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
#ifndef CASTOR_TOOLS_CASTORLD_BTREEBUILDER_H
#define CASTOR_TOOLS_CASTORLD_BTREEBUILDER_H

#include <vector>

#include "pagewriter.h"

namespace castor {

/**
 * Helper to build a B+-tree. A B+-tree is encoded as follows:
 *
 * Leaves:
 * +-----------+-----------+---------------------------------+
 * | prev page | next page | data                            |
 * +-----------+-----------+---------------------------------+
 * 0           4           8                                end
 * (prev/next page is 0 for the first/last leaf)
 *
 * Inner nodes:
 * +--------+-------+----------------------------------------+
 * | marker | count | nodes                                  |
 * +--------+-------+----------------------------------------+
 * 0        4       8                                       end
 * (a node is a key followed by the page number of lower level containing up
 * to that key)
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
    unsigned getLastLeaf() { return previousLeaf_; }

    /**
     * Construct the inner nodes of the tree.
     *
     * @return the page number of the root of the tree
     */
    unsigned constructTree();

private:
    PageWriter* writer_; //!< output writer
    std::vector<std::pair<K, unsigned> > boundaries_; //!< level boundaries

    bool pendingLeaf_; //!< there is a leaf pending to write
    unsigned previousLeaf_; //!< page number of the previous leaf

    static constexpr unsigned LEAF_HEADER_SIZE = 8; //!< size of leaf header
    static constexpr unsigned NODE_HEADER_SIZE = 8; //!< size of inner node header
};



// implementation

template<class K>
BTreeBuilder<K>::BTreeBuilder(PageWriter* writer) : writer_(writer) {
    assert(writer->offset() == 0);
    pendingLeaf_ = false;
    previousLeaf_ = 0;
}

template<class K>
void BTreeBuilder<K>::beginLeaf() {
    if(pendingLeaf_) {
        // update header and flush previous leaf
        previousLeaf_ = writer_->page();
        writer_->writeInt(writer_->page() + 1, 4); // next page pointer
        writer_->flush();
        pendingLeaf_ = false;
    }
    writer_->writeInt(previousLeaf_);
    writer_->skip(4); // leave room for next page pointer
}

template<class K>
void BTreeBuilder<K>::endLeaf(K last) {
    boundaries_.push_back(std::pair<K,unsigned>(last, writer_->page()));
    pendingLeaf_ = true;
}

template<class K>
unsigned BTreeBuilder<K>::constructTree() {
    assert(pendingLeaf_);
    assert(!boundaries_.empty());

    // flush last leaf
    writer_->writeInt(0, 4);
    writer_->flush();

    // create inner nodes
    bool first = true;
    while(first || boundaries_.size() > 1) {
        first = false;
        writer_->skip(NODE_HEADER_SIZE);
        std::vector<std::pair<K, unsigned> > newBoundaries;
        unsigned count = 0;
        K last;
        for(std::pair<K, unsigned> b : boundaries_) {
            // check remaining space
            if(K::SIZE + 4 > writer_->remaining()) {
                // write header, flush page and start a new one
                newBoundaries.push_back(
                            std::pair<K,unsigned>(last, writer_->page()));
                writer_->writeInt(0xffffffff, 0);
                writer_->writeInt(count, 4);
                writer_->flush();
                count = 0;
                writer_->skip(NODE_HEADER_SIZE);
            }

            // write entry
            b.first.write(*writer_);
            writer_->writeInt(b.second);
            count++;

            last = b.first;
        }
        // flush last page
        newBoundaries.push_back(std::pair<K,unsigned>(last, writer_->page()));
        writer_->writeInt(0xffffffff, 0);
        writer_->writeInt(count, 4);
        writer_->flush();

        std::swap(boundaries_, newBoundaries);
    }

    return writer_->page() - 1;
}


}


#endif // CASTOR_TOOLS_CASTORLD_BTREEBUILDER_H
