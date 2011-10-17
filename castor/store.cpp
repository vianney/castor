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

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <set>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include "store.h"

namespace castor {

Store::Store(const char* fileName) : db(fileName) {
    Cursor cur = db.getPage(0);

    // check magic number and version format
    if(memcmp(cur.get(), CASTOR_STORE_MAGIC, sizeof(CASTOR_STORE_MAGIC) - 1) != 0)
        throw "Invalid magic number";
    cur += sizeof(CASTOR_STORE_MAGIC) - 1;
    if(cur.readInt() != VERSION)
        throw "Invalid format version";


    // Get triples pointers
    for(int i = 0; i < 6; i++) {
        triplesStart[i] = cur.readInt();
        triplesIndex[i] = new BTree<TripleKey>(&db, cur.readInt());
    }

    // Get values pointers
    nbValues = cur.readInt();
    valuesStart = cur.readInt();
    valuesMapping = cur.readInt();
    valuesIndex = new ValueHashTree(&db, cur.readInt());
}

Store::~Store() {
    for(int i = 0; i < 6; i++)
        delete triplesIndex[i];
    delete valuesIndex;
}

void Store::fetchValue(Value::id_t id, Value &val) {
    // read mapping
    const unsigned EPP = PageReader::PAGE_SIZE / 8; // entries per page in map
    id--;
    Cursor cur = db.getPage(valuesMapping + id / EPP) + 8*(id % EPP);
    unsigned page = cur.readInt();
    unsigned offlen = cur.readInt();
    unsigned offset = offlen >> 16;

    // read value
    cur = db.getPage(page) + offset;
    cur.readValue(val);
}

void Store::lookupId(Value &val) {
    if(val.id > 0)
        return;

    // look for pages containing the hash
    val.ensureLexical();
    uint32_t hash = val.hash();
    Cursor listCur = valuesIndex->lookup(hash);
    if(!listCur.valid())
        return;

    // scan all candidates in the collision list
    for(Cursor listEnd = db.getPageEnd(listCur); listCur != listEnd;) {
        if(listCur.readInt() != hash)
            break;

        // scan page
        Cursor cur = db.getPage(listCur.readInt());
        cur.skipInt(); // skip next page header
        unsigned count = cur.readInt();
        unsigned idx = 0;
        for(; idx < count; ++idx) { // skip hashes before
            if(hash == cur.peekValueHash())
                break;
            cur.skipValue();
        }
        for(; idx < count; ++idx) {
            if(hash != cur.peekValueHash())
                break;
            Value v;
            cur.readValue(v);
            if(v == val) {
                val.id = v.id;
                return;
            }
        }
    }
}

Store::StatementQuery::StatementQuery(Store &store, Statement &stmt) : db(&store.db) {
    // determine index such that wildcards are the last components
    switch((stmt.subject == 0) |
           (stmt.predicate == 0) << 1 |
           (stmt.object == 0) << 2) {
    case 0: // (s,p,o)
    case 4: // (s,p,*)
    case 6: // (s,*,*)
    case 7: // (*,*,*)
        order = SPO;
        key.a = stmt.subject;
        key.b = stmt.predicate;
        key.c = stmt.object;
        break;
    case 1: // (*,p,o)
    case 5: // (*,p,*)
        order = POS;
        key.a = stmt.predicate;
        key.b = stmt.object;
        key.c = stmt.subject;
        break;
    case 2: // (s,*,o)
    case 3: // (*,*,o)
        order = OSP;
        key.a = stmt.object;
        key.b = stmt.subject;
        key.c = stmt.predicate;
        break;
    }

    // look for the first leaf
    nextPage = store.triplesIndex[order]->lookupLeaf(key);
    readNextPage();
}

bool Store::StatementQuery::next(Statement *stmt) {
    if(it == end && !readNextPage())
        return false;
    if(stmt != NULL) {
        switch(order) {
        case SPO:
            stmt->subject = it->a;
            stmt->predicate = it->b;
            stmt->object = it->c;
            break;
        case POS:
            stmt->predicate = it->a;
            stmt->object = it->b;
            stmt->subject = it->c;
            break;
        case OSP:
            stmt->object = it->a;
            stmt->subject = it->b;
            stmt->predicate = it->c;
            break;
        }
    }
    it++;
    return true;
}

bool Store::StatementQuery::readNextPage() {
    if(nextPage == 0)
        return false;

    // read page and interpret header
    Cursor cur = db->getPage(nextPage);
    Cursor pageEnd = cur + PageReader::PAGE_SIZE;
    nextPage = cur.readInt();

    // read first triple
    TripleKey t = {cur.readInt(),
                   cur.readInt(),
                   cur.readInt()};
    end = triples;
    if(key.matches(t)) // first triple is matching
        (*end++) = t;

    // Unpack other triples
    while(cur < pageEnd) {
        unsigned header = cur.readByte();
        if(header < 0x80) {
            // small gap in last component
            if(header == 0)
                break;
            t.c += header;
        } else {
            switch(header & 127) {
            case 0: t.c += 128; break;
            case 1: t.c += cur.readDelta1()+128; break;
            case 2: t.c += cur.readDelta2()+128; break;
            case 3: t.c += cur.readDelta3()+128; break;
            case 4: t.c += cur.readDelta4()+128; break;
            case 5: t.b += cur.readDelta1(); t.c = 1; break;
            case 6: t.b += cur.readDelta1(); t.c = cur.readDelta1()+1; break;
            case 7: t.b += cur.readDelta1(); t.c = cur.readDelta2()+1; break;
            case 8: t.b += cur.readDelta1(); t.c = cur.readDelta3()+1; break;
            case 9: t.b += cur.readDelta1(); t.c = cur.readDelta4()+1; break;
            case 10: t.b += cur.readDelta2(); t.c = 1; break;
            case 11: t.b += cur.readDelta2(); t.c = cur.readDelta1()+1; break;
            case 12: t.b += cur.readDelta2(); t.c = cur.readDelta2()+1; break;
            case 13: t.b += cur.readDelta2(); t.c = cur.readDelta3()+1; break;
            case 14: t.b += cur.readDelta2(); t.c = cur.readDelta4()+1; break;
            case 15: t.b += cur.readDelta3(); t.c = 1; break;
            case 16: t.b += cur.readDelta3(); t.c = cur.readDelta1()+1; break;
            case 17: t.b += cur.readDelta3(); t.c = cur.readDelta2()+1; break;
            case 18: t.b += cur.readDelta3(); t.c = cur.readDelta3()+1; break;
            case 19: t.b += cur.readDelta3(); t.c = cur.readDelta4()+1; break;
            case 20: t.b += cur.readDelta4(); t.c = 1; break;
            case 21: t.b += cur.readDelta4(); t.c = cur.readDelta1()+1; break;
            case 22: t.b += cur.readDelta4(); t.c = cur.readDelta2()+1; break;
            case 23: t.b += cur.readDelta4(); t.c = cur.readDelta3()+1; break;
            case 24: t.b += cur.readDelta4(); t.c = cur.readDelta4()+1; break;
            case 25: t.a += cur.readDelta1(); t.b = 1; t.c = 1; break;
            case 26: t.a += cur.readDelta1(); t.b = 1; t.c = cur.readDelta1()+1; break;
            case 27: t.a += cur.readDelta1(); t.b = 1; t.c = cur.readDelta2()+1; break;
            case 28: t.a += cur.readDelta1(); t.b = 1; t.c = cur.readDelta3()+1; break;
            case 29: t.a += cur.readDelta1(); t.b = 1; t.c = cur.readDelta4()+1; break;
            case 30: t.a += cur.readDelta1(); t.b = cur.readDelta1()+1; t.c = 1; break;
            case 31: t.a += cur.readDelta1(); t.b = cur.readDelta1()+1; t.c = cur.readDelta1()+1; break;
            case 32: t.a += cur.readDelta1(); t.b = cur.readDelta1()+1; t.c = cur.readDelta2()+1; break;
            case 33: t.a += cur.readDelta1(); t.b = cur.readDelta1()+1; t.c = cur.readDelta3()+1; break;
            case 34: t.a += cur.readDelta1(); t.b = cur.readDelta1()+1; t.c = cur.readDelta4()+1; break;
            case 35: t.a += cur.readDelta1(); t.b = cur.readDelta2()+1; t.c = 1; break;
            case 36: t.a += cur.readDelta1(); t.b = cur.readDelta2()+1; t.c = cur.readDelta1()+1; break;
            case 37: t.a += cur.readDelta1(); t.b = cur.readDelta2()+1; t.c = cur.readDelta2()+1; break;
            case 38: t.a += cur.readDelta1(); t.b = cur.readDelta2()+1; t.c = cur.readDelta3()+1; break;
            case 39: t.a += cur.readDelta1(); t.b = cur.readDelta2()+1; t.c = cur.readDelta4()+1; break;
            case 40: t.a += cur.readDelta1(); t.b = cur.readDelta3()+1; t.c = 1; break;
            case 41: t.a += cur.readDelta1(); t.b = cur.readDelta3()+1; t.c = cur.readDelta1()+1; break;
            case 42: t.a += cur.readDelta1(); t.b = cur.readDelta3()+1; t.c = cur.readDelta2()+1; break;
            case 43: t.a += cur.readDelta1(); t.b = cur.readDelta3()+1; t.c = cur.readDelta3()+1; break;
            case 44: t.a += cur.readDelta1(); t.b = cur.readDelta3()+1; t.c = cur.readDelta4()+1; break;
            case 45: t.a += cur.readDelta1(); t.b = cur.readDelta4()+1; t.c = 1; break;
            case 46: t.a += cur.readDelta1(); t.b = cur.readDelta4()+1; t.c = cur.readDelta1()+1; break;
            case 47: t.a += cur.readDelta1(); t.b = cur.readDelta4()+1; t.c = cur.readDelta2()+1; break;
            case 48: t.a += cur.readDelta1(); t.b = cur.readDelta4()+1; t.c = cur.readDelta3()+1; break;
            case 49: t.a += cur.readDelta1(); t.b = cur.readDelta4()+1; t.c = cur.readDelta4()+1; break;
            case 50: t.a += cur.readDelta2(); t.b = 1; t.c = 1; break;
            case 51: t.a += cur.readDelta2(); t.b = 1; t.c = cur.readDelta1()+1; break;
            case 52: t.a += cur.readDelta2(); t.b = 1; t.c = cur.readDelta2()+1; break;
            case 53: t.a += cur.readDelta2(); t.b = 1; t.c = cur.readDelta3()+1; break;
            case 54: t.a += cur.readDelta2(); t.b = 1; t.c = cur.readDelta4()+1; break;
            case 55: t.a += cur.readDelta2(); t.b = cur.readDelta1()+1; t.c = 1; break;
            case 56: t.a += cur.readDelta2(); t.b = cur.readDelta1()+1; t.c = cur.readDelta1()+1; break;
            case 57: t.a += cur.readDelta2(); t.b = cur.readDelta1()+1; t.c = cur.readDelta2()+1; break;
            case 58: t.a += cur.readDelta2(); t.b = cur.readDelta1()+1; t.c = cur.readDelta3()+1; break;
            case 59: t.a += cur.readDelta2(); t.b = cur.readDelta1()+1; t.c = cur.readDelta4()+1; break;
            case 60: t.a += cur.readDelta2(); t.b = cur.readDelta2()+1; t.c = 1; break;
            case 61: t.a += cur.readDelta2(); t.b = cur.readDelta2()+1; t.c = cur.readDelta1()+1; break;
            case 62: t.a += cur.readDelta2(); t.b = cur.readDelta2()+1; t.c = cur.readDelta2()+1; break;
            case 63: t.a += cur.readDelta2(); t.b = cur.readDelta2()+1; t.c = cur.readDelta3()+1; break;
            case 64: t.a += cur.readDelta2(); t.b = cur.readDelta2()+1; t.c = cur.readDelta4()+1; break;
            case 65: t.a += cur.readDelta2(); t.b = cur.readDelta3()+1; t.c = 1; break;
            case 66: t.a += cur.readDelta2(); t.b = cur.readDelta3()+1; t.c = cur.readDelta1()+1; break;
            case 67: t.a += cur.readDelta2(); t.b = cur.readDelta3()+1; t.c = cur.readDelta2()+1; break;
            case 68: t.a += cur.readDelta2(); t.b = cur.readDelta3()+1; t.c = cur.readDelta3()+1; break;
            case 69: t.a += cur.readDelta2(); t.b = cur.readDelta3()+1; t.c = cur.readDelta4()+1; break;
            case 70: t.a += cur.readDelta2(); t.b = cur.readDelta4()+1; t.c = 1; break;
            case 71: t.a += cur.readDelta2(); t.b = cur.readDelta4()+1; t.c = cur.readDelta1()+1; break;
            case 72: t.a += cur.readDelta2(); t.b = cur.readDelta4()+1; t.c = cur.readDelta2()+1; break;
            case 73: t.a += cur.readDelta2(); t.b = cur.readDelta4()+1; t.c = cur.readDelta3()+1; break;
            case 74: t.a += cur.readDelta2(); t.b = cur.readDelta4()+1; t.c = cur.readDelta4()+1; break;
            case 75: t.a += cur.readDelta3(); t.b = 1; t.c = 1; break;
            case 76: t.a += cur.readDelta3(); t.b = 1; t.c = cur.readDelta1()+1; break;
            case 77: t.a += cur.readDelta3(); t.b = 1; t.c = cur.readDelta2()+1; break;
            case 78: t.a += cur.readDelta3(); t.b = 1; t.c = cur.readDelta3()+1; break;
            case 79: t.a += cur.readDelta3(); t.b = 1; t.c = cur.readDelta4()+1; break;
            case 80: t.a += cur.readDelta3(); t.b = cur.readDelta1()+1; t.c = 1; break;
            case 81: t.a += cur.readDelta3(); t.b = cur.readDelta1()+1; t.c = cur.readDelta1()+1; break;
            case 82: t.a += cur.readDelta3(); t.b = cur.readDelta1()+1; t.c = cur.readDelta2()+1; break;
            case 83: t.a += cur.readDelta3(); t.b = cur.readDelta1()+1; t.c = cur.readDelta3()+1; break;
            case 84: t.a += cur.readDelta3(); t.b = cur.readDelta1()+1; t.c = cur.readDelta4()+1; break;
            case 85: t.a += cur.readDelta3(); t.b = cur.readDelta2()+1; t.c = 1; break;
            case 86: t.a += cur.readDelta3(); t.b = cur.readDelta2()+1; t.c = cur.readDelta1()+1; break;
            case 87: t.a += cur.readDelta3(); t.b = cur.readDelta2()+1; t.c = cur.readDelta2()+1; break;
            case 88: t.a += cur.readDelta3(); t.b = cur.readDelta2()+1; t.c = cur.readDelta3()+1; break;
            case 89: t.a += cur.readDelta3(); t.b = cur.readDelta2()+1; t.c = cur.readDelta4()+1; break;
            case 90: t.a += cur.readDelta3(); t.b = cur.readDelta3()+1; t.c = 1; break;
            case 91: t.a += cur.readDelta3(); t.b = cur.readDelta3()+1; t.c = cur.readDelta1()+1; break;
            case 92: t.a += cur.readDelta3(); t.b = cur.readDelta3()+1; t.c = cur.readDelta2()+1; break;
            case 93: t.a += cur.readDelta3(); t.b = cur.readDelta3()+1; t.c = cur.readDelta3()+1; break;
            case 94: t.a += cur.readDelta3(); t.b = cur.readDelta3()+1; t.c = cur.readDelta4()+1; break;
            case 95: t.a += cur.readDelta3(); t.b = cur.readDelta4()+1; t.c = 1; break;
            case 96: t.a += cur.readDelta3(); t.b = cur.readDelta4()+1; t.c = cur.readDelta1()+1; break;
            case 97: t.a += cur.readDelta3(); t.b = cur.readDelta4()+1; t.c = cur.readDelta2()+1; break;
            case 98: t.a += cur.readDelta3(); t.b = cur.readDelta4()+1; t.c = cur.readDelta3()+1; break;
            case 99: t.a += cur.readDelta3(); t.b = cur.readDelta4()+1; t.c = cur.readDelta4()+1; break;
            case 100: t.a += cur.readDelta4(); t.b = 1; t.c = 1; break;
            case 101: t.a += cur.readDelta4(); t.b = 1; t.c = cur.readDelta1()+1; break;
            case 102: t.a += cur.readDelta4(); t.b = 1; t.c = cur.readDelta2()+1; break;
            case 103: t.a += cur.readDelta4(); t.b = 1; t.c = cur.readDelta3()+1; break;
            case 104: t.a += cur.readDelta4(); t.b = 1; t.c = cur.readDelta4()+1; break;
            case 105: t.a += cur.readDelta4(); t.b = cur.readDelta1()+1; t.c = 1; break;
            case 106: t.a += cur.readDelta4(); t.b = cur.readDelta1()+1; t.c = cur.readDelta1()+1; break;
            case 107: t.a += cur.readDelta4(); t.b = cur.readDelta1()+1; t.c = cur.readDelta2()+1; break;
            case 108: t.a += cur.readDelta4(); t.b = cur.readDelta1()+1; t.c = cur.readDelta3()+1; break;
            case 109: t.a += cur.readDelta4(); t.b = cur.readDelta1()+1; t.c = cur.readDelta4()+1; break;
            case 110: t.a += cur.readDelta4(); t.b = cur.readDelta2()+1; t.c = 1; break;
            case 111: t.a += cur.readDelta4(); t.b = cur.readDelta2()+1; t.c = cur.readDelta1()+1; break;
            case 112: t.a += cur.readDelta4(); t.b = cur.readDelta2()+1; t.c = cur.readDelta2()+1; break;
            case 113: t.a += cur.readDelta4(); t.b = cur.readDelta2()+1; t.c = cur.readDelta3()+1; break;
            case 114: t.a += cur.readDelta4(); t.b = cur.readDelta2()+1; t.c = cur.readDelta4()+1; break;
            case 115: t.a += cur.readDelta4(); t.b = cur.readDelta3()+1; t.c = 1; break;
            case 116: t.a += cur.readDelta4(); t.b = cur.readDelta3()+1; t.c = cur.readDelta1()+1; break;
            case 117: t.a += cur.readDelta4(); t.b = cur.readDelta3()+1; t.c = cur.readDelta2()+1; break;
            case 118: t.a += cur.readDelta4(); t.b = cur.readDelta3()+1; t.c = cur.readDelta3()+1; break;
            case 119: t.a += cur.readDelta4(); t.b = cur.readDelta3()+1; t.c = cur.readDelta4()+1; break;
            case 120: t.a += cur.readDelta4(); t.b = cur.readDelta4()+1; t.c = 1; break;
            case 121: t.a += cur.readDelta4(); t.b = cur.readDelta4()+1; t.c = cur.readDelta1()+1; break;
            case 122: t.a += cur.readDelta4(); t.b = cur.readDelta4()+1; t.c = cur.readDelta2()+1; break;
            case 123: t.a += cur.readDelta4(); t.b = cur.readDelta4()+1; t.c = cur.readDelta3()+1; break;
            case 124: t.a += cur.readDelta4(); t.b = cur.readDelta4()+1; t.c = cur.readDelta4()+1; break;
            default: assert(false); // should not happen
            }
        }
        if(key.matches(t))
            (*end++) = t;
        else if(end != triples)
            break; // we are at the end of the range of interest
    }

    it = triples;
    if(it == end) {
        // no triples found
        nextPage = 0;
        return false;
    }

    return true;
}

}
