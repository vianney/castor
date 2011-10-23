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

#ifndef CASTOR_STORE_MMAPFILE_H
#define CASTOR_STORE_MMAPFILE_H

#include "../model.h"

namespace castor {

/**
 * Pointer in a mapped file
 */
class Cursor {
    const unsigned char *ptr;

public:

    Cursor() {}
    Cursor(const unsigned char *ptr) : ptr(ptr) {}
    Cursor(const Cursor &o) : ptr(o.ptr) {}

    bool operator==(const Cursor &o) const { return ptr == o.ptr; }
    bool operator!=(const Cursor &o) const { return ptr != o.ptr; }
    bool operator<(const Cursor &o) const { return ptr < o.ptr; }
    bool operator<=(const Cursor &o) const { return ptr <= o.ptr; }
    bool operator>(const Cursor &o) const { return ptr > o.ptr; }
    bool operator>=(const Cursor &o) const { return ptr >= o.ptr; }

    Cursor& operator=(const Cursor &o) { ptr = o.ptr; return *this;}

    //! Compute offset
    unsigned operator-(const Cursor &o) const { return ptr - o.ptr; }

    //Cursor operator+(const Cursor &o) const { return Cursor(ptr + o.ptr); }
    //Cursor operator-(const Cursor &o) const { return Cursor(ptr - o.ptr); }
    //Cursor& operator+=(const Cursor &o) { ptr += o.ptr; return *this; }
    //Cursor& operator-=(const Cursor &o) { ptr -= o.ptr; return *this; }

    Cursor operator+(unsigned offset) const { return Cursor(ptr + offset); }
    Cursor operator-(unsigned offset) const { return Cursor(ptr - offset); }
    Cursor& operator+=(unsigned offset) { ptr += offset; return *this; }
    Cursor& operator-=(unsigned offset) { ptr -= offset; return *this; }

    const unsigned char* get() const { return ptr; }

    //! @return whether this pointer is valid (i.e., it is != NULL)
    bool valid() { return ptr != 0; }


    ////////////////////////////////////////////////////////////////////////////
    // Skip methods

    void skipByte() { ++ptr; } //!< skip a byte
    void skipInt() { ptr += 4; } //!< skip a 32-bit integer
    //! Skip a 64-bit integer with variable-size encoding
    void skipBigInt() {
        while(*ptr & 128)
            ++ptr;
        ++ptr;
    }
    //!< Skip a Value
    void skipValue() { ptr += peekValueSize(); }


    ////////////////////////////////////////////////////////////////////////////
    // Peek methods

    /**
     * @param offset additional offset to add to the pointer
     * @return the 32-bit integer in big-endian under the cursor head
     */
    unsigned peekInt(unsigned offset=0) {
        const unsigned char *p = ptr + offset;
        return p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
    }

    //! @return the hashcode of the value under the cursor head
    unsigned peekValueHash() { return peekInt(4); }

    //! @return the total length of the value under the cursor head
    unsigned peekValueSize() { return peekInt(8) + 16; }


    ////////////////////////////////////////////////////////////////////////////
    // Read methods (move the cursor)

    /**
     * Read a byte and advance pointer.
     */
    unsigned char readByte() { return *(ptr++); }

    /**
     * Read a 16-bit integer in big-endian and advance pointer.
     */
    unsigned readShort() {
        unsigned result = ptr[0] << 8 | ptr[1];
        ptr += 2;
        return result;
    }

    /**
     * Read a 32-bit integer in big-endian and advance pointer.
     */
    unsigned readInt() {
        unsigned result = ptr[0] << 24 | ptr[1] << 16 | ptr[2] << 8 | ptr[3];
        ptr += 4;
        return result;
    }

    //! Read delta-compressed values
    unsigned readDelta1() { return readByte(); }
    unsigned readDelta2() { return readShort(); }
    unsigned readDelta3() {
        unsigned result = ptr[0] << 16 | ptr[1] << 8 | ptr[2];
        ptr += 3;
        return result;
    }
    unsigned readDelta4() { return readInt(); }

    /**
     * Read a 64-bit integer with variable-size encoding
     */
    uint64_t readBigInt() {
        unsigned shift = 0;
        uint64_t val = 0;
        while(*ptr & 128) {
            val |= static_cast<uint64_t>(*ptr & 0x7f) << shift;
            shift += 7;
            ++ptr;
        }
        val |= static_cast<uint64_t>(*ptr) << shift;
        ++ptr;
        return val;
    }

    /**
     * Read a value and advance pointer.
     *
     * The serialized format of a value is
     * +----+------+--------+------+---------+-----------+------------------+
     * | id | hash | length | type | typelen | type/lang | lexical          |
     * +----+------+--------+------+---------+-----------+------------------+
     *    4     4      4       2        2       typelen
     *
     * typelen includes terminal null character
     * length is the length of type/lang + lexical (including terminal null)
     */
    void readValue(Value &val);
};

/**
 * Memory-mapped file
 */
class MMapFile {

    int fd; //!< file descriptor
    Cursor pBegin; //!< start of the file
    Cursor pEnd; //!< end of the file

public:
    MMapFile(const char* fileName);
    ~MMapFile();

    Cursor begin() const { return pBegin; }
    Cursor end() const { return pEnd; }
};


/**
 * Read pages
 */
class PageReader {
    MMapFile in;

public:
    static const unsigned PAGE_SIZE = 16384;

    PageReader(const char *fileName) : in(fileName) {}

    /**
     * @return the requested page
     */
    Cursor getPage(unsigned page) {
        return in.begin() + (page * PAGE_SIZE);
    }

    /**
     * @param it a pointer inside a page
     * @return a pointer to the end of the page (just outside it)
     */
    Cursor getPageEnd(Cursor it) {
        return it + (PAGE_SIZE - (it - in.begin()) % PAGE_SIZE);
    }
};


}

#endif // CASTOR_STORE_MMAPFILE_H
