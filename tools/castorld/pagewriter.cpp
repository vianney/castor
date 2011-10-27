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
#include "pagewriter.h"

#include <cstring>

namespace castor {

PageWriter::PageWriter(const char *fileName)
        : out(fileName, std::ios::out | std::ios::binary | std::ios::trunc),
          page(0) {
    iter = buffer;
    bufEnd = iter + PAGE_SIZE;
}

PageWriter::~PageWriter() {
    close();
}

void PageWriter::close() {
    out.flush();
    out.close();
}

void PageWriter::setPage(unsigned p) {
    out.seekp(p * PAGE_SIZE, std::ios::beg);
    page = p;
}

void PageWriter::directWrite(const void *data) {
    out.write(static_cast<const char*>(data), PAGE_SIZE);
    page++;
}

void PageWriter::flushPage() {
    memset(iter, 0, getRemaining() * sizeof(char));
    directWrite(buffer);
    iter = buffer;
}

void PageWriter::write(const void *data, unsigned len) {
    memcpy(iter, data, len);
    iter += len;
}

void PageWriter::writeByte(unsigned char byte) {
    *(iter++) = byte;
}

void PageWriter::writeInt(unsigned char* &it, unsigned value) {
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
        *(iter++) = value >> 16;
        *(iter++) = (value >> 8) & 0xff;
        *(iter++) = value & 0xff;
    } else if(value >= 1 << 8) {
        *(iter++) = value >> 8;
        *(iter++) = value & 0xff;
    } else if(value > 0) {
        *(iter++) = value;
    }
}


}
