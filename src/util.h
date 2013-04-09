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
#ifndef CASTOR_UTIL_H
#define CASTOR_UTIL_H

#include <exception>
#include <string>
#include <sstream>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cassert>

#include <sys/param.h>  /* attempt to define endianness */
#ifdef linux
# include <endian.h>    /* attempt to define endianness */
#endif

namespace castor {

static_assert(sizeof(void*) >= sizeof(uint64_t),
              "A 64-bit architecture is required.");
static_assert(sizeof(unsigned long) >= sizeof(uint64_t),
              "unsigned long must be at least 64 bits");

/* Guess if we can assume little-endian byte order.
 */
#if (defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) && \
     __BYTE_ORDER == __LITTLE_ENDIAN) || \
    (defined(i386) || defined(__i386__) || defined(__i486__) || \
     defined(__i586__) || defined(__i686__) || defined(vax) || defined(MIPSEL))
static constexpr bool littleEndian = true;
#else
static constexpr bool littleEndian = false;
#endif

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

    // FIXME: garbage collection?
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
    // TODO: make this portable
    asm("bsrl %1,%0" : "=r"(result) : "r"(i));
    return result;
}

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
    std::ptrdiff_t operator-(const Cursor& o) const { return ptr_ - o.ptr_; }

    Cursor operator+(std::size_t offset) const { return Cursor(ptr_ + offset); }
    Cursor operator-(std::size_t offset) const { return Cursor(ptr_ - offset); }
    Cursor& operator+=(std::size_t offset) { ptr_ += offset; return *this; }
    Cursor& operator-=(std::size_t offset) { ptr_ -= offset; return *this; }

    const unsigned char* get() const { return ptr_; }

    //! @return whether this pointer is valid (i.e., it is != nullptr)
    bool valid() const { return ptr_ != nullptr; }


    ////////////////////////////////////////////////////////////////////////////
    // Peek methods

    /**
     * @param offset additional offset to add to the pointer
     * @return the 16-bit integer in little-endian under the cursor head
     */
    unsigned peekShort(std::size_t offset=0) const {
        const unsigned char* p = ptr_ + offset;
        if(littleEndian)
            return *reinterpret_cast<const uint16_t*>(p);
        else
            return static_cast<unsigned>(p[1]) << 8 |
                   static_cast<unsigned>(p[0]);
    }

    /**
     * @param offset additional offset to add to the pointer
     * @return the 32-bit integer in little-endian under the cursor head
     */
    unsigned peekInt(std::size_t offset=0) const {
        const unsigned char* p = ptr_ + offset;
        if(littleEndian)
            return *reinterpret_cast<const uint32_t*>(p);
        else
            return static_cast<unsigned>(p[3]) << 24 |
                   static_cast<unsigned>(p[2]) << 16 |
                   static_cast<unsigned>(p[1]) << 8 |
                   static_cast<unsigned>(p[0]);
    }

    /**
     * @param offset additional offset to add to the pointer
     * @return the 64-bit integer in little-endian under the cursor head
     */
    unsigned long peekLong(std::size_t offset=0) const {
        const unsigned char* p = ptr_ + offset;
        if(littleEndian)
            return *reinterpret_cast<const uint64_t*>(p);
        else
            return static_cast<unsigned long>(p[7]) << 56 |
                   static_cast<unsigned long>(p[6]) << 48 |
                   static_cast<unsigned long>(p[5]) << 40 |
                   static_cast<unsigned long>(p[4]) << 32 |
                   static_cast<unsigned long>(p[3]) << 24 |
                   static_cast<unsigned long>(p[2]) << 16 |
                   static_cast<unsigned long>(p[1]) << 8 |
                   static_cast<unsigned long>(p[0]);
    }


    ////////////////////////////////////////////////////////////////////////////
    // Skip methods (move the cursor)

    void skipByte() { ++ptr_; } //!< skip a byte
    void skipInt() { ptr_ += 4; } //!< skip a 32-bit integer
    void skipLong() { ptr_ += 8; } //!< skip a 64-bit integer
    //! Skip a 64-bit integer with variable-size encoding
    void skipVarInt() {
        while(*ptr_ & 128)
            ++ptr_;
        ++ptr_;
    }


    ////////////////////////////////////////////////////////////////////////////
    // Read methods (move the cursor)

    /**
     * Read a byte and advance pointer.
     */
    unsigned char readByte() { return *(ptr_++); }

    /**
     * Read a 16-bit integer in little-endian and advance pointer.
     */
    unsigned readShort() {
        unsigned result = peekShort();
        ptr_ += 2;
        return result;
    }

    /**
     * Read a 32-bit integer in little-endian and advance pointer.
     */
    unsigned readInt() {
        unsigned result = peekInt();
        ptr_ += 4;
        return result;
    }

    /**
     * Read a 64-bit integer in little-endian and advance pointer.
     */
    unsigned long readLong() {
        unsigned long result = peekLong();
        ptr_ += 8;
        return result;
    }

    //! Read delta-compressed values
    unsigned readDelta1() { return readByte(); }
    unsigned readDelta2() { return readShort(); }
    unsigned readDelta3() {
        unsigned result;
        if(littleEndian)
            result = *reinterpret_cast<const uint16_t*>(ptr_) |
                     static_cast<unsigned>(ptr_[2]) << 16;
        else
            result = static_cast<unsigned>(ptr_[2]) << 16 |
                     static_cast<unsigned>(ptr_[1]) << 8 |
                     static_cast<unsigned>(ptr_[0]);
        ptr_ += 3;
        return result;
    }
    unsigned readDelta4() { return readInt(); }

    /**
     * Read a 64-bit integer with variable-size encoding
     */
    unsigned long readVarInt() {
        unsigned shift = 0;
        unsigned long val = 0;
        while(*ptr_ & 128) {
            val |= static_cast<unsigned long>(*ptr_ & 0x7f) << shift;
            shift += 7;
            ++ptr_;
        }
        val |= static_cast<unsigned long>(*ptr_) << shift;
        ++ptr_;
        return val;
    }

private:
    const unsigned char* ptr_;
};

/**
 * A buffer where data can be written.
 */
class Buffer {
public:
    explicit Buffer(std::size_t size) {
        buffer_ = new unsigned char[size];
        iter_ = buffer_;
        end_ = buffer_ + size;
    }

    ~Buffer() { if(buffer_ != nullptr) delete [] buffer_; }

    //! Non-copyable
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    //! Moveable
    Buffer(Buffer&& o) { *this = std::move(o); }
    Buffer& operator=(Buffer&& o) {
        buffer_ = o.buffer_;
        iter_ = o.iter_;
        end_ = o.end_;
        o.buffer_ = nullptr;
        return *this;
    }

    /**
     * Clear the buffer.
     */
    void clear() { iter_ = buffer_; }

    /**
     * @return the number of bytes written
     */
    std::size_t written() const { return iter_ - buffer_; }
    /**
     * @return the number of bytes remaining in this buffer
     */
    std::size_t remaining() const { return end_ - iter_; }

    /**
     * @return the buffer
     */
    const unsigned char* get() const { return buffer_; }

    ////////////////////////////////////////////////////////////////////////////
    // Main write methods

    /**
     * Skip len bytes
     */
    virtual void skip(std::size_t len) { assert(len <= remaining());
                                         iter_ += len; }

    /**
     * Append arbitrary data to the buffer.
     *
     * @pre len <= remaining()
     * @param data data to write
     * @param len size of data
     * @return the number of bytes written
     */
    virtual std::size_t write(const unsigned char* data, std::size_t len) {
        assert(len <= remaining());
        memcpy(iter_, data, len);
        iter_ += len;
        return len;
    }

    /**
     * Overwrite data at a specified position in the buffer.
     *
     * @pre offset + len <= written()
     * @param data data to write
     * @param len size of data
     * @param offset offset in the buffer
     * @return the number of bytes written
     */
    virtual std::size_t write(const unsigned char *data, std::size_t len,
                              std::size_t offset) {
        assert(offset + len <= written());
        memcpy(buffer_ + offset, data, len);
        return len;
    }

    ////////////////////////////////////////////////////////////////////////////
    // Convenience write methods (should call write())

    /**
     * Special offset value to append data rather than overwriting existing
     * data in the buffer.
     */
    static constexpr std::size_t OFFSET_APPEND = static_cast<std::size_t>(-1);

    /**
     * Copy the content of another buffer (only the bytes written).
     * @return the number of bytes written
     */
    std::size_t writeBuffer(const Buffer& buf, std::size_t offset=OFFSET_APPEND) {
        if(offset == OFFSET_APPEND)
            return write(buf.get(), buf.written());
        else
            return write(buf.get(), buf.written(), offset);
    }

    /**
     * Write a byte.
     * @return the number of bytes written
     */
    std::size_t writeByte(unsigned char val, std::size_t offset=OFFSET_APPEND) {
        if(offset == OFFSET_APPEND)
            return write(&val, 1);
        else
            return write(&val, 1, offset);
    }

    /**
     * Write a 16-bit unsigned integer in little-endian encoding.
     * @return the number of bytes written
     */
    std::size_t writeShort(unsigned short val, std::size_t offset=OFFSET_APPEND) {
        unsigned char data[2] = {static_cast<unsigned char>(val & 0xff),
                                 static_cast<unsigned char>(val >> 8)};
        if(offset == OFFSET_APPEND)
            return write(data, 2);
        else
            return write(data, 2, offset);
    }

    /**
     * Write a 32-bit unsigned integer in little-endian encoding
     * @return the number of bytes written
     */
    std::size_t writeInt(unsigned val, std::size_t offset=OFFSET_APPEND) {
        unsigned char data[4] = {static_cast<unsigned char>(val & 0xff),
                                 static_cast<unsigned char>((val >> 8) & 0xff),
                                 static_cast<unsigned char>((val >> 16) & 0xff),
                                 static_cast<unsigned char>(val >> 24)};
        if(offset == OFFSET_APPEND)
            return write(data, 4);
        else
            return write(data, 4, offset);
    }

    /**
     * Write a 64-bit unsigned integer in little-endian encoding
     * @return the number of bytes written
     */
    std::size_t writeLong(unsigned long val, std::size_t offset=OFFSET_APPEND) {
        unsigned char data[8] = {static_cast<unsigned char>(val & 0xff),
                                 static_cast<unsigned char>((val >> 8) & 0xff),
                                 static_cast<unsigned char>((val >> 16) & 0xff),
                                 static_cast<unsigned char>((val >> 24) & 0xff),
                                 static_cast<unsigned char>((val >> 32) & 0xff),
                                 static_cast<unsigned char>((val >> 40) & 0xff),
                                 static_cast<unsigned char>((val >> 48) & 0xff),
                                 static_cast<unsigned char>(val >> 56)};
        if(offset == OFFSET_APPEND)
            return write(data, 8);
        else
            return write(data, 8, offset);
    }

    /**
     * Maximum size of a big integer encoded with variable-length.
     */
    static constexpr std::size_t MAX_VARINT_SIZE = 10;

    /**
     * Write a big integer with variable-length encoding
     * @return the number of bytes written
     */
    std::size_t writeVarInt(unsigned long val) {
        unsigned char data[MAX_VARINT_SIZE], *it = data;
        std::size_t len = 0;
        while(val >= 128) {
            *(it++) = static_cast<unsigned char>(val | 128);
            ++len;
            val >>= 7;
        }
        *it = static_cast<unsigned char>(val);
        ++len;
        return write(data, len);
    }

    /**
     * @param value a value
     * @return the number of bytes value would take to write using delta
     *         compression
     */
    static std::size_t lenDelta(unsigned value) {
        if     (value >= 1 << 24) return 4;
        else if(value >= 1 << 16) return 3;
        else if(value >= 1 << 8)  return 2;
        else if(value > 0)        return 1;
        else                      return 0;
    }

    /**
     * Write an integer using delta compression (variable size).
     *
     * @param value the value
     */
    std::size_t writeDelta(unsigned value) {
        if(value >= 1 << 24) {
            return writeInt(value);
        } else if(value >= 1 << 16) {
            unsigned char data[3] = {static_cast<unsigned char>(value & 0xff),
                                     static_cast<unsigned char>((value >> 8) & 0xff),
                                     static_cast<unsigned char>(value >> 16)};
            return write(data, 3);
        } else if(value >= 1 << 8) {
            return writeShort(value);
        } else if(value > 0) {
            return writeByte(value);
        } else {
            return 0;
        }
    }

private:
    unsigned char* buffer_; //!< buffer
    unsigned char* iter_;   //!< current write pointer in the buffer
    unsigned char* end_;    //!< pointer to the end of the buffer (first byte outside)
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

    //! @return size of the file
    std::size_t size() const { return end_ - begin_; }

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
    static constexpr std::size_t PAGE_SIZE = 16384;

    PageReader(const char* fileName) : in_(fileName) {}

    //! Non-copyable
    PageReader(const PageReader&) = delete;
    PageReader& operator=(const PageReader&) = delete;

    /**
     * @return the requested page
     */
    Cursor page(unsigned page) const {
        return in_.begin() + (page * PAGE_SIZE);
    }

    /**
     * @param it a pointer inside a page
     * @return a pointer to the end of the page (just outside it)
     */
    Cursor pageEnd(Cursor it) const {
        return it + (PAGE_SIZE - (it - in_.begin()) % PAGE_SIZE);
    }

private:
    MMapFile in_;
};

}

#endif // CASTOR_UTIL_H
