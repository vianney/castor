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
#include "tempfile.h"

#include <cstdio>
#include <cstring>
#include <sstream>
#include <sys/stat.h>
#include <cassert>

#include "pagewriter.h"

namespace castor {

unsigned TempFile::nextId = 0;

TempFile::TempFile(const std::string &baseName)
        : baseName(baseName) {
    struct stat stbuf;
    while(fileName.empty() || lstat(fileName.c_str(), &stbuf) != -1) {
        std::stringstream name;
        name << baseName << '.' << nextId++;
        fileName = name.str();
    }
    out.open(fileName.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    iter = buffer;
    bufEnd = buffer + BUFFER_SIZE;
}

TempFile::~TempFile() {
    discard();
}

void TempFile::flush() {
    if(iter > buffer) {
        out.write(buffer, iter-buffer);
        iter = buffer;
    }
    out.flush();
}

void TempFile::close() {
    flush();
    out.close();
}

void TempFile::discard() {
    close();
    remove(fileName.c_str());
}

void TempFile::write(unsigned len, const char *data) {
    // Fill the current buffer
    if(iter + len > bufEnd) {
        unsigned remaining = bufEnd - iter;
        memcpy(iter, data, remaining);
        out.write(buffer, BUFFER_SIZE);
        iter = buffer;
        len -= remaining;
        data += remaining;
    }
    // write big chuncks if any
    if(iter + len > bufEnd) {
        assert(iter == buffer);
        unsigned chunks = len / BUFFER_SIZE;
        out.write(data, chunks*BUFFER_SIZE);
        len -= chunks*BUFFER_SIZE;
        data += chunks*BUFFER_SIZE;
    }
    // write the remaining
    memcpy(iter, data, len);
    iter += len;
}

void TempFile::writeBigInt(uint64_t val) {
    while(val >= 128) {
        unsigned char c = static_cast<unsigned char>(val | 128);
        if(iter == bufEnd) {
            out.write(buffer, BUFFER_SIZE);
            iter = buffer;
        }
        *(iter++) = c;
        val >>= 7;
    }
    if(iter == bufEnd) {
        out.write(buffer, BUFFER_SIZE);
        iter = buffer;
    }
    *(iter++) = static_cast<unsigned char>(val);
}

void TempFile::writeValue(const Value &val) {
    unsigned typelen = 0;
    if(val.type == Value::TYPE_CUSTOM)
        typelen = val.typeUriLen;
    else if(val.isPlain() && val.language != NULL)
        typelen = val.languageLen;
    else
        typelen = 0;
    unsigned len = 16 + val.lexicalLen + typelen;

    unsigned char buffer[len];
    unsigned char *it = buffer;
    PageWriter::writeInt(it, val.id);
    PageWriter::writeInt(it, val.hash());
    PageWriter::writeInt(it, typelen + val.lexicalLen);
    assert(val.type < 1<<16);
    assert(typelen < 1<<16);
    PageWriter::writeInt(it, val.type << 16 | typelen);
    if(val.type == Value::TYPE_CUSTOM)
        memcpy(it, val.typeUri, typelen);
    else if(val.isPlain() && val.language != NULL)
        memcpy(it, val.language, typelen);
    it += typelen;
    memcpy(it, val.lexical, val.lexicalLen);

    write(len, reinterpret_cast<char*>(buffer));
}

}
