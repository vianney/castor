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
#include "btree.h"

#include <cassert>

namespace castor {

Cursor ValueHashTree::lookup(uint32_t hash) {
    unsigned page = lookupLeaf(ValueHashKey(hash));
    if(page == 0)
        return Cursor(0);
    Cursor pageCur = db->getPage(page);
    pageCur.skipInt(); // skip "previous page" pointers
    pageCur.skipInt(); // skip "next page" pointers
    // binary search
    unsigned left = 0, right = pageCur.readInt();
    while(left != right) {
        unsigned middle = (left + right) / 2;
        Cursor middleCur = pageCur + middle * 8;
        uint32_t middleHash = middleCur.readInt();
        if(middleHash < hash) {
            left = middle + 1;
        } else if(middleHash > hash) {
            right = middle;
        } else {
            // match: find first in collision list
            while(middle > 0 && (pageCur + (middle-1) * 8).readInt() == hash)
                middle--;
            return pageCur + middle * 8;
        }
    }
    // not found
    return Cursor(0);
}

}
