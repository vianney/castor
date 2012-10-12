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
#include <cstring>
#include <cstdint>

#include "util.h"
#include "model.h"

namespace castor {

/**
 * A value complemented with early identifiers as VarInts.
 *
 * The serialized format of an early value is
 * +----------------------+--------------+---------------+----------+
 * |        Value         | earlyLexical | earlyDatatype | earlyTag |
 * +----------------------+--------------+---------------+----------+
 *  Value::SERIALIZED_SIZE     varint         varint        varint
 */
class EarlyValue : public Value {
public:
    unsigned long earlyLexical;
    unsigned long earlyDatatype;
    unsigned long earlyTag;

    EarlyValue() : earlyLexical(0), earlyDatatype(0), earlyTag(0) {}
    EarlyValue(raptor_term* term) :
        Value(term), earlyLexical(0), earlyDatatype(0), earlyTag(0) {}

    /**
     * Deserialize a temporary value and advance cursor.
     */
    explicit EarlyValue(Cursor& cur) : Value(cur) {
        earlyLexical = cur.readVarInt();
        earlyDatatype = cur.readVarInt();
        earlyTag = cur.readVarInt();
    }

    /**
     * @return the serialized raw value
     */
    Buffer serialize() const {
        Buffer buf(Value::SERIALIZED_SIZE + 3 * Buffer::MAX_VARINT_SIZE);
        buf.writeBuffer(Value::serialize());
        buf.writeVarInt(earlyLexical);
        buf.writeVarInt(earlyDatatype);
        buf.writeVarInt(earlyTag);
        return buf;
    }

    /**
     * Advance cursor to skip a raw value.
     */
    static void skip(Cursor& cur) {
        Value::skip(cur);
        cur.skipVarInt();
        cur.skipVarInt();
        cur.skipVarInt();
    }


    bool operator==(const EarlyValue& o) const {
        return category() == o.category() &&
               (!isNumeric() || numCategory() == o.numCategory()) &&
               earlyLexical == o.earlyLexical &&
               earlyDatatype == o.earlyDatatype &&
               earlyTag == o.earlyTag;
    }

    Hash::hash_t hash() const {
        Hash::hash_t result = category() << 16;
        if(isNumeric())
            result |= numCategory();
        result = Hash::hash(&earlyLexical,  sizeof(earlyLexical),  result);
        result = Hash::hash(&earlyDatatype, sizeof(earlyDatatype), result);
        result = Hash::hash(&earlyTag,      sizeof(earlyTag),      result);
        return result;
    }

};

/**
 * A temporary file. Also includes utilities to read/write it.
 */
class TempFile : public Buffer {
public:
    explicit TempFile(const std::string& baseName_);
    ~TempFile();

    //! Non-copyable
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;

    /**
     * @return the basename used to construct the filename
     */
    const std::string& baseName() const { return baseName_; }
    /**
     * @return the name of this temporary file
     */
    const std::string& fileName() const { return fileName_; }

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
     * Overwritten to flush the buffer when full.
     */
    std::size_t write(const unsigned char *data, std::size_t len);

private:
    std::string baseName_; //!< basename for name creation
    std::string fileName_; //!< name of this temporary file
    std::ofstream out_; //!< file stream

    static unsigned nextId_; //!< next id for name creation

    static constexpr unsigned BUFFER_SIZE = 16384; //!< buffer size
};

}

#endif // CASTOR_TOOLS_CASTORLD_TEMPFILE_H
