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
#include "pagewriter.h"

#include <cstring>

namespace castor {

PageWriter::PageWriter(const char* fileName) :
        Buffer(PAGE_SIZE),
        out_(fileName, std::ios::out | std::ios::binary | std::ios::trunc),
        page_(0) {
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

void PageWriter::directWrite(const unsigned char *data, std::size_t len) {
    assert(offset() == 0);
    out_.write(reinterpret_cast<const char*>(data), len);
    page_ += len / PAGE_SIZE;
    std::size_t overflow = len % PAGE_SIZE;
    if(overflow > 0) {
        std::size_t padding = PAGE_SIZE - overflow;
        unsigned char blank[padding];
        memset(blank, 0, padding);
        out_.write(reinterpret_cast<const char*>(blank), padding);
        ++page_;
    }
}

void PageWriter::flush() {
    if(remaining() > 0) {
        unsigned char blank[remaining()];
        memset(blank, 0, remaining() * sizeof(unsigned char));
        write(blank, remaining());
    }
    clear(); // reset pointer
    directWrite(get());
}

}
