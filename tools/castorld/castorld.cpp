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

class RDFLoader : public librdf::RDFParseHandler {
    TempFile *triples;
    ValueLookup values;

public:
    RDFLoader(TempFile *rawTriples, TempFile *rawValues)
            : triples(rawTriples), values(rawValues) {}

    void parseTriple(raptor_statement *triple) {
        Value subject(triple->subject);
        Value predicate(triple->predicate);
        Value object(triple->object);

        uint64_t subjectId = values.lookup(subject);
        uint64_t predicateId = values.lookup(predicate);
        uint64_t objectId = values.lookup(object);

        triples->writeBigInt(subjectId);
        triples->writeBigInt(predicateId);
        triples->writeBigInt(objectId);
    }
};

////////////////////////////////////////////////////////////////////////////////
// Dictionary building

/**
 * Skip a (Value, int) pair
 */
static void skipValueInt(Cursor &cur) {
    cur.skipValue();
    cur.skipBigInt();
}

/**
 * Skip a (int, int) pair
 */
static void skipIntInt(Cursor &cur) {
    cur.skipBigInt();
    cur.skipBigInt();
}

/**
 * Compare function for values using SPARQL order
 */
static int compareValue(Cursor a, Cursor b) {
    Value va, vb;
    a.readValue(va); va.ensureInterpreted();
    b.readValue(vb); vb.ensureInterpreted();
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
 * @param classStart array that will contain the start ids for each class
 *                   (including virtual last class)
 */
static void buildDictionary(TempFile &rawValues, TempFile &values,
                            TempFile &valueIdMap, TempFile &valueEqClasses,
                            Value::id_t *classStart) {
    // sort values using SPARQL order
    TempFile sortedValues(rawValues.getBaseName());
    FileSorter::sort(rawValues, sortedValues, skipValueInt, compareValue);
    rawValues.discard();

    for(Value::Class cls = Value::CLASS_BLANK; cls <= Value::CLASSES_COUNT; ++cls)
        classStart[cls] = 0;

    // construct values list without duplicates and remember mappings
    TempFile rawMap(rawValues.getBaseName());
    {
        MMapFile in(sortedValues.getFileName().c_str());
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
                if(last.id == 0 || last.getClass() != val.getClass())
                    classStart[val.getClass()] = val.id;
                last.fillMove(val);
            }
            rawMap.writeBigInt(id);
            rawMap.writeBigInt(last.id);
        }
        // terminate equivalence classes boundaries
        eqBuf |= 1 << eqShift;
        valueEqClasses.writeInt(eqBuf);
        // terminate class starts
        classStart[Value::CLASSES_COUNT] = last.id + 1;
        for(Value::Class cls = Value::CLASSES_COUNT; cls >= Value::CLASS_BLANK; --cls) {
            if(classStart[cls] == 0)
                classStart[cls] = classStart[cls + 1];
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
static void skipTriple(Cursor &cur) {
    cur.skipBigInt();
    cur.skipBigInt();
    cur.skipBigInt();
}

/**
 * Compare function for triples using the specified component order
 */
template <int C1=1, int C2=2, int C3=3>
static int compareTriple(Cursor a, Cursor b) {
    uint64_t ta[3] = {a.readBigInt(),
                      a.readBigInt(),
                      a.readBigInt()};
    uint64_t tb[3] = {b.readBigInt(),
                      b.readBigInt(),
                      b.readBigInt()};
    int c = cmpInt(ta[C1-1], tb[C1-1]);
    if(c)
        return c;
    c = cmpInt(ta[C2-1], tb[C2-1]);
    if(c)
        return c;
    return cmpInt(ta[C3-1], tb[C3-1]);
}

/**
 * Rewrite triples, resolving the first component with the new ids
 *
 * @param in the triples with old ids for the first component. Will be discarded.
 * @param out will contain the triples with the new ids and components shifted
 * @param map file with (old id, new id) mappings
 */
static void resolveIdsComponent(TempFile &in, TempFile &out, MMapFile &map) {
    // sort by first component
    TempFile sorted(in.getBaseName());
    FileSorter::sort(in, sorted, skipTriple, compareBigInt);
    in.discard();

    // resolve first component and shift components
    {
        MMapFile f(sorted.getFileName().c_str());
        uint64_t from = 0, to = 0;
        Cursor mapCursor = map.begin();
        for(Cursor cur = f.begin(), end = f.end(); cur != end;) {
            uint64_t first = cur.readBigInt();
            uint64_t second = cur.readBigInt();
            uint64_t third = cur.readBigInt();
            while(from < first) {
                from = mapCursor.readBigInt();
                to = mapCursor.readBigInt();
            }
            out.writeBigInt(second);
            out.writeBigInt(third);
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
static void resolveIds(TempFile &rawTriples, TempFile &triples, TempFile &idMap) {
    MMapFile map(idMap.getFileName().c_str());

    // resolve subjects
    TempFile subjectResolved(rawTriples.getBaseName());
    resolveIdsComponent(rawTriples, subjectResolved, map);

    // resolve predicates
    TempFile predicateResolved(rawTriples.getBaseName());
    resolveIdsComponent(subjectResolved, predicateResolved, map);

    // resolve objects
    TempFile objectResolved(rawTriples.getBaseName());
    resolveIdsComponent(predicateResolved, objectResolved, map);

    // final sort, removing duplicates
    FileSorter::sort(objectResolved, triples, skipTriple, compareTriple, true);
}

////////////////////////////////////////////////////////////////////////////////
// Common definitions for store creation

struct StoreBuilder {
    PageWriter w; //!< store output

    unsigned triplesStart[3]; //!< start of triples tables in all orderings
    unsigned triplesIndex[3]; //!< root of triples index in all orderings
    unsigned valuesStart; //!< start of values table
    unsigned valuesMapping; //!< start of values mapping
    unsigned valuesIndex; //!< root of values index (hash->page mapping)
    unsigned valueEqClasses; //!< start of value equivalence classes boundaries
    Value::id_t classStart[Value::CLASSES_COUNT + 1]; //!< first id of each class

    StoreBuilder(const char* fileName) : w(fileName) {}
};

////////////////////////////////////////////////////////////////////////////////
// Storing triples

struct WriteTriple {
    Value::id_t a, b, c;

    static const unsigned SIZE = 12;
    void write(PageWriter &writer) const {
        writer.writeInt(a);
        writer.writeInt(b);
        writer.writeInt(c);
    }
};

/**
 * Store a particular triples order.
 */
template<int C1=1, int C2=2, int C3=3>
static void storeTriplesOrder(StoreBuilder &b, TempFile &triples, int order) {
    b.triplesStart[order] = b.w.getPage();

    BTreeBuilder<WriteTriple> tb(&b.w);
    // Construct leaves
    {
        WriteTriple last = {0, 0, 0};
        MMapFile in(triples.getFileName().c_str());
        for(Cursor cur = in.begin(), end = in.end(); cur != end;) {
            // Read triple and reorder
            Value::id_t read[3] = {static_cast<Value::id_t>(cur.readBigInt()),
                                   static_cast<Value::id_t>(cur.readBigInt()),
                                   static_cast<Value::id_t>(cur.readBigInt())};
            WriteTriple t = {read[C1-1], read[C2-1], read[C3-1]};

            // Compute encoded length
            unsigned len;
            if(t.a == last.a) {
                if(t.b == last.b) {
                    assert(t.c != last.c); // there should not be any duplicate anymore
                    if(t.c - last.c < 128)
                        len = 1;
                    else
                        len = 1 + PageWriter::lenDelta(t.c - last.c - 128);
                } else {
                    len = 1 + PageWriter::lenDelta(t.b - last.b)
                            + PageWriter::lenDelta(t.c - 1);
                }
            } else {
                len = 1 + PageWriter::lenDelta(t.a - last.a)
                        + PageWriter::lenDelta(t.b - 1)
                        + PageWriter::lenDelta(t.c - 1);
            }

            // Should we start a new leaf? (first element or no more room)
            if(last.a == 0 || len > b.w.getRemaining()) {
                if(last.a != 0)
                    tb.endLeaf(last);
                tb.beginLeaf();
                // Write the first element of a page fully
                b.w.writeInt(t.a);
                b.w.writeInt(t.b);
                b.w.writeInt(t.c);
            } else {
                // Otherwise, pack the triple
                if(t.a == last.a) {
                    if(t.b == last.b) {
                        if(t.c - last.c < 128) {
                            b.w.writeByte(t.c - last.c);
                        } else {
                            unsigned delta = t.c - last.c - 128;
                            b.w.writeByte(0x80 + PageWriter::lenDelta(delta));
                            b.w.writeDelta(delta);
                        }
                    } else {
                        unsigned delta = t.b - last.b;
                        b.w.writeByte(0x80 + PageWriter::lenDelta(delta) * 5
                                           + PageWriter::lenDelta(t.c - 1));
                        b.w.writeDelta(delta);
                        b.w.writeDelta(t.c - 1);
                    }
                } else {
                    unsigned delta = t.a - last.a;
                    b.w.writeByte(0x80 + PageWriter::lenDelta(delta) * 25
                                       + PageWriter::lenDelta(t.b - 1) * 5
                                       + PageWriter::lenDelta(t.c - 1));
                    b.w.writeDelta(delta);
                    b.w.writeDelta(t.b - 1);
                    b.w.writeDelta(t.c - 1);
                }
            }

            last = t;
        }

        tb.endLeaf(last);
    }

    // Construct inner nodes
    b.triplesIndex[order] = tb.constructTree();
}

/**
 * Sort and reorder the triples file and store that particular order
 */
template<int C1=1, int C2=2, int C3=3>
static void storeTriplesOrderSorted(StoreBuilder &b, TempFile &triples, int order) {
    TempFile sorted(triples.getBaseName());
    FileSorter::sort(triples, sorted, skipTriple, compareTriple<C1,C2,C3>);
    storeTriplesOrder<C1,C2,C3>(b, sorted, order);
}

/**
 * Store the triples.
 *
 * @param b store builder
 * @param triples file with triples. Will be discarded.
 */
static void storeTriples(StoreBuilder &b, TempFile &triples) {
    storeTriplesOrder(b, triples, 0);
    storeTriplesOrderSorted<2,3,1>(b, triples, 1);
    storeTriplesOrderSorted<3,1,2>(b, triples, 2);
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
static void storeValuesRaw(StoreBuilder &b, TempFile &values, TempFile &loc) {
    const unsigned HEADER_SIZE = 8;
    b.valuesStart = b.w.getPage();

    MMapFile in(values.getFileName().c_str());
    b.w.skip(HEADER_SIZE); // skip header (next page, count)
    unsigned count = 0;
    for(Cursor cur = in.begin(), end = in.end(); cur != end;) {
        uint32_t hash = cur.peekValueHash();
        unsigned len = cur.peekValueSize();
        Cursor data = cur;
        cur += len;

        // full page?
        if(len > b.w.getRemaining()) {
            b.w.writeInt(b.w.getPage()+1, 0);
            b.w.writeInt(count, 4);
            b.w.flushPage();
            count = 0;
            b.w.skip(HEADER_SIZE);
        }

        if(len > b.w.getRemaining()) {
            // overlong value
            unsigned pages = (HEADER_SIZE + len + PageWriter::PAGE_SIZE - 1)
                             / PageWriter::PAGE_SIZE;

            // write location
            loc.writeBigInt(hash);
            loc.writeBigInt(b.w.getPage());
            loc.writeBigInt(b.w.getOffset());

            // first page
            b.w.writeInt(b.w.getPage() + pages, 0);
            b.w.writeInt(1, 4);
            data += b.w.getRemaining();
            len -= b.w.getRemaining();
            b.w.write(data.get() - b.w.getRemaining(), b.w.getRemaining());
            b.w.flushPage();

            // intermediate pages
            while(len > PageWriter::PAGE_SIZE) {
                b.w.directWrite(data.get());
                data += PageWriter::PAGE_SIZE;
                len -= PageWriter::PAGE_SIZE;
            }

            // last page
            if(len > 0) {
                b.w.write(data.get(), len);
                b.w.flushPage(); // needed to pad with zeros
            }
        } else {
            // normal case

            // write location
            loc.writeBigInt(hash);
            loc.writeBigInt(b.w.getPage());
            loc.writeBigInt(b.w.getOffset());

            // write value
            b.w.write(data.get(), len);

            count++;
        }
    }

    // last page
    b.w.writeInt(0, 0);
    b.w.writeInt(count, 4);
    b.w.flushPage();

    values.discard();
}

/**
 * Store the value mappings
 *
 * @param b store builder
 * @param loc file with (hash, page, offset) triple of each value
 */
static void storeValuesMapping(StoreBuilder &b, TempFile &loc) {
    b.valuesMapping = b.w.getPage();

    MMapFile in(loc.getFileName().c_str());
    for(Cursor cur = in.begin(), end = in.end(); cur != end;) {
        cur.skipBigInt(); // skip hash
        unsigned page = cur.readBigInt();
        unsigned offset = cur.readBigInt();

        if(b.w.getRemaining() < 8)
            b.w.flushPage();

        b.w.writeInt(page);
        b.w.writeInt(offset);
    }

    b.w.flushPage();
}

struct ValueHash {
    uint32_t h;

    static const unsigned SIZE = 4;
    void write(PageWriter &writer) const { writer.writeInt(h); }
};

/**
 * Store the values index
 *
 * @param b store builder
 * @param loc file with (hash, page, offset) triple of each value.
 *            Will be discarded.
 */
static void storeValuesIndex(StoreBuilder &b, TempFile &loc) {
    TempFile sorted(loc.getBaseName());
    FileSorter::sort(loc, sorted, skipTriple, compareBigInt);
    loc.discard();

    const unsigned SUBHEADER_SIZE = 4; // additional header size
    const unsigned ENTRY_SIZE = 8; // size of an entry

    BTreeBuilder<ValueHash> tb(&b.w);
    std::vector<unsigned> pages;
    MMapFile in(sorted.getFileName().c_str());
    ValueHash last;
    unsigned count = 0;
    tb.beginLeaf();
    unsigned countOffset = b.w.getOffset(); // offset of count header
    b.w.skip(SUBHEADER_SIZE); // keep room for count
    unsigned headerSize = b.w.getOffset(); // full header size
    for(Cursor cur = in.begin(), end = in.end(); cur != end;) {
        // collect identical hash values
        uint32_t hash = cur.readBigInt();
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
        if(ENTRY_SIZE * pages.size() > b.w.getRemaining()) {
            if(headerSize + ENTRY_SIZE * pages.size() > PageWriter::PAGE_SIZE) {
                // too big for any page
                throw "Too many collisions in hash table.";
            }
            // flush page
            b.w.writeInt(count, countOffset);
            tb.endLeaf(last);
            count = 0;
            tb.beginLeaf();
            b.w.skip(SUBHEADER_SIZE);
        }

        for(unsigned &page : pages) {
            b.w.writeInt(hash);
            b.w.writeInt(page);
            count++;
        }

        last.h = hash;
        pages.clear();
    }

    // flush last page
    b.w.writeInt(count, countOffset);
    tb.endLeaf(last);

    b.valuesIndex = tb.constructTree();
}

/**
 * Store the values equivalence classes boundaries
 *
 * @param b store builder
 * @param valueEqClasses file with the boundaries
 */
static void storeValuesEqClasses(StoreBuilder &b, TempFile &valueEqClasses) {
    b.valueEqClasses = b.w.getPage();

    MMapFile in(valueEqClasses.getFileName().c_str());
    Cursor cur = in.begin();
    unsigned len = in.end() - cur;
    while(len > PageWriter::PAGE_SIZE) {
        b.w.directWrite(cur.get());
        cur += PageWriter::PAGE_SIZE;
        len -= PageWriter::PAGE_SIZE;
    }

    if(len > 0) {
        b.w.write(cur.get(), len);
        b.w.flushPage();
    }
}

/**
 * Store the values
 *
 * @param b store builder
 * @param values file with ordered values. Will be discarded.
 * @param valueEqClasses value equivalence classes boundaries
 */
static void storeValues(StoreBuilder &b, TempFile &values,
                        TempFile &valueEqClasses) {
    TempFile loc(values.getBaseName());
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
static void storeHeader(StoreBuilder &b) {
    b.w.setPage(0);

    // Magic number
    b.w.write(CASTOR_STORE_MAGIC, sizeof(CASTOR_STORE_MAGIC) - 1);
    // Format version
    b.w.writeInt(Store::VERSION);

    // Triples
    for(int i = 0; i < 3; i++) {
        b.w.writeInt(b.triplesStart[i]);
        b.w.writeInt(b.triplesIndex[i]);
    }

    // Values
    b.w.writeInt(b.valuesStart);
    b.w.writeInt(b.valuesMapping);
    b.w.writeInt(b.valuesIndex);
    b.w.writeInt(b.valueEqClasses);
    for(Value::Class cls = Value::CLASS_BLANK; cls <= Value::CLASSES_COUNT; ++cls) {
        b.w.writeInt(b.classStart[cls]);
    }

    b.w.flushPage();
}

////////////////////////////////////////////////////////////////////////////////
// Program entry point

int main(int argc, char* argv[]) {
    // Parse options
    bool force = false;
    const char *syntax = "turtle";
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
    char *dbpath = argv[optind++];
    char *rdfpath = argv[optind++];

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
        librdf::RDFParser parser(syntax, rdfpath);
        RDFLoader loader(&rawTriples, &rawValues);
        parser.parse(&loader);
    }
    rawTriples.close();
    rawValues.close();

    cout << "Building value dictionary..." << endl;
    TempFile values(dbpath), valueIdMap(dbpath), valueEqClasses(dbpath);
    Value::id_t classStart[Value::CLASSES_COUNT + 1];
    buildDictionary(rawValues, values, valueIdMap, valueEqClasses, classStart);
    values.close();
    valueIdMap.close();
    valueEqClasses.close();

    cout << "Resolving ids..." << endl;
    TempFile triples(dbpath);
    resolveIds(rawTriples, triples, valueIdMap);
    valueIdMap.discard();
    triples.close();


    StoreBuilder b(dbpath);
    b.w.flushPage(); // reserve page 0 for header
    memcpy(b.classStart, classStart, sizeof(classStart));

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
