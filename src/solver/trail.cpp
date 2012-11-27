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
#include "trail.h"

namespace castor {
namespace cp {

Trail::Trail(std::size_t capacity) {
    trail_ = reinterpret_cast<char*>(malloc(capacity));
    end_ = trail_ + capacity;
    ptr_ = trail_;
    timestamp_ = 0;
}

Trail::~Trail() {
    free(trail_);
}

void Trail::enlargeSpace(std::size_t capacity) {
    std::size_t size = end_ - trail_;
    while(size < capacity)
        size *= 2;
    std::ptrdiff_t ptrpos = (ptr_ - trail_);
    trail_ = reinterpret_cast<char*>(realloc(trail_, size));
    end_ = trail_ + size;
    ptr_ = trail_ + ptrpos;
}

Trail::checkpoint_t Trail::checkpoint() {
    timestamp_++;
    return ptr_ - trail_;
}

void Trail::restore(checkpoint_t chkp) {
    while(ptr_ - trail_ > chkp) {
        Trailable* x = pop<Trailable*>();
        x->restore(*this);
    }
    assert(ptr_ - trail_ == chkp);
    timestamp_++;
}

void Trailable::modifying() {
    if(timestamp_ != trail_->timestamp()) {
        save(*trail_);
        trail_->push(this);
        timestamp_ = trail_->timestamp();
    }
}

}
}
