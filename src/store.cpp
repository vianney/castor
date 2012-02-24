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
#include "store.h"

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

#include "util.h"

namespace castor {

const char Store::MAGIC[] = {'\xd0', '\xd4', '\xc5', '\xd8',
                             'C', 'a', 's', 't', 'o', 'r'};

Store::Store(const char* fileName) : db_(fileName) {
    Cursor cur = db_.page(0);

    // check magic number and version format
    if(memcmp(cur.get(), MAGIC, sizeof(MAGIC)) != 0)
        throw CastorException() << "Invalid magic number";
    cur += sizeof(MAGIC);
    if(cur.readInt() != VERSION)
        throw CastorException() << "Invalid format version";


    // Get triples pointers
    for(int i = 0; i < ORDERS; i++) {
        triples_[i].begin = cur.readInt();
        triples_[i].end   = cur.readInt();
        triples_[i].index = new BTree<Triple>(&db_, cur.readInt());
    }

    // Get values pointers
    values_.begin = cur.readInt();
    values_.mapping = cur.readInt();
    values_.index = new ValueHashTree(&db_, cur.readInt());
    values_.eqClasses = cur.readInt();
    for(Value::Category cat = Value::CAT_BLANK; cat <= Value::CATEGORIES; ++cat)
        values_.categories[cat] = cur.readInt();
    values_.count = values_.categories[Value::CATEGORIES] - 1;

    // initialize triples cache
    cache_.initialize(&db_, values_.begin - 1);
}

Store::~Store() {
    for(int i = 0; i < ORDERS; i++)
        delete triples_[i].index;
    delete values_.index;
}

void Store::fetch(Value::id_t id, Value& val) {
    assert(id > 0);
    // read mapping
    const unsigned EPP = PageReader::PAGE_SIZE / 8; // entries per page in map
    id--;
    Cursor cur = db_.page(values_.mapping + id / EPP) + 8*(id % EPP);
    unsigned page = cur.readInt();
    unsigned offset = cur.readInt();

    // read value
    cur = db_.page(page) + offset;
    cur.readValue(val);
}

void Store::lookup(Value& val) {
    if(val.id > 0)
        return;

    // look for pages containing the hash
    val.ensureLexical();
    Hash::hash_t hash = val.hash();
    Cursor listCur = values_.index->lookup(hash);
    if(!listCur.valid())
        return;

    // scan all candidates in the collision list
    Cursor listEnd = db_.pageEnd(listCur);
    while(listCur != listEnd) {
        if(listCur.readInt() != hash)
            break;

        // scan page
        Cursor cur = db_.page(listCur.readInt());
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


ValueRange Store::eqClass(Value::id_t id) {
    assert(id > 0);
    id--;  // page offsets start with 0
    Cursor cur = db_.page(values_.eqClasses);
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

ValueRange Store::eqClass(const Value& val) {
    if(val.id > 0)
        return eqClass(val.id);
    assert(val.isInterpreted);

    // costly binary search for equivalent value
    Value::id_t left  = 1;
    Value::id_t right = values_.count + 1;
    while(left != right) {
        Value::id_t middle = left + (right - left) / 2;
        Value mVal;
        fetch(middle, mVal);
        mVal.ensureInterpreted();
        if(mVal.compare(val) == 0)
            return eqClass(middle);
        if(mVal < val)
            left = middle + 1;
        else
            right = middle;
    }
    ValueRange result = {left, left - 1};
    return result;
}

Value::Category Store::category(Value::id_t id) {
    for(Value::Category cat = Value::CAT_BLANK; cat <= Value::CATEGORIES; ++cat) {
        if(values_.categories[cat] > id)
            return --cat;
    }
    // should not happen
    assert(false);
    return static_cast<Value::Category>(Value::CATEGORIES);
}

Store::TripleRange::TripleRange(Store* store, Triple from, Triple to,
                                Store::TripleOrder order) :
        store_(store) {
    if(order == Store::AUTO) {
        /* determine index such that non-singleton ranges are the last
         * components
         */
        switch((from[0] != to[0]) |
               (from[1] != to[1]) << 1 |
               (from[2] != to[2]) << 2) {
        case 0: // (s,p,o)
        case 4: // (s,p,*)
        case 6: // (s,*,*)
        case 7: // (*,*,*)
            order = SPO;
            break;
        case 1: // (*,p,o)
        case 5: // (*,p,*)
            order = POS;
            break;
        case 2: // (s,*,o)
        case 3: // (*,*,o)
            order = OSP;
            break;
        }
    }

    Triple key;

    order_ = order;
    switch(order) {
    case SPO:
        key    = from;
        limit_ = to;
        break;
    case SOP:
        key    = from.reorder<0,2,1>();
        limit_ = to  .reorder<0,2,1>();
        break;
    case PSO:
        key    = from.reorder<1,0,2>();
        limit_ = to  .reorder<1,0,2>();
        break;
    case POS:
        key    = from.reorder<1,2,0>();
        limit_ = to  .reorder<1,2,0>();
        break;
    case OSP:
        key    = from.reorder<2,0,1>();
        limit_ = to  .reorder<2,0,1>();
        break;
    case OPS:
        key    = from.reorder<2,1,0>();
        limit_ = to  .reorder<2,1,0>();
        break;
    }

    direction_ = from <= to ? +1 : -1;

    // look for the first leaf
    nextPage_ = store->triples_[order_].index->lookupLeaf(key);
    if(nextPage_ == 0) {
        it_ = end_ = nullptr;
        return;
    }

    if(direction_ < 0) {
        /* We are searching backwards. The leaf we just found is the first
         * containing keys >= the upper bound ("from"). If all keys are >= from,
         * i.e., the first key >= from, the leaf has no interesting triple and
         * we need to get the previous one, if possible.
         */
        unsigned p;
        unsigned n;
        Triple k;
        store->cache_.peek(nextPage_, p, n, k);
        if(key < k) {
            if(p != 0) {
                /* We are in such a case, just fetch the previous page and
                 * start iterating from the last triple.
                 */
                nextPage_ = p;
                const TripleCache::Line* line = store->cache_.fetch(nextPage_);
                end_      = line->triples;
                it_       = end_ + (line->count - 1);
                nextPage_ = line->prevPage;
            } else {
                it_ = end_ = nullptr;
                nextPage_  = 0;
            }
            return;
        }
    }

    // lookup page in cache
    const TripleCache::Line* line = store->cache_.fetch(nextPage_);
    if(direction_ > 0) {
        it_       = line->triples;
        end_      = it_ + line->count;
        nextPage_ = line->nextPage;

        // binary search for first triple
        const Triple* left  = it_;
        const Triple* right = end_;
        while(left != right) {
            const Triple* middle = left + (right - left) / 2;
            if(*middle < key) {
                left = middle + 1;
            } else if(middle == it_ || *(middle - 1) < key) {
                // found!
                it_ = middle;
                return;
            } else {
                right = middle;
            }
        }
    } else {
        end_      = line->triples - 1;
        it_       = end_ + line->count;
        nextPage_ = line->prevPage;

        // binary search for last triple
        const Triple* left  = end_ + 1;
        const Triple* right = it_ + 1;
        while(left != right) {
            const Triple* middle = left + (right - left) / 2;
            if(key < *middle) {
                right = middle;
            } else if(middle == it_ || key < *(middle + 1)) {
                // found!
                it_ = middle;
                return;
            } else {
                left = middle + 1;
            }
        }
    }

    // unsuccessful search
    it_ = end_;
    nextPage_ = 0;
}

bool Store::TripleRange::next(Triple* t) {
    if(it_ == end_) {
        if(nextPage_ == 0)
            return false;
        const TripleCache::Line* line = store_->cache_.fetch(nextPage_);
        if(direction_ > 0) {
            it_       = line->triples;
            end_      = it_ + line->count;
            nextPage_ = line->nextPage;
        } else {
            end_      = line->triples - 1;
            it_       = end_ + line->count;
            nextPage_ = line->prevPage;
        }
    }
    if((direction_ > 0 && *it_ > limit_) ||
       (direction_ < 0 && *it_ < limit_))
        return false;
    if(t != nullptr) {
        switch(order_) {
        case SPO: *t = *it_;                  break;
        case SOP: *t = it_->reorder<0,2,1>(); break;
        case PSO: *t = it_->reorder<1,0,2>(); break;
        case POS: *t = it_->reorder<2,0,1>(); break;
        case OSP: *t = it_->reorder<1,2,0>(); break;
        case OPS: *t = it_->reorder<2,1,0>(); break;
        }
    }
    it_ += direction_;
    return true;
}

}
