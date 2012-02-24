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
#ifndef CASTOR_TOOLS_CASTORLD_PAGEWRITER_H
#define CASTOR_TOOLS_CASTORLD_PAGEWRITER_H

#include <fstream>

#include "store/readutils.h"

namespace castor {

/**
 * Writing out in pages
 */
class PageWriter {
public:
    static constexpr unsigned PAGE_SIZE = PageReader::PAGE_SIZE;  //!< page size

    PageWriter(const char* fileName);
    ~PageWriter();

    //! Non-copyable
    PageWriter(const PageWriter&) = delete;
    PageWriter& operator=(const PageWriter&) = delete;

    /**
     * Close the writer
     */
    void close();

    /**
     * @return the current page number
     */
    unsigned page() const { return page_; }
    /**
     * @return the current offset in the page
     */
    unsigned offset() const { return iter_ - buffer_; }
    /**
     * @return remaining bytes left in the page
     */
    unsigned remaining() const { return end_ - iter_; }

    /**
     * Goto page p
     */
    void seek(unsigned p);

    /**
     * Write directly an entire page
     *
     * @param data the data of the page, must be of size PAGE_SIZE
     */
    void directWrite(const void* data);

    /**
     * Pad the remaining of the page with zeros and write the page to disk.
     */
    void flush();

    /**
     * Skip len bytes
     */
    void skip(unsigned len) { iter_ += len; }

    /**
     * Write raw data.
     *
     * @pre enough room should be available in the page
     */
    void write(const void* data, unsigned len);

    /**
     * Write a byte.
     *
     * @pre enough room should be available in the page
     */
    void writeByte(unsigned char byte);

    /**
     * Write a 32-bit unsigned integer to the page in big endian encoding.
     * Advances the write pointer.
     *
     * @pre enough room should be available in the page
     */
    void writeInt(unsigned value) { writeInt(iter_, value); }

    /**
     * Write a 32-bit unsigned integer to the page in big endian encoding at
     * the specified offset. Do not move the write pointer.
     */
    void writeInt(unsigned value, unsigned offset) {
        unsigned char *it = buffer_ + offset;
        writeInt(it, value);
    }
    /**
     * Static version of writeInt(unsigned) to write in other buffers.
     * Advances the write pointer.
     */
    static void writeInt(unsigned char*& it, unsigned value);

    /**
     * @return the number of bytes value would take to write using delta
     *         compression
     */
    static unsigned lenDelta(unsigned value);
    /**
     * Write an integer using delta compression (variable size).
     * Advances the write pointer.
     *
     * @pre enough room should be available in the page
     */
    void writeDelta(unsigned value);

private:
    std::ofstream  out_;               //!< database output stream
    unsigned       page_;              //!< current page number
    unsigned char  buffer_[PAGE_SIZE]; //!< page buffer
    unsigned char* iter_;              //!< current write pointer in the page
    unsigned char* end_;               //!< pointer to the end of the buffer (first byte outside)
};

}

#endif // CASTOR_TOOLS_CASTORLD_PAGEWRITER_H
