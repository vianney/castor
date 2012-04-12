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
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cstdint>
#include <cassert>
#include <unistd.h>
#include <sys/stat.h>


#include "librdfwrapper.h"
#include "model.h"
#include "util.h"
#include "store.h"
#include "store/readutils.h"

#include "tempfile.h"
#include "sort.h"
#include "valuelookup.h"
#include "pagewriter.h"
#include "btreebuilder.h"

using namespace std;
using namespace castor;

////////////////////////////////////////////////////////////////////////////////
// RDF Parsing

class RDFLoader : public librdf::RdfParseHandler {
public:
    RDFLoader(TempFile* rawTriples, TempFile* rawValues)
            : triples(rawTriples), values(rawValues) {}

    void parseTriple(raptor_statement* triple) {
        Value subject  (triple->subject);
        Value predicate(triple->predicate);
        Value object   (triple->object);

        uint64_t subjectId   = values.lookup(subject);
        uint64_t predicateId = values.lookup(predicate);
        uint64_t objectId    = values.lookup(object);

        triples->writeBigInt(subjectId);
        triples->writeBigInt(predicateId);
        triples->writeBigInt(objectId);
    }

private:
    TempFile*   triples;
    ValueLookup values;
};

////////////////////////////////////////////////////////////////////////////////
// Dictionary building

/**
 * Skip a (Value, int) pair
 */
static void skipValueInt(Cursor& cur) {
    cur.skipValue();
    cur.skipBigInt();
}

/**
 * Skip a (int, int) pair
 */
static void skipIntInt(Cursor& cur) {
    cur.skipBigInt();
    cur.skipBigInt();
}

/**
 * Compare function for values using SPARQL order
 */
static int compareValue(Cursor a, Cursor b) {
    Value va;  a.readValue(va);  va.ensureInterpreted();
    Value vb;  b.readValue(vb);  vb.ensureInterpreted();
    if(va == vb) {
        return 0;
    } else if(va < vb) {
        return -1;
    } else {
        return 1;
    }
}

/**
 * Compare two integers
 */
static inline int cmpInt(uint64_t a, uint64_t b) {
    return a < b ? -1 : (a > b ? 1 : 0);
}

/**
 * Compare function for integers
 */
static int compareBigInt(Cursor a, Cursor b) {
    return cmpInt(a.readBigInt(), b.readBigInt());
}

/**
 * Build the dictionary
 *
 * @param rawValues early (value, id) mapping. Will be discarded.
 * @param values will contain the sorted values
 * @param valueIdMap will contain the (old id, new id) mapping ordered by
 *                   old id
 * @param valueEqClasses will contain the equivalence classes boundaries
 * @param categories array that will contain the start ids for each category
 *                   (including virtual last class)
 */
static void buildDictionary(TempFile& rawValues, TempFile& values,
                            TempFile& valueIdMap, TempFile& valueEqClasses,
                            Value::id_t* categories) {
    // sort values using SPARQL order
    TempFile sortedValues(rawValues.baseName());
    FileSorter::sort(rawValues, sortedValues, skipValueInt, compareValue);
    rawValues.discard();

    for(Value::Category cat = Value::CAT_BLANK; cat <= Value::CATEGORIES; ++cat)
        categories[cat] = 0;

    // construct values list without duplicates and remember mappings
    TempFile rawMap(rawValues.baseName());
    {
        MMapFile in(sortedValues.fileName().c_str());
        Value last;
        unsigned eqBuf = 0, eqShift = 0;
        for(Cursor cur = in.begin(), end = in.end(); cur != end;) {
            Value val;
            cur.readValue(val);
            uint64_t id = cur.readBigInt();
            val.ensureInterpreted();
            if(last.id == 0 || last != val) {
                val.id = last.id + 1;
                values.writeValue(val);
                eqBuf |= (last.id != 0 && last.compare(val) == 0 ? 0 : 1) << (eqShift++);
                if(eqShift == 32) {
                    valueEqClasses.writeInt(eqBuf);
                    eqBuf = 0;
                    eqShift = 0;
                }
                if(last.id == 0 || last.category() != val.category())
                    categories[val.category()] = val.id;
                last = std::move(val);
            }
            rawMap.writeBigInt(id);
            rawMap.writeBigInt(last.id);
        }
        // terminate equivalence classes boundaries
        eqBuf |= 1 << eqShift;
        valueEqClasses.writeInt(eqBuf);
        // terminate class starts
        categories[Value::CATEGORIES] = last.id + 1;
        for(Value::Category cat = static_cast<Value::Category>(Value::CATEGORIES);
            cat >= Value::CAT_BLANK; --cat) {
            if(categories[cat] == 0)
                categories[cat] = categories[cat + 1];
        }
    }
    rawMap.close();
    sortedValues.discard();

    // sort the id map
    FileSorter::sort(rawMap, valueIdMap, skipIntInt, compareBigInt);
    rawMap.discard();
}

////////////////////////////////////////////////////////////////////////////////
// ID resolving

/**
 * Skip a (int, int, int) triple
 */
static void skipTriple(Cursor& cur) {
    cur.skipBigInt();
    cur.skipBigInt();
    cur.skipBigInt();
}

/**
 * Compare function for triples using the specified component order
 */
template <int C1=0, int C2=1, int C3=2>
static int compareTriple(Cursor a, Cursor b) {
    BasicTriple<uint64_t> ta, tb;
    for(int i = 0; i < ta.COMPONENTS; i++)
        ta[i] = a.readBigInt();
    for(int i = 0; i < tb.COMPONENTS; i++)
        tb[i] = b.readBigInt();
    int c = cmpInt(ta[C1], tb[C1]);
    if(c)
        return c;
    c = cmpInt(ta[C2], tb[C2]);
    if(c)
        return c;
    return cmpInt(ta[C3], tb[C3]);
}

/**
 * Rewrite triples, resolving the first component with the new ids
 *
 * @param in the triples with old ids for the first component. Will be discarded.
 * @param out will contain the triples with the new ids and components shifted
 * @param map file with (old id, new id) mappings
 */
static void resolveIdsComponent(TempFile& in, TempFile& out, MMapFile& map) {
    // sort by first component
    TempFile sorted(in.baseName());
    FileSorter::sort(in, sorted, skipTriple, compareBigInt);
    in.discard();

    // resolve first component and shift components
    {
        MMapFile f(sorted.fileName().c_str());
        uint64_t from = 0;
        uint64_t to   = 0;
        Cursor mapCursor = map.begin();
        for(Cursor cur = f.begin(), end = f.end(); cur != end;) {
            BasicTriple<uint64_t> t;
            for(int i = 0; i < t.COMPONENTS; i++)
                t[i] = cur.readBigInt();
            while(from < t[0]) {
                from = mapCursor.readBigInt();
                to   = mapCursor.readBigInt();
            }
            for(int i = 1; i < t.COMPONENTS; i++)
                out.writeBigInt(t[i]);
            out.writeBigInt(to);
        }
    }
    sorted.discard();
}

/**
 * Rewrite triples using the new ids.
 *
 * @param rawTriples the triples with old ids. Will be discarded.
 * @param triples will contain the triples with the new ids
 * @param idMap file with (old id, new id) mappings
 */
static void resolveIds(TempFile& rawTriples, TempFile& triples, TempFile& idMap) {
    MMapFile map(idMap.fileName().c_str());

    // resolve subjects
    TempFile subjectResolved(rawTriples.baseName());
    resolveIdsComponent(rawTriples, subjectResolved, map);

    // resolve predicates
    TempFile predicateResolved(rawTriples.baseName());
    resolveIdsComponent(subjectResolved, predicateResolved, map);

    // resolve objects
    TempFile objectResolved(rawTriples.baseName());
    resolveIdsComponent(predicateResolved, objectResolved, map);

    // final sort, removing duplicates
    FileSorter::sort(objectResolved, triples, skipTriple, compareTriple, true);
}

////////////////////////////////////////////////////////////////////////////////
// Common definitions for store creation

struct StoreBuilder {
    PageWriter w; //!< store output

    unsigned triplesCount; //!< number of triples

    /**
     * Triple indexes (in various orderings)
     */
    struct {
        unsigned begin; //!< first page of the table
        unsigned end;   //!< last page of the table
        unsigned index; //!< root node of the B+-tree
        unsigned aggregated; //!< root node of the B+-tree for aggregated triples
    } triples[TRIPLE_ORDERS];

    /**
     * Root nodes of the B+-trees of the fully aggregated triples
     */
    unsigned fullyAggregated[Triple::COMPONENTS];

    /**
     * Values
     */
    struct {
        unsigned begin;     //!< first page of table
        unsigned mapping;   //!< first page of mapping
        unsigned index;     //!< index (hash->page mapping)
        unsigned eqClasses; //!< first page of equivalence classes boundaries

        //! first id of each category
        Value::id_t categories[Value::CATEGORIES + 1];
    } values;

    StoreBuilder(const char* fileName) : w(fileName) {}
};

////////////////////////////////////////////////////////////////////////////////
// Storing triples

struct WriteTriple : public Triple {
    WriteTriple() {}
    WriteTriple(const BasicTriple<Value::id_t>& o) : Triple(o) {}

    /**
     * Construct a triple with value v for each component.
     */
    WriteTriple(Value::id_t v) {
        for(int i = 0; i < COMPONENTS; i++)
            c[i] = v;
    }

    WriteTriple(const WriteTriple&) = default;
    WriteTriple& operator=(const WriteTriple&) = default;

    void write(PageWriter& writer) const {
        for(int i = 0; i < COMPONENTS; i++)
            writer.writeInt(c[i]);
    }
};

/**
 * Store full triples of a particular order.
 *
 * @param b store builder
 * @param triples file with triples
 * @param order order defined by the template parameters
 */
template<int C1, int C2, int C3>
static void storeFullTriples(StoreBuilder& b, TempFile& triples,
                             TripleOrder order) {
    b.triples[static_cast<int>(order)].begin = b.w.page();

    unsigned count = 0;
    BTreeBuilder<WriteTriple> tb(&b.w);
    // Construct leaves
    {
        WriteTriple last(0);
        MMapFile in(triples.fileName().c_str());
        for(Cursor cur = in.begin(), end = in.end(); cur != end;) {
            // Read triple and reorder
            WriteTriple t;
            for(int i = 0; i < t.COMPONENTS; i++)
                t[i] = static_cast<Value::id_t>(cur.readBigInt());
            t = t.reorder<C1,C2,C3>();

            // Compute encoded length
            unsigned len;
            if(t[0] == last[0]) {
                if(t[1] == last[1]) {
                    assert(t[2] != last[2]); // there should not be any duplicate anymore
                    if(t[2] - last[2] < 128)
                        len = 1;
                    else
                        len = 1 + PageWriter::lenDelta(t[2] - last[2] - 128);
                } else {
                    len = 1 + PageWriter::lenDelta(t[1] - last[1])
                            + PageWriter::lenDelta(t[2] - 1);
                }
            } else {
                len = 1 + PageWriter::lenDelta(t[0] - last[0])
                        + PageWriter::lenDelta(t[1] - 1)
                        + PageWriter::lenDelta(t[2] - 1);
            }

            // Should we start a new leaf? (first element or no more room)
            if(last[0] == 0 || len > b.w.remaining()) {
                if(last[0] != 0)
                    tb.endLeaf(last);
                tb.beginLeaf();
                // Write the first element of a page fully
                t.write(b.w);
            } else {
                // Otherwise, pack the triple
                if(t[0] == last[0]) {
                    if(t[1] == last[1]) {
                        if(t[2] - last[2] < 128) {
                            b.w.writeByte(t[2] - last[2]);
                        } else {
                            unsigned delta = t[2] - last[2] - 128;
                            b.w.writeByte(0x80 + PageWriter::lenDelta(delta));
                            b.w.writeDelta(delta);
                        }
                    } else {
                        unsigned delta = t[1] - last[1];
                        b.w.writeByte(0x80 + PageWriter::lenDelta(delta) * 5
                                           + PageWriter::lenDelta(t[2] - 1));
                        b.w.writeDelta(delta);
                        b.w.writeDelta(t[2] - 1);
                    }
                } else {
                    unsigned delta = t[0] - last[0];
                    b.w.writeByte(0x80 + PageWriter::lenDelta(delta) * 25
                                       + PageWriter::lenDelta(t[1] - 1) * 5
                                       + PageWriter::lenDelta(t[2] - 1));
                    b.w.writeDelta(delta);
                    b.w.writeDelta(t[1] - 1);
                    b.w.writeDelta(t[2] - 1);
                }
            }

            last = t;
            ++count;
        }

        tb.endLeaf(last);
    }

    b.triples[static_cast<int>(order)].end = tb.getLastLeaf();
    b.triplesCount = count;

    // Construct inner nodes
    b.triples[static_cast<int>(order)].index = tb.constructTree();
}

struct WriteAggregatedTriple : public AggregatedTriple {
    WriteAggregatedTriple() {}
    WriteAggregatedTriple(const BasicTriple<Value::id_t>& o) : AggregatedTriple(o) {}

    /**
     * Construct a triple with value v for each component.
     */
    WriteAggregatedTriple(Value::id_t v) {
        for(int i = 0; i < COMPONENTS; i++)
            c[i] = v;
    }

    WriteAggregatedTriple(const WriteAggregatedTriple&) = default;
    WriteAggregatedTriple& operator=(const WriteAggregatedTriple&) = default;

    void write(PageWriter& writer) const {
        // only write the key, without the count
        for(int i = 0; i < COMPONENTS - 1; i++)
            writer.writeInt(c[i]);
    }
};

/**
 * Store the aggregated triples of a particular order.
 *
 * @param b store builder
 * @param triples file with triples
 * @param order order of the triples
 */
template<int C1, int C2, int C3>
static void storeAggregatedTriples(StoreBuilder& b, TempFile& triples,
                                   TripleOrder order) {
    BTreeBuilder<WriteAggregatedTriple> tb(&b.w);
    // Construct leaves
    {
        WriteAggregatedTriple last(0);
        MMapFile in(triples.fileName().c_str());
        for(Cursor cur = in.begin(), end = in.end(); cur != end;) {
            // Read triples, reorder and count
            WriteAggregatedTriple t;
            for(int i = 0; i < t.COMPONENTS; i++)
                t[i] = static_cast<Value::id_t>(cur.readBigInt());
            t = t.reorder<C1,C2,C3>();
            t[2] = 1;
            while(cur != end) {
                Cursor backup = cur;
                Triple t2;
                for(int i = 0; i < t2.COMPONENTS; i++)
                    t2[i] = static_cast<Value::id_t>(cur.readBigInt());
                t2 = t2.reorder<C1,C2,C3>();
                if(t2[0] == t[0] && t2[1] == t[1]) {
                    ++t[2];
                } else {
                    cur = backup;
                    break;
                }
            }

            // Compute encoded length
            unsigned len;
            if(t[0] == last[0]) {
                if(t[1] - last[1] < 32 && t.count() < 5)
                    len = 1;
                else
                    len = 1 + PageWriter::lenDelta(t[1] - last[1] - 1)
                            + PageWriter::lenDelta(t.count()-1);
            } else {
                len = 1 + PageWriter::lenDelta(t[0] - last[0])
                        + PageWriter::lenDelta(t[1] - 1)
                        + PageWriter::lenDelta(t.count()-1);
            }

            // Should we start a new leaf? (first element or no more room)
            if(last[0] == 0 || len > b.w.remaining()) {
                if(last[0] != 0)
                    tb.endLeaf(last);
                tb.beginLeaf();
                // Write the first element of a page fully
                for(int i = 0; i < t.COMPONENTS; i++)
                    b.w.writeInt(t[i]);
            } else {
                // Otherwise, pack the triple
                if(t[0] == last[0]) {
                    if(t[1] - last[1] < 32 && t.count() < 5) {
                        b.w.writeByte(((t.count()-1) << 5) | (t[1] - last[1]));
                    } else {
                        unsigned delta = t[1] - last[1] - 1;
                        b.w.writeByte(0x80 + PageWriter::lenDelta(delta) * 5
                                           + PageWriter::lenDelta(t.count()-1));
                        b.w.writeDelta(delta);
                        b.w.writeDelta(t.count()-1);
                    }
                } else {
                    unsigned delta = t[0] - last[0];
                    b.w.writeByte(0x80 + PageWriter::lenDelta(delta) * 25
                                       + PageWriter::lenDelta(t[1] - 1) * 5
                                       + PageWriter::lenDelta(t.count()-1));
                    b.w.writeDelta(delta);
                    b.w.writeDelta(t[1] - 1);
                    b.w.writeDelta(t.count()-1);
                }
            }

            last = t;
        }

        tb.endLeaf(last);
    }

    // Construct inner nodes
    b.triples[static_cast<int>(order)].aggregated = tb.constructTree();
}

struct WriteFullyAggregatedTriple : public FullyAggregatedTriple {
    WriteFullyAggregatedTriple() {}
    WriteFullyAggregatedTriple(const BasicTriple<Value::id_t>& o) : FullyAggregatedTriple(o) {}

    /**
     * Construct a triple with value v for each component.
     */
    WriteFullyAggregatedTriple(Value::id_t v) {
        for(int i = 0; i < COMPONENTS; i++)
            c[i] = v;
    }

    WriteFullyAggregatedTriple(const WriteFullyAggregatedTriple&) = default;
    WriteFullyAggregatedTriple& operator=(const WriteFullyAggregatedTriple&) = default;

    void write(PageWriter& writer) const {
        // only write the key, without the count
        for(int i = 0; i < COMPONENTS - 1; i++)
            writer.writeInt(c[i]);
    }
};

/**
 * Store the fully aggregated triples of a particular order.
 *
 * @param b store builder
 * @param triples file with triples
 */
template<int C1, int C2, int C3>
static void storeFullyAggregatedTriples(StoreBuilder& b, TempFile& triples) {
    BTreeBuilder<WriteAggregatedTriple> tb(&b.w);
    // Construct leaves
    {
        WriteAggregatedTriple last(0);
        MMapFile in(triples.fileName().c_str());
        for(Cursor cur = in.begin(), end = in.end(); cur != end;) {
            // Read triples, reorder and count
            WriteFullyAggregatedTriple t;
            for(int i = 0; i < Triple::COMPONENTS; i++)
                t[i] = static_cast<Value::id_t>(cur.readBigInt());
            t = t.reorder<C1,C2,C3>();
            t[1] = 1;
            while(cur != end) {
                Cursor backup = cur;
                Triple t2;
                for(int i = 0; i < t2.COMPONENTS; i++)
                    t2[i] = static_cast<Value::id_t>(cur.readBigInt());
                t2 = t2.reorder<C1,C2,C3>();
                if(t2[0] == t[0]) {
                    ++t[1];
                } else {
                    cur = backup;
                    break;
                }
            }

            // Compute encoded length
            unsigned len;
            if(t[0] - last[0] < 16 && t.count() < 9)
                len = 1;
            else
                len = 1 + PageWriter::lenDelta(t[0] - last[0] - 1)
                        + PageWriter::lenDelta(t.count() - 1);

            // Should we start a new leaf? (first element or no more room)
            if(last[0] == 0 || len > b.w.remaining()) {
                if(last[0] != 0)
                    tb.endLeaf(last);
                tb.beginLeaf();
                // Write the first element of a page fully
                for(int i = 0; i < t.COMPONENTS; i++)
                    b.w.writeInt(t[i]);
            } else {
                // Otherwise, pack the triple
                if(t[0] - last[0] < 16 && t.count() < 9) {
                    b.w.writeByte(((t.count()-1) << 4) | (t[0] - last[0]));
                } else {
                    unsigned delta = t[0] - last[0] - 1;
                    b.w.writeByte(0x80 + PageWriter::lenDelta(delta) * 5
                                       + PageWriter::lenDelta(t.count()-1));
                    b.w.writeDelta(delta);
                    b.w.writeDelta(t.count()-1);
                }
            }

            last = t;
        }

        tb.endLeaf(last);
    }

    // Construct inner nodes
    b.fullyAggregated[C1] = tb.constructTree();
}

/**
 * Store triples of a particular order
 *
 * @param b store builder
 * @param triples file with triples (should be sorted according to order)
 * @param order order defined by the template parameters
 * @param fullyAggregated if true, also generate fully aggregated triples
 */
template<int C1=0, int C2=1, int C3=2>
static void storeTriplesOrder(StoreBuilder& b, TempFile& triples,
                              TripleOrder order,
                              bool fullyAggregated) {
    storeFullTriples<C1,C2,C3>(b, triples, order);
    storeAggregatedTriples<C1,C2,C3>(b, triples, order);
    if(fullyAggregated)
        storeFullyAggregatedTriples<C1,C2,C3>(b, triples);
}

/**
 * Sort and reorder the triples file and store that particular order
 *
 * @param b store builder
 * @param triples file with triples
 * @param order order defined by the template parameters
 * @param fullyAggregated if true, also generate fully aggregated triples
 */
template<int C1, int C2, int C3>
static void storeTriplesOrderSorted(StoreBuilder& b, TempFile& triples,
                                    TripleOrder order,
                                    bool fullyAggregated) {
    TempFile sorted(triples.baseName());
    FileSorter::sort(triples, sorted, skipTriple, compareTriple<C1,C2,C3>);
    storeTriplesOrder<C1,C2,C3>(b, sorted, order, fullyAggregated);
}

/**
 * Store the triples.
 *
 * @param b store builder
 * @param triples file with triples. Will be discarded.
 */
static void storeTriples(StoreBuilder& b, TempFile& triples) {
    storeTriplesOrder             (b, triples, TripleOrder::SPO, true);
    storeTriplesOrderSorted<0,2,1>(b, triples, TripleOrder::SOP, false);
    storeTriplesOrderSorted<1,0,2>(b, triples, TripleOrder::PSO, true);
    storeTriplesOrderSorted<1,2,0>(b, triples, TripleOrder::POS, false);
    storeTriplesOrderSorted<2,0,1>(b, triples, TripleOrder::OSP, true);
    storeTriplesOrderSorted<2,1,0>(b, triples, TripleOrder::OPS, false);
    triples.discard();
}

////////////////////////////////////////////////////////////////////////////////
// Storing values

/**
 * Store the raw values
 *
 * @param b store builder
 * @param values file with ordered values. Will be discarded.
 * @param loc will contain (hash, page, offset) triple for each value
 */
static void storeValuesRaw(StoreBuilder& b, TempFile& values, TempFile& loc) {
    const unsigned HEADER_SIZE = 8;
    b.values.begin = b.w.page();

    MMapFile in(values.fileName().c_str());
    b.w.skip(HEADER_SIZE); // skip header (next page, count)
    unsigned count = 0;
    for(Cursor cur = in.begin(), end = in.end(); cur != end;) {
        Hash::hash_t hash = cur.peekValueHash();
        unsigned len = cur.peekValueSize();
        Cursor data = cur;
        cur += len;

        // full page?
        if(len > b.w.remaining()) {
            b.w.writeInt(b.w.page()+1, 0);
            b.w.writeInt(count, 4);
            b.w.flush();
            count = 0;
            b.w.skip(HEADER_SIZE);
        }

        if(len > b.w.remaining()) {
            // overlong value
            unsigned pages = (HEADER_SIZE + len + PageWriter::PAGE_SIZE - 1)
                             / PageWriter::PAGE_SIZE;

            // write location
            loc.writeBigInt(hash);
            loc.writeBigInt(b.w.page());
            loc.writeBigInt(b.w.offset());

            // first page
            b.w.writeInt(b.w.page() + pages, 0);
            b.w.writeInt(1, 4);
            data += b.w.remaining();
            len -= b.w.remaining();
            b.w.write(data.get() - b.w.remaining(), b.w.remaining());
            b.w.flush();

            // intermediate pages
            while(len > PageWriter::PAGE_SIZE) {
                b.w.directWrite(data.get());
                data += PageWriter::PAGE_SIZE;
                len -= PageWriter::PAGE_SIZE;
            }

            // last page
            if(len > 0) {
                b.w.write(data.get(), len);
                b.w.flush(); // needed to pad with zeros
            }
        } else {
            // normal case

            // write location
            loc.writeBigInt(hash);
            loc.writeBigInt(b.w.page());
            loc.writeBigInt(b.w.offset());

            // write value
            b.w.write(data.get(), len);

            count++;
        }
    }

    // last page
    b.w.writeInt(0, 0);
    b.w.writeInt(count, 4);
    b.w.flush();

    values.discard();
}

/**
 * Store the value mappings
 *
 * @param b store builder
 * @param loc file with (hash, page, offset) triple of each value
 */
static void storeValuesMapping(StoreBuilder& b, TempFile& loc) {
    b.values.mapping = b.w.page();

    MMapFile in(loc.fileName().c_str());
    for(Cursor cur = in.begin(), end = in.end(); cur != end;) {
        cur.skipBigInt(); // skip hash
        unsigned page = cur.readBigInt();
        unsigned offset = cur.readBigInt();

        if(b.w.remaining() < 8)
            b.w.flush();

        b.w.writeInt(page);
        b.w.writeInt(offset);
    }

    b.w.flush();
}

struct WriteValueHashKey : public ValueHashKey {
    void write(PageWriter& writer) const { writer.writeInt(hash); }
};

/**
 * Store the values index
 *
 * @param b store builder
 * @param loc file with (hash, page, offset) triple of each value.
 *            Will be discarded.
 */
static void storeValuesIndex(StoreBuilder& b, TempFile& loc) {
    TempFile sorted(loc.baseName());
    FileSorter::sort(loc, sorted, skipTriple, compareBigInt);
    loc.discard();

    const unsigned SUBHEADER_SIZE = 4; // additional header size
    const unsigned ENTRY_SIZE = 8; // size of an entry

    BTreeBuilder<WriteValueHashKey> tb(&b.w);
    std::vector<unsigned> pages;
    MMapFile in(sorted.fileName().c_str());
    WriteValueHashKey last;
    unsigned count = 0;
    tb.beginLeaf();
    unsigned countOffset = b.w.offset(); // offset of count header
    b.w.skip(SUBHEADER_SIZE); // keep room for count
    unsigned headerSize = b.w.offset(); // full header size
    for(Cursor cur = in.begin(), end = in.end(); cur != end;) {
        // collect identical hash values
        Hash::hash_t hash = cur.readBigInt();
        pages.push_back(cur.readBigInt());
        cur.skipBigInt(); // skip offset
        while(cur != end) {
            Cursor oldcur = cur;
            if(cur.readBigInt() == hash) {
                pages.push_back(cur.readBigInt());
                cur.skipBigInt(); // skip offset
            } else {
                cur = oldcur;
                break;
            }
        }

        // start new page?
        if(ENTRY_SIZE * pages.size() > b.w.remaining()) {
            if(headerSize + ENTRY_SIZE * pages.size() > PageWriter::PAGE_SIZE) {
                // too big for any page
                throw CastorException() << "Too many collisions in hash table";
            }
            // flush page
            b.w.writeInt(count, countOffset);
            tb.endLeaf(last);
            count = 0;
            tb.beginLeaf();
            b.w.skip(SUBHEADER_SIZE);
        }

        for(unsigned page : pages) {
            b.w.writeInt(hash);
            b.w.writeInt(page);
            count++;
        }

        last.hash = hash;
        pages.clear();
    }

    // flush last page
    b.w.writeInt(count, countOffset);
    tb.endLeaf(last);

    b.values.index = tb.constructTree();
}

/**
 * Store the values equivalence classes boundaries
 *
 * @param b store builder
 * @param valueEqClasses file with the boundaries
 */
static void storeValuesEqClasses(StoreBuilder& b, TempFile& valueEqClasses) {
    b.values.eqClasses = b.w.page();

    MMapFile in(valueEqClasses.fileName().c_str());
    Cursor cur = in.begin();
    unsigned len = in.end() - cur;
    while(len > PageWriter::PAGE_SIZE) {
        b.w.directWrite(cur.get());
        cur += PageWriter::PAGE_SIZE;
        len -= PageWriter::PAGE_SIZE;
    }

    if(len > 0) {
        b.w.write(cur.get(), len);
        b.w.flush();
    }
}

/**
 * Store the values
 *
 * @param b store builder
 * @param values file with ordered values. Will be discarded.
 * @param valueEqClasses value equivalence classes boundaries
 */
static void storeValues(StoreBuilder& b, TempFile& values,
                        TempFile& valueEqClasses) {
    TempFile loc(values.baseName());
    storeValuesRaw(b, values, loc);
    loc.close();
    storeValuesMapping(b, loc);
    storeValuesIndex(b, loc);
    storeValuesEqClasses(b, valueEqClasses);
}

////////////////////////////////////////////////////////////////////////////////
// Storing header

/**
 * Store the dictionary and write header
 *
 * @param b store builder
 */
static void storeHeader(StoreBuilder& b) {
    b.w.seek(0);

    // Magic number
    b.w.write(Store::MAGIC, sizeof(Store::MAGIC));
    // Format version
    b.w.writeInt(Store::VERSION);

    // Triples count
    b.w.writeInt(b.triplesCount);

    // Triples
    for(int i = 0; i < TRIPLE_ORDERS; i++) {
        b.w.writeInt(b.triples[i].begin);
        b.w.writeInt(b.triples[i].end);
        b.w.writeInt(b.triples[i].index);
        b.w.writeInt(b.triples[i].aggregated);
    }

    // Fully aggregated triples
    for(int i = 0; i < Triple::COMPONENTS; i++)
        b.w.writeInt(b.fullyAggregated[i]);

    // Values
    b.w.writeInt(b.values.begin);
    b.w.writeInt(b.values.mapping);
    b.w.writeInt(b.values.index);
    b.w.writeInt(b.values.eqClasses);
    for(Value::Category cat = Value::CAT_BLANK; cat <= Value::CATEGORIES; ++cat)
        b.w.writeInt(b.values.categories[cat]);

    b.w.flush();
}

////////////////////////////////////////////////////////////////////////////////
// Program entry point

int main(int argc, char* argv[]) {
    // Parse options
    bool force = false;
    const char* syntax = "turtle";
    int c;
    while((c = getopt(argc, argv, "s:f")) != -1) {
        switch(c) {
        case 's':
            syntax = optarg;
            break;
        case 'f':
            force = true;
            break;
        default:
            return 1;
        }
    }

    if(argc - optind != 2) {
        cout << "Usage: " << argv[0] << " [options] DB RDF" << endl;
        return 1;
    }
    char* dbpath = argv[optind++];
    char* rdfpath = argv[optind++];

    struct stat stbuf;
    if(lstat(rdfpath, &stbuf) == -1) {
        cerr << "Cannot find RDF input '" << rdfpath << "'." << endl;
        return 2;
    }
    if(!force && lstat(dbpath, &stbuf) != -1) {
        cerr << "Output file '" << dbpath << "' already exists. Exiting." << endl;
        return 2;
    }


    cout << "Parsing RDF..." << endl;
    TempFile rawTriples(dbpath), rawValues(dbpath);
    {
        librdf::RdfParser parser(syntax, rdfpath);
        RDFLoader loader(&rawTriples, &rawValues);
        parser.parse(&loader);
    }
    rawTriples.close();
    rawValues.close();

    cout << "Building value dictionary..." << endl;
    TempFile values(dbpath), valueIdMap(dbpath), valueEqClasses(dbpath);
    Value::id_t categories[Value::CATEGORIES + 1];
    buildDictionary(rawValues, values, valueIdMap, valueEqClasses, categories);
    values.close();
    valueIdMap.close();
    valueEqClasses.close();

    cout << "Resolving ids..." << endl;
    TempFile triples(dbpath);
    resolveIds(rawTriples, triples, valueIdMap);
    valueIdMap.discard();
    triples.close();


    StoreBuilder b(dbpath);
    b.w.flush(); // reserve page 0 for header
    memcpy(b.values.categories, categories, sizeof(categories));

    cout << "Storing triples..." << endl;
    storeTriples(b, triples);

    cout << "Storing values..." << endl;
    storeValues(b, values, valueEqClasses);

    cout << "Storing header..." << endl;
    storeHeader(b);

    b.w.close();
    cout << "Done." << endl;
    return 0;
}
