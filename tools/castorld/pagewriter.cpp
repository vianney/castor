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
#include "pagewriter.h"

#include <cstring>

namespace castor {

PageWriter::PageWriter(const char* fileName)
        : out_(fileName, std::ios::out | std::ios::binary | std::ios::trunc),
          page_(0) {
    iter_ = buffer_;
    end_ = iter_ + PAGE_SIZE;
}

PageWriter::~PageWriter() {
    close();
}

void PageWriter::close() {
    out_.flush();
    out_.close();
}

void PageWriter::seek(unsigned p) {
    out_.seekp(p * PAGE_SIZE, std::ios::beg);
    page_ = p;
}

void PageWriter::directWrite(const void* data) {
    out_.write(static_cast<const char*>(data), PAGE_SIZE);
    page_++;
}

void PageWriter::flush() {
    memset(iter_, 0, remaining() * sizeof(char));
    directWrite(buffer_);
    iter_ = buffer_;
}

void PageWriter::write(const void* data, unsigned len) {
    memcpy(iter_, data, len);
    iter_ += len;
}

void PageWriter::writeByte(unsigned char byte) {
    *(iter_++) = byte;
}

void PageWriter::writeInt(unsigned char*& it, unsigned value) {
    *(it++) = value >> 24;
    *(it++) = (value >> 16) & 0xff;
    *(it++) = (value >> 8) & 0xff;
    *(it++) = value & 0xff;
}

unsigned PageWriter::lenDelta(unsigned value) {
    if(value >= 1 << 24)
        return 4;
    else if(value >= 1 << 16)
        return 3;
    else if(value >= 1 << 8)
        return 2;
    else if(value > 0)
        return 1;
    else
        return 0;
}

void PageWriter::writeDelta(unsigned value) {
    if(value >= 1 << 24) {
        writeInt(value);
    } else if(value >= 1 << 16) {
        *(iter_++) = value >> 16;
        *(iter_++) = (value >> 8) & 0xff;
        *(iter_++) = value & 0xff;
    } else if(value >= 1 << 8) {
        *(iter_++) = value >> 8;
        *(iter_++) = value & 0xff;
    } else if(value > 0) {
        *(iter_++) = value;
    }
}


}
