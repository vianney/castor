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
#ifndef CASTOR_TOOLS_CASTORLD_PAGEWRITER_H
#define CASTOR_TOOLS_CASTORLD_PAGEWRITER_H

#include <fstream>
#include "util.h"

namespace castor {

/**
 * Writing out in pages
 */
class PageWriter : public Buffer {
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
    std::size_t offset() const { return written(); }

    /**
     * Goto page p
     */
    void seek(unsigned p);

    /**
     * Write entire pages directly.
     * The last page will be padded with zeros.
     *
     * @param data the data of the page
     * @param len length of data
     */
    void directWrite(const unsigned char* data, std::size_t len=PAGE_SIZE);

    /**
     * Pad the remaining of the page with zeros and write the page to disk.
     */
    void flush();

private:
    std::ofstream  out_;               //!< database output stream
    unsigned       page_;              //!< current page number
};

}

#endif // CASTOR_TOOLS_CASTORLD_PAGEWRITER_H
