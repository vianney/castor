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
#include "tempfile.h"

#include <cstdio>
#include <cstring>
#include <sstream>
#include <sys/stat.h>
#include <cassert>

#include "pagewriter.h"

namespace castor {

unsigned TempFile::nextId_ = 0;

TempFile::TempFile(const std::string& baseName) : baseName_(baseName) {
    struct stat stbuf;
    while(fileName_.empty() || lstat(fileName_.c_str(), &stbuf) != -1) {
        std::stringstream name;
        name << baseName << '.' << nextId_++;
        fileName_ = name.str();
    }
    out_.open(fileName_.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    iter_ = buffer_;
    end_ = buffer_ + BUFFER_SIZE;
}

TempFile::~TempFile() {
    discard();
}

void TempFile::flush() {
    if(iter_ > buffer_) {
        out_.write(buffer_, iter_-buffer_);
        iter_ = buffer_;
    }
    out_.flush();
}

void TempFile::close() {
    flush();
    out_.close();
}

void TempFile::discard() {
    close();
    remove(fileName_.c_str());
}

void TempFile::write(unsigned len, const char* data) {
    // Fill the current buffer
    if(iter_ + len > end_) {
        unsigned remaining = end_ - iter_;
        memcpy(iter_, data, remaining);
        out_.write(buffer_, BUFFER_SIZE);
        iter_ = buffer_;
        len -= remaining;
        data += remaining;
    }
    // write big chuncks if any
    if(iter_ + len > end_) {
        assert(iter_ == buffer_);
        unsigned chunks = len / BUFFER_SIZE;
        out_.write(data, chunks*BUFFER_SIZE);
        len -= chunks*BUFFER_SIZE;
        data += chunks*BUFFER_SIZE;
    }
    // write the remaining
    memcpy(iter_, data, len);
    iter_ += len;
}

void TempFile::writeBigInt(uint64_t val) {
    while(val >= 128) {
        unsigned char c = static_cast<unsigned char>(val | 128);
        if(iter_ == end_) {
            out_.write(buffer_, BUFFER_SIZE);
            iter_ = buffer_;
        }
        *(iter_++) = c;
        val >>= 7;
    }
    if(iter_ == end_) {
        out_.write(buffer_, BUFFER_SIZE);
        iter_ = buffer_;
    }
    *(iter_++) = static_cast<unsigned char>(val);
}

void TempFile::writeInt(unsigned val) {
    unsigned char buf[4], *it = buf;
    PageWriter::writeInt(it, val);
    write(4, reinterpret_cast<char*>(buf));
}

void TempFile::writeValue(const Value& val) {
    unsigned typelen;
    if(val.type == Value::TYPE_CUSTOM)
        typelen = val.typeUriLen + 1;
    else if(val.isPlain() && val.language != nullptr)
        typelen = val.languageLen + 1;
    else
        typelen = 0;
    unsigned len = 16 + val.lexicalLen + 1 + typelen;

    unsigned char buffer[len];
    unsigned char* it = buffer;
    PageWriter::writeInt(it, val.id);
    PageWriter::writeInt(it, val.hash());
    PageWriter::writeInt(it, typelen + val.lexicalLen + 1);
    assert(val.type < 1<<16);
    assert(typelen < 1<<16);
    PageWriter::writeInt(it, val.type << 16 | typelen);
    if(val.type == Value::TYPE_CUSTOM)
        memcpy(it, val.typeUri, typelen);
    else if(val.isPlain() && val.language != nullptr)
        memcpy(it, val.language, typelen);
    it += typelen;
    memcpy(it, val.lexical, val.lexicalLen + 1);

    write(len, reinterpret_cast<char*>(buffer));
}

}
