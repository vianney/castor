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
#include "readutils.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cassert>

namespace castor {

void Cursor::readValue(Value &val) {
    val.clean();
    val.id = readInt();
    skipInt(); // skip hash
    unsigned len = readInt();
    unsigned typelen = readInt();
    val.type = static_cast<Value::Type>(typelen >> 16);
    typelen = typelen & 0xffff;
    val.lexicalLen = len - typelen - 1;
    val.lexical = reinterpret_cast<const char*>(ptr + typelen);
    if(val.type == Value::TYPE_CUSTOM) {
        val.typeUri = reinterpret_cast<const char*>(ptr);
        val.typeUriLen = typelen - 1;
        val.isInterpreted = true;
    } else {
        assert(val.type >= Value::TYPE_BLANK && val.type < Value::TYPE_CUSTOM);
        val.typeUri = Value::TYPE_URIS[val.type];
        val.typeUriLen = Value::TYPE_URIS_LEN[val.type];
        if(val.isPlain()) {
            if(typelen > 0) {
                val.language = reinterpret_cast<const char*>(ptr);
                val.languageLen = typelen - 1;
            } else {
                val.language = nullptr;
                val.languageLen = 0;
            }
            val.isInterpreted = true;
        } else {
            val.isInterpreted = false;
        }
    }
    ptr += len;
}


MMapFile::MMapFile(const char *fileName) {
    fd = open(fileName, O_RDONLY);
    if(fd == -1)
        throw "Unable to open file";
    size_t size = lseek(fd, 0, SEEK_END);
    if(size < 0)
        throw "Unable to seek file";
    pBegin = Cursor(static_cast<const unsigned char*>
                        (mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0)));
    if(pBegin.get() == MAP_FAILED)
        throw "Unable to map file";
    pEnd = pBegin + size;
}

MMapFile::~MMapFile() {
    munmap(const_cast<unsigned char*>(pBegin.get()), pEnd - pBegin);
    close(fd);
}

}
