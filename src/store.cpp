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
#include "store.h"

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

String StringMapper::lookupString(String::id_t id) const {
    assert(String::validId(id));
    Cursor cur;
    // read map
    cur = map_ + 8 * (std::size_t)(id - 1);
    std::size_t offset = cur.readLong();
    // read string
    cur = strings_ + offset;
    String s(cur);
    assert(s.id() == id);
    return s;
}


const unsigned char Store::MAGIC[] = {0xd0, 0xd4, 0xc5, 0xd8,
                                      'C', 'a', 's', 't', 'o', 'r'};

Store::Store(const char* fileName, unsigned cacheCapacity) : db_(fileName) {
    Cursor cur = db_.page(0);

    // check magic number and version format
    if(memcmp(cur.get(), MAGIC, sizeof(MAGIC)) != 0)
        throw CastorException() << "Invalid magic number";
    cur += sizeof(MAGIC);
    if(cur.readInt() != VERSION)
        throw CastorException() << "Invalid format version";

    // Get triples count
    triplesCount_ = cur.readInt();

    // Get raw triples table
    triplesTable_ = cur.readInt();

    // Get triples pointers
    for(int i = 0; i < TRIPLE_ORDERS; i++) {
        triples_[i].begin = cur.readInt();
        triples_[i].end   = cur.readInt();
        triples_[i].index = new BTree<Triple>(&db_, cur.readInt());
        triples_[i].aggregated = new BTree<AggregatedTriple>(&db_, cur.readInt());
    }

    // Get fully aggregated triples pointers
    for(int i = 0; i < Triple::COMPONENTS; i++)
        fullyAggregated_[i] = new BTree<FullyAggregatedTriple>(&db_, cur.readInt());

    // Get strings pointers
    strings_.count = cur.readInt();
    strings_.begin = cur.readInt();
    strings_.map = cur.readInt();
    strings_.index = new HashTree<8>(&db_, cur.readInt());
    StringMapper::strings_ = db_.page(strings_.begin);
    StringMapper::map_ = db_.page(strings_.map);

    // Get values pointers
    values_.begin = cur.readInt();
    values_.index = new HashTree<4>(&db_, cur.readInt());
    values_.eqClasses = cur.readInt();
    for(Value::Category cat = Value::CAT_BLANK; cat <= Value::CATEGORIES; ++cat)
        values_.categories[cat] = cur.readInt();
    values_.count = values_.categories[Value::CATEGORIES] - 1;

    // initialize triples cache
    cache_.initialize(&db_, values_.begin - 1, cacheCapacity);
}

Store::~Store() {
    for(int i = 0; i < TRIPLE_ORDERS; i++) {
        delete triples_[i].index;
        delete triples_[i].aggregated;
    }
    for(int i = 0; i < Triple::COMPONENTS; i++)
        delete fullyAggregated_[i];
    delete strings_.index;
    delete values_.index;
    for(cp::RDFVar* x : varcache_)
        delete x;
}

Value Store::lookupValue(Value::id_t id) const {
    assert(id > 0 && id <= valuesCount());
    Cursor cur = db_.page(values_.begin) + (id - 1) * Value::SERIALIZED_SIZE;
    return Value(cur);
}

void Store::resolve(String& str) const {
    if(str.resolved())
        return;
    assert(str.direct());

    // look for pages containing the hash
    Hash::hash_t hash = str.hash();
    Cursor cur = strings_.index->lookup(hash);
    if(!cur.valid()) {
        str.id(0);
        return;
    }

    // scan all candidates in the collision list
    Cursor end = db_.pageEnd(cur);
    while(cur != end) {
        if(cur.readInt() != hash)
            break;
        std::size_t offset = cur.readLong();
        Cursor stringCur = db_.page(strings_.begin) + offset;
        String s(stringCur);
        if(s == str) {
            str.id(s.id());
            return;
        }
    }
    str.id(0);
}

void Store::resolve(Value& val) const {
    if(val.id() != Value::UNKNOWN_ID)
        return;

    // lookup strings
    val.ensureLexical();
    if(val.isTyped() && val.datatypeLex().null()) {
        Value v = lookupValue(val.datatypeId());
        val.datatypeLex(String(v.lexical()));
    }
    val.ensureDirectStrings(*this);
    val.ensureResolvedStrings(*this);

    // look for pages containing the hash
    Hash::hash_t hash = val.hash();
    Cursor cur = values_.index->lookup(hash);
    if(!cur.valid()) {
        val.id(0);
        return;
    }

    // scan all candidates in the collision list
    Cursor end = db_.pageEnd(cur);
    while(cur != end) {
        if(cur.readInt() != hash)
            break;
        Value::id_t id = cur.readInt();
        Value v = lookupValue(id);
        if(v == val) {
            val.id(id);
            return;
        }
    }
    val.id(0);
}


ValueRange Store::eqClass(Value::id_t id) const {
    assert(id > 0);
    if(id < values_.categories[Value::CAT_BOOLEAN] ||
       id >= values_.categories[Value::CAT_OTHER]) {
        /* Categories BLANK, IRI, SIMPLE_LITERAL, TYPED_LITERAL and OTHER
         * are always compared on lexical value. Their equivalence class is
         * thus always a singleton.
         */
        ValueRange result = {id, id};
        return result;
    }

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

ValueRange Store::eqClass(const Value& val) const {
    if(val.id() > 0)
        return eqClass(val.id());
    assert(val.interpreted());

    // costly binary search for equivalent value
    Value::id_t left  = 1;
    Value::id_t right = values_.count + 1;
    while(left != right) {
        Value::id_t middle = left + (right - left) / 2;
        Value mVal = lookupValue(middle);
        mVal.ensureInterpreted(*this);
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

Value::Category Store::category(Value::id_t id) const {
    for(Value::Category cat = Value::CAT_BLANK; cat <= Value::CATEGORIES; ++cat) {
        if(values_.categories[cat] > id)
            return --cat;
    }
    // should not happen
    assert(false);
    return static_cast<Value::Category>(Value::CATEGORIES);
}

cp::RDFVar* Store::variable(cp::Solver *solver) {
    cp::RDFVar* x;
    if(varcache_.empty()) {
        x = new cp::RDFVar(solver, 0, valuesCount());
    } else {
        x = varcache_.back();
        varcache_.pop_back();
        x->reset(solver);
    }
    return x;
}

void Store::release(cp::RDFVar *x) {
    varcache_.push_back(x);
}

unsigned Store::triplesCount(Triple pattern) {
    unsigned count = 0;
    switch((pattern[0] == 0) + (pattern[1] == 0) + (pattern[2] == 0)) {
    case 0:
    {
        TripleRange q(this, pattern, pattern);
        if(q.next(nullptr))
            count = 1;
        break;
    }
    case 1:
    {
        TripleOrder order;
        if     (pattern[0] == 0) order = TripleOrder::POS;
        else if(pattern[1] == 0) order = TripleOrder::OSP;
        else                     order = TripleOrder::SPO;
        AggregatedTriple key = pattern.toOrdered(order);
        unsigned page = triples_[static_cast<int>(order)].aggregated->lookupLeaf(key);
        if(page != 0) {
            const TripleCache::Line* line = cache_.fetch<AggregatedTriple>(page);
            const AggregatedTriple* t = line->findLower(key);
            if(t != line->end<AggregatedTriple>() && !(key < *t))
                count = t->count();
            cache_.release(line);
        }
        break;
    }
    case 2:
    {
        int index;
        TripleOrder order;
        if     (pattern[0] != 0) { index = 0; order = TripleOrder::SPO; }
        else if(pattern[1] != 0) { index = 1; order = TripleOrder::POS; }
        else                     { index = 2; order = TripleOrder::OSP; }
        FullyAggregatedTriple key = pattern.toOrdered(order);
        unsigned page = fullyAggregated_[index]->lookupLeaf(key);
        if(page != 0) {
            const TripleCache::Line* line = cache_.fetch<FullyAggregatedTriple>(page);
            const FullyAggregatedTriple* t = line->findLower(key);
            if(t != line->end<FullyAggregatedTriple>() && !(key < *t))
                count = t->count();
            cache_.release(line);
        }
        break;
    }
    case 3:
        count = triplesCount_;
        break;
    default:
        assert(false); // should not happen
    }
    return count;
}

Store::TripleRange::TripleRange(Store* store, Triple from, Triple to,
                                TripleOrder order) :
        store_(store) {
    if(order == TRIPLE_ORDER_AUTO) {
        /* determine index such that non-singleton ranges are the last
         * components
         */
        switch((from[0] != to[0]) |
               (from[1] != to[1]) << 1 |
               (from[2] != to[2]) << 2) {
        case 0: // (s,p,o)
        case 1: // (*,p,o)
        case 5: // (*,p,*)
        case 7: // (*,*,*)
            order = TripleOrder::POS;
            break;
        case 4: // (s,p,*)
        case 6: // (s,*,*)
            order = TripleOrder::SPO;
            break;
        case 2: // (s,*,o)
        case 3: // (*,*,o)
            order = TripleOrder::OSP;
            break;
        }
    }

    Triple key;

    order_ = order;
    key    = from.toOrdered(order);
    limit_ = to.toOrdered(order);
    direction_ = to < from ? -1 : +1;

    // look for the first leaf
    nextPage_ = store->triples_[static_cast<int>(order_)].index->lookupLeaf(key);
    if(nextPage_ == 0) {
        line_ = nullptr;
        it_ = end_ = nullptr;
        return;
    }

    if(direction_ < 0) {
        /* We are searching backwards. The leaf we just found is the first
         * containing keys >= the upper bound ("from"). If all keys are >= from,
         * i.e., the first key >= from, the leaf has no interesting triple and
         * we need to get the previous one, if possible.
         */
        bool first;
        bool last;
        Triple k;
        store->cache_.peek(nextPage_, first, last, k);
        if(key < k) {
            if(!first) {
                /* We are in such a case, just fetch the previous page and
                 * start iterating from the last triple.
                 */
                nextPage_--;
                line_ = store->cache_.fetch(nextPage_);
                nextPage_ = line_->first ? 0 : nextPage_ - 1;
                it_       = line_->end() - 1;
                end_      = line_->begin() - 1;
            } else {
                line_ = nullptr;
                it_ = end_ = nullptr;
                nextPage_  = 0;
            }
            return;
        }
    }

    // lookup page in cache
    line_ = store->cache_.fetch(nextPage_);
    if(direction_ > 0) {
        nextPage_ = line_->last ? 0 : nextPage_ + 1;
        it_       = line_->findLower(key);
        end_      = line_->end();
    } else {
        nextPage_ = line_->first ? 0 : nextPage_ - 1;
        it_       = line_->findUpper(key) - 1;
        end_      = line_->begin() - 1;
    }

    if(it_ == end_) {
        // unsuccessful search
        store->cache_.release(line_);
        line_ = nullptr;
        nextPage_ = 0;
    }
}

Store::TripleRange::~TripleRange() {
    if(line_ != nullptr)
        store_->cache_.release(line_);
}

bool Store::TripleRange::next(Triple* t) {
    if(it_ == end_) {
        if(line_ != nullptr) {
            store_->cache_.release(line_);
            line_ = nullptr;
        }
        if(nextPage_ == 0)
            return false;
        line_ = store_->cache_.fetch(nextPage_);
        if(direction_ > 0) {
            nextPage_ = line_->last ? 0 : nextPage_ + 1;
            it_       = line_->begin();
            end_      = line_->end();
        } else {
            nextPage_ = line_->first ? 0 : nextPage_ - 1;
            it_       = line_->end() - 1;
            end_      = line_->begin() - 1;
        }
    }
    if((direction_ > 0 && limit_ < *it_) ||
       (direction_ < 0 && *it_ < limit_))
        return false;
    if(t != nullptr)
        *t = it_->toSPO(order_);
    it_ += direction_;
    return true;
}

}
