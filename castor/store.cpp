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
#include <strings.h>
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
    for(int i = 0; i < 3; i++) {
        triplesStart[i] = cur.readInt();
        triplesIndex[i] = new BTree<Triple>(&db, cur.readInt());
    }

    // Get values pointers
    valuesStart = cur.readInt();
    valuesMapping = cur.readInt();
    valuesIndex = new ValueHashTree(&db, cur.readInt());
    valuesEqClasses = cur.readInt();
    for(Value::Class cls = Value::CLASS_BLANK; cls <= Value::CLASSES_COUNT; ++cls)
        valuesClassStart[cls] = cur.readInt();
    nbValues = valuesClassStart[Value::CLASSES_COUNT] - 1;

    // initialize triples cache
    cache.initialize(&db, valuesStart - 1);
}

Store::~Store() {
    for(int i = 0; i < 3; i++)
        delete triplesIndex[i];
    delete valuesIndex;
}

void Store::fetchValue(Value::id_t id, Value &val) {
    assert(id > 0);
    // read mapping
    const unsigned EPP = PageReader::PAGE_SIZE / 8; // entries per page in map
    id--;
    Cursor cur = db.getPage(valuesMapping + id / EPP) + 8*(id % EPP);
    unsigned page = cur.readInt();
    unsigned offset = cur.readInt();

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


ValueRange Store::getValueEqClass(Value::id_t id) {
    assert(id > 0);
    id--;  // page offsets start with 0
    Cursor cur = db.getPage(valuesEqClasses);
    ValueRange result;
    unsigned idOffset = id / 32;
    unsigned idWord = cur.peekInt(idOffset * 4);
    unsigned word, offset, bit;

    // search for boundary start
    offset = idOffset;
    word = idWord;
    bit = id % 32;
    if(word & (1 << bit)) {
        result.from = id + 1;
    } else {
        word &= (1 << bit) - 1; // this is the portion before the bit
        while(!word) { // find word with at least one bit set
            offset--;
            word = cur.peekInt(offset * 4);
        }
        bit = fls(word);
        result.from = offset * 32 + bit + 1;
    }

    // search for boundary end
    id++;
    offset = id / 32;
    bit = id % 32;
    word = (bit == 0 ? cur.peekInt(offset * 4) : idWord);
    if(word & (1 << bit)) {
        result.to = id;
    } else {
        word &= ~(((1 << bit) - 1) | (1 << bit)); // this is the portion after the bit
        while(!word) { // find word with at least one bit set
            offset++;
            word = cur.peekInt(offset * 4);
        }
        bit = ffs(word) - 1;
        result.to = offset * 32 + bit;
    }

    return result;
}

ValueRange Store::getValueEqClass(const Value &val) {
    if(val.id > 0)
        return getValueEqClass(val.id);
    assert(val.isInterpreted);

    // costly binary search for equivalent value
    Value::id_t left = 1, right = nbValues + 1;
    while(left != right) {
        Value::id_t middle = left + (right - left) / 2;
        Value mVal;
        fetchValue(middle, mVal);
        mVal.ensureInterpreted();
        if(mVal.compare(val) == 0)
            return getValueEqClass(middle);
        if(mVal < val)
            left = middle + 1;
        else
            right = middle;
    }
    ValueRange result = {left, left - 1};
    return result;
}

Value::Class Store::getValueClass(Value::id_t id) {
    for(Value::Class cls = Value::CLASS_BLANK; cls <= Value::CLASSES_COUNT; ++cls) {
        if(valuesClassStart[cls] > id)
            return --cls;
    }
    // should not happen
    assert(false);
    return Value::CLASSES_COUNT;
}

Store::RangeQuery::RangeQuery(Store *store, Triple from, Triple to) :
        store(store) {
    assert(from <= to);
    // determine index such that non-singleton ranges are the last components
    Triple key;
    switch((from[0] != to[0]) |
           (from[1] != to[1]) << 1 |
           (from[2] != to[2]) << 2) {
    case 0: // (s,p,o)
    case 4: // (s,p,*)
    case 6: // (s,*,*)
    case 7: // (*,*,*)
        order = SPO;
        key = from;
        limit = to;
        break;
    case 1: // (*,p,o)
    case 5: // (*,p,*)
        order = POS;
        key   = from << 1;
        limit = to   << 1;
        break;
    case 2: // (s,*,o)
    case 3: // (*,*,o)
        order = OSP;
        key   = from << 2;
        limit = to   << 2;
        break;
    }

    // look for the first leaf
    nextPage = store->triplesIndex[order]->lookupLeaf(key);
    if(nextPage == 0) {
        it = end = NULL;
        return;
    }

    // lookup page in cache
    const TripleCache::Line *line = store->cache.fetch(nextPage);
    it = line->triples;
    end = it + line->count;
    nextPage = line->nextPage;

    // binary search for first triple
    const Triple *left = it, *right = end;
    while(left != right) {
        const Triple *middle = left + (right - left) / 2;
        if(*middle < key) {
            left = middle + 1;
        } else if(middle == it || *(middle - 1) < key) {
            it = middle;
            break;
        } else {
            right = middle;
        }
    }
    if(left == right) {
        // unsuccessful search
        it = end;
        nextPage = 0;
    }
}

bool Store::RangeQuery::next(Triple *t) {
    if(it == end) {
        if(nextPage == 0)
            return false;
        const TripleCache::Line *line = store->cache.fetch(nextPage);
        it = line->triples;
        end = it + line->count;
        nextPage = line->nextPage;
    }
    if(*it > limit)
        return false;
    if(t != NULL) {
        switch(order) {
        case SPO:
            *t = *it;
            break;
        case POS:
            *t = *it >> 1;
            break;
        case OSP:
            *t = *it >> 2;
            break;
        }
    }
    it++;
    return true;
}

}
