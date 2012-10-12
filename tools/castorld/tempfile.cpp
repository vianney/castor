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


namespace castor {

unsigned TempFile::nextId_ = 0;

TempFile::TempFile(const std::string& baseName) : Buffer(BUFFER_SIZE),
                                                  baseName_(baseName) {
    struct stat stbuf;
    while(fileName_.empty() || lstat(fileName_.c_str(), &stbuf) != -1) {
        std::stringstream name;
        name << baseName << '.' << nextId_++;
        fileName_ = name.str();
    }
    out_.open(fileName_.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
}

TempFile::~TempFile() {
    discard();
}

void TempFile::flush() {
    if(written() > 0) {
        out_.write(reinterpret_cast<const char*>(get()), written());
        clear();
    }
}

void TempFile::close() {
    flush();
    out_.close();
}

void TempFile::discard() {
    close();
    remove(fileName_.c_str());
}

std::size_t TempFile::write(const unsigned char* data, std::size_t len) {
    std::size_t towrite = len;
    if(towrite >= remaining()) {
        // fill the current buffer
        towrite -= remaining();
        data += Buffer::write(data, remaining());
        flush();
        // write big chuncks if any
        if(towrite >= BUFFER_SIZE) {
            unsigned chunks = towrite / BUFFER_SIZE;
            out_.write(reinterpret_cast<const char*>(data), chunks*BUFFER_SIZE);
            towrite -= chunks*BUFFER_SIZE;
            data += chunks*BUFFER_SIZE;
        }
    }
    // keep the remaining in the buffer
    if(towrite > 0)
        Buffer::write(data, towrite);

    return len;
}

}
