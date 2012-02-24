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
#ifndef CASTOR_UTIL_H
#define CASTOR_UTIL_H

#include <exception>
#include <string>
#include <sstream>
#include <algorithm>
#include <cstdint>
#include <cstring>

namespace castor {

/**
 * Exception thrown by the Castor library.
 *
 * An exception can be constructed using the stream interface. E.g.,
 * throw CastorException() << "Error " << 42;
 */
class CastorException : public std::exception {
public:
    CastorException() {}
    CastorException(const CastorException& o) : msg_(o.msg_.str()) {}
    ~CastorException() throw() {}

    CastorException& operator=(const CastorException& o) {
        msg_.str(o.msg_.str());
        return *this;
    }

    const char* what() const throw() { return msg_.str().c_str(); }

    template<typename T>
    CastorException& operator<<(const T& t) {
        msg_ << t;
        return *this;
    }

private:
    std::ostringstream msg_;
};

/**
 * Comparator that compares the dereferenced values.
 */
struct DereferenceLess {
    template <typename T>
    bool operator()(T a, T b) const { return *a < *b; }
};

/**
 * Compare two (non-null terminated) strings with specified length
 */
static inline int cmpstr(const char* a, size_t alen,
                         const char* b, size_t blen) {
    int cmp = memcmp(a, b, std::min(alen, blen));
    if(cmp) return cmp;
    else if(alen < blen) return -1;
    else if(alen > blen) return 1;
    else return 0;
}

/**
 * Check whether two (non-null terminated) strings with specified length are
 * equal
 */
static inline bool eqstr(const char* a, size_t alen,
                         const char* b, size_t blen) {
    return alen == blen && cmpstr(a, alen, b, blen) == 0;
}

class Hash {
public:
    typedef uint32_t hash_t;

    /**
     * Hash a variable-length key into a 32-bit value
     *
     * @param key the key (the unaligned variable-length array of bytes)
     * @param length the length of the key, counting by bytes
     * @param initval can be any 4-byte value
     * @return a 32-bit value
     */
    static hash_t hash(const void* key, size_t length, hash_t initval=0);
};

/**
 * Find last bit set in a word. Similar to posix ffs.
 *
 * @param i the word (!= 0)
 */
static inline int fls(int i) {
    int result;
    // TODO make this portable
    asm("bsrl %1,%0" : "=r"(result) : "r"(i));
    return result;
}

}

#endif // CASTOR_UTIL_H
