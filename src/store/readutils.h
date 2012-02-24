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
#ifndef CASTOR_STORE_READUTILS_H
#define CASTOR_STORE_READUTILS_H

#include "../model.h"

namespace castor {

/**
 * Pointer in a mapped file
 */
class Cursor {
public:
    Cursor() {}
    Cursor(const unsigned char* ptr) : ptr_(ptr) {}

    //! Default copy constructor
    Cursor(const Cursor&) = default;
    //! Default assignment operator
    Cursor& operator=(const Cursor&) = default;

    bool operator==(const Cursor& o) const { return ptr_ == o.ptr_; }
    bool operator!=(const Cursor& o) const { return ptr_ != o.ptr_; }
    bool operator< (const Cursor& o) const { return ptr_ <  o.ptr_; }
    bool operator<=(const Cursor& o) const { return ptr_ <= o.ptr_; }
    bool operator> (const Cursor& o) const { return ptr_ >  o.ptr_; }
    bool operator>=(const Cursor& o) const { return ptr_ >= o.ptr_; }

    //! Compute offset
    unsigned operator-(const Cursor& o) const { return ptr_ - o.ptr_; }

    Cursor operator+(unsigned offset) const { return Cursor(ptr_ + offset); }
    Cursor operator-(unsigned offset) const { return Cursor(ptr_ - offset); }
    Cursor& operator+=(unsigned offset) { ptr_ += offset; return *this; }
    Cursor& operator-=(unsigned offset) { ptr_ -= offset; return *this; }

    const unsigned char* get() const { return ptr_; }

    //! @return whether this pointer is valid (i.e., it is != nullptr)
    bool valid() { return ptr_ != 0; }


    ////////////////////////////////////////////////////////////////////////////
    // Skip methods

    void skipByte() { ++ptr_; } //!< skip a byte
    void skipInt() { ptr_ += 4; } //!< skip a 32-bit integer
    //! Skip a 64-bit integer with variable-size encoding
    void skipBigInt() {
        while(*ptr_ & 128)
            ++ptr_;
        ++ptr_;
    }
    //!< Skip a Value
    void skipValue() { ptr_ += peekValueSize(); }


    ////////////////////////////////////////////////////////////////////////////
    // Peek methods

    /**
     * @param offset additional offset to add to the pointer
     * @return the 32-bit integer in big-endian under the cursor head
     */
    unsigned peekInt(unsigned offset=0) {
        const unsigned char* p = ptr_ + offset;
        return p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
    }

    //! @return the hashcode of the value under the cursor head
    Hash::hash_t peekValueHash() { return peekInt(4); }

    //! @return the total length of the value under the cursor head
    unsigned peekValueSize() { return peekInt(8) + 16; }


    ////////////////////////////////////////////////////////////////////////////
    // Read methods (move the cursor)

    /**
     * Read a byte and advance pointer.
     */
    unsigned char readByte() { return *(ptr_++); }

    /**
     * Read a 16-bit integer in big-endian and advance pointer.
     */
    unsigned readShort() {
        unsigned result = ptr_[0] << 8 | ptr_[1];
        ptr_ += 2;
        return result;
    }

    /**
     * Read a 32-bit integer in big-endian and advance pointer.
     */
    unsigned readInt() {
        unsigned result = ptr_[0] << 24 | ptr_[1] << 16 | ptr_[2] << 8 | ptr_[3];
        ptr_ += 4;
        return result;
    }

    //! Read delta-compressed values
    unsigned readDelta1() { return readByte(); }
    unsigned readDelta2() { return readShort(); }
    unsigned readDelta3() {
        unsigned result = ptr_[0] << 16 | ptr_[1] << 8 | ptr_[2];
        ptr_ += 3;
        return result;
    }
    unsigned readDelta4() { return readInt(); }

    /**
     * Read a 64-bit integer with variable-size encoding
     */
    uint64_t readBigInt() {
        unsigned shift = 0;
        uint64_t val = 0;
        while(*ptr_ & 128) {
            val |= static_cast<uint64_t>(*ptr_ & 0x7f) << shift;
            shift += 7;
            ++ptr_;
        }
        val |= static_cast<uint64_t>(*ptr_) << shift;
        ++ptr_;
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
    void readValue(Value& val);

private:
    const unsigned char* ptr_;
};


/**
 * Memory-mapped file
 */
class MMapFile {
public:
    MMapFile(const char* fileName);
    ~MMapFile();

    //! Non-copyable
    MMapFile(const MMapFile&) = delete;
    MMapFile& operator=(const MMapFile&) = delete;

    Cursor begin() const { return begin_; } //!< @return begin of the file
    Cursor end()   const { return end_;   } //!< @return end of the file

private:
    int    fd_;    //!< file descriptor
    Cursor begin_; //!< start of the file
    Cursor end_;   //!< end of the file
};


/**
 * Read pages
 */
class PageReader {
public:
    static constexpr unsigned PAGE_SIZE = 16384;

    PageReader(const char* fileName) : in_(fileName) {}

    //! Non-copyable
    PageReader(const PageReader&) = delete;
    PageReader& operator=(const PageReader&) = delete;

    /**
     * @return the requested page
     */
    Cursor page(unsigned page) {
        return in_.begin() + (page * PAGE_SIZE);
    }

    /**
     * @param it a pointer inside a page
     * @return a pointer to the end of the page (just outside it)
     */
    Cursor pageEnd(Cursor it) {
        return it + (PAGE_SIZE - (it - in_.begin()) % PAGE_SIZE);
    }

private:
    MMapFile in_;
};


}

#endif // CASTOR_STORE_READUTILS_H
