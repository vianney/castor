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
#ifndef CASTOR_UTIL_H
#define CASTOR_UTIL_H

#include <string>
#include <algorithm>
#include <cstdint>
#include <cstring>

namespace castor {

struct DereferenceLess {
    template <typename T>
    bool operator()(T a, T b) const { return *a < *b; }
};

/**
 * Compare two (non-null terminated) strings with specified length
 */
static inline int cmpstr(const char *a, size_t alen, const char *b, size_t blen) {
    int cmp = memcmp(a, b, std::min(alen, blen));
    if(cmp) return cmp;
    else if(alen < blen) return -1;
    else if(alen > blen) return 1;
    else return 0;
}

class Hash {
public:
    /**
     * Hash a variable-length key into a 32-bit value
     *
     * @param k the key (the unaligned variable-length array of bytes)
     * @param length the length of the key, counting by bytes
     * @param initval can be any 4-byte value
     * @return a 32-bit value
     */
    static uint32_t hash(const void *key, size_t length, uint32_t initval=0);

    /**
     * Hash a string into a 32-bit value
     *
     * @see hash(const void*, size_t, uint32_t)
     */
    static uint32_t hash(const std::string &str, uint32_t initval=0) {
        return hash(str.c_str(), str.size(), initval);
    }
};

}

#endif // CASTOR_UTIL_H
