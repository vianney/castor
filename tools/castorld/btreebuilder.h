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

#ifndef CASTOR_TOOLS_CASTORLD_BTREEBUILDER_H
#define CASTOR_TOOLS_CASTORLD_BTREEBUILDER_H

#include <vector>

#include "pagewriter.h"

namespace castor {

/**
 * Helper to build a B+-tree. A B+-tree is encoded as follows:
 *
 * Leaves:
 * +-----------+---------------------------------------------+
 * | next page | data                                        |
 * +-----------+---------------------------------------------+
 * 0           4                                            end
 *
 * Inner nodes:
 * +--------+-----------+-------+---------+------------------+
 * | marker | next page | count | padding | nodes            |
 * +--------+-----------+-------+---------+------------------+
 * 0        4           8      12        16                 end
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
 * - void write(PageWriter &writer): write the key
 */
template<class K>
class BTreeBuilder {

    PageWriter *writer; //!< output writer
    std::vector<std::pair<K, unsigned> > boundaries; //!< level boundaries

    bool pendingLeaf; //!< there is a laef pending to write

    static const unsigned LEAF_HEADER_SIZE = 4; //!< size of leaf header
    static const unsigned NODE_HEADER_SIZE = 16; //!< size of inner node header

public:
    /**
     * Construct a new B+-tree.
     *
     * @pre writer should be at the beginning of a page
     */
    BTreeBuilder(PageWriter *writer);

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
     * Construct the inner nodes of the tree.
     *
     * @return the page number of the root of the tree
     */
    unsigned constructTree();
};



// implementation

template<class K>
BTreeBuilder<K>::BTreeBuilder(PageWriter *writer) : writer(writer) {
    assert(writer->getOffset() == 0);
    pendingLeaf = false;
}

template<class K>
void BTreeBuilder<K>::beginLeaf() {
    if(pendingLeaf) {
        // update header and flush previous leaf
        writer->writeInt(writer->getPage() + 1, 0); // next page pointer
        writer->flushPage();
        pendingLeaf = false;
    }
    writer->skip(LEAF_HEADER_SIZE);
}

template<class K>
void BTreeBuilder<K>::endLeaf(K last) {
    boundaries.push_back(std::pair<K,unsigned>(last, writer->getPage()));
    pendingLeaf = true;
}

template<class K>
unsigned BTreeBuilder<K>::constructTree() {
    assert(pendingLeaf);
    assert(!boundaries.empty());

    // flush last leaf
    writer->writeInt(0, 0);
    writer->flushPage();

    // create inner nodes
    bool first = true;
    while(first || boundaries.size() > 1) {
        first = false;
        writer->skip(NODE_HEADER_SIZE);
        std::vector<std::pair<K, unsigned> > newBoundaries;
        unsigned count = 0;
        for(typename std::vector<std::pair<K, unsigned> >::const_iterator it=boundaries.begin(),
            end = boundaries.end(); it != end; ++it) {
            // check remaining space
            if(K::SIZE + 4 > writer->getRemaining()) {
                // write header, flush page and start a new one
                newBoundaries.push_back(std::pair<K,unsigned>((*(it-1)).first,
                                                              writer->getPage()));
                writer->writeInt(0xffffffff, 0);
                writer->writeInt(writer->getPage()+1, 4);
                writer->writeInt(count, 8);
                writer->writeInt(0, 12);
                writer->flushPage();
                count = 0;
                writer->skip(NODE_HEADER_SIZE);
            }

            // write entry
            (*it).first.write(*writer);
            writer->writeInt((*it).second);
            count++;
        }
        // flush last page
        newBoundaries.push_back(std::pair<K,unsigned>(boundaries.back().first,
                                                      writer->getPage()));
        writer->writeInt(0xffffffff, 0);
        writer->writeInt(0, 4);
        writer->writeInt(count, 8);
        writer->writeInt(0, 12);
        writer->flushPage();

        std::swap(boundaries, newBoundaries);
    }

    return writer->getPage() - 1;
}


}


#endif // CASTOR_TOOLS_CASTORLD_BTREEBUILDER_H
