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
#ifndef CASTOR_TOOLS_CASTORLD_TEMPFILE_H
#define CASTOR_TOOLS_CASTORLD_TEMPFILE_H

#include <string>
#include <fstream>
#include <cstdint>

#include "model.h"
#include "store/readutils.h"

namespace castor {

/**
 * A temporary file. Also includes utilities to read/write it.
 */
class TempFile {

    std::string baseName; //!< basename for name creation
    std::string fileName; //!< name of this temporary file
    std::ofstream out; //!< file stream

    static unsigned nextId; //!< next id for name creation

    static const unsigned BUFFER_SIZE = 16384; //!< buffer size
    char buffer[BUFFER_SIZE]; //!< write buffer
    char *iter; //!< current write pointer in the buffer
    char *bufEnd; //!< pointer to the end of the buffer (first byte outside)

public:

    TempFile(const std::string &baseName);
    ~TempFile();

    /**
     * @return the basename used to construct the filename
     */
    const std::string& getBaseName() const { return baseName; }
    /**
     * @return the name of this temporary file
     */
    const std::string& getFileName() const { return fileName; }

    /**
     * Flush the buffer.
     */
    void flush();
    /**
     * Close the file.
     */
    void close();
    /**
     * Discard this temporary file (remove it from disk).
     */
    void discard();

    /**
     * Write arbitrary data
     */
    void write(unsigned len, const char* data);
    /**
     * Write a 32-bit unsigned integer to the page in big endian encoding.
     * @see PageWriter::writeInt(unsigned)
     */
    void writeInt(unsigned val);
    /**
     * Write a big integer with variable-length encoding
     */
    void writeBigInt(uint64_t val);
    /**
     * Serialize a value. The value must have a lexical form.
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
    void writeValue(const Value &val);
};

}

#endif // CASTOR_TOOLS_CASTORLD_TEMPFILE_H
