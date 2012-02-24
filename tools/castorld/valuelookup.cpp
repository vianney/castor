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
#include "valuelookup.h"

#include <cstring>

#include "util.h"

namespace castor {

ValueLookup::ValueLookup(TempFile* file) : file_(file) {
    values_ = new Value[SIZE];
    ids_ = new uint64_t[SIZE];
    memset(ids_, 0, SIZE * sizeof(uint64_t));
    next_ = 1;
}

ValueLookup::~ValueLookup() {
    delete [] values_;
    delete [] ids_;
}

uint64_t ValueLookup::lookup(const Value& val) {
    // already in hash table?
    unsigned slot = val.hash() % SIZE;
    if(ids_[slot] && values_[slot] == val)
        return ids_[slot];

    // no, construct a new id
    values_[slot] = val;
    uint64_t id = ids_[slot] = next_++;

    // write mapping to file
    file_->writeValue(val);
    file_->writeBigInt(id);

    return id;
}

}
