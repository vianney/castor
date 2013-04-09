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

#include "tempfile.h"
#include "sort.h"
#include "lookup.h"
#include "pagewriter.h"
#include "btreebuilder.h"

using namespace std;
using namespace castor;

namespace {

////////////////////////////////////////////////////////////////////////////////
// RDF Parsing

class RDFLoader : public librdf::RdfParseHandler {
public:
    RDFLoader(TempFile* rawTriples, TempFile* rawStrings, TempFile* rawValues)
        : triples(rawTriples), strings(rawStrings), values(rawValues) {
        addURI(String("http://www.w3.org/2001/XMLSchema#string"));
        addURI(String("http://www.w3.org/2001/XMLSchema#boolean"));
        addURI(String("http://www.w3.org/2001/XMLSchema#integer"));
        addURI(String("http://www.w3.org/2001/XMLSchema#double"));
        addURI(String("http://www.w3.org/2001/XMLSchema#decimal"));
        addURI(String("http://www.w3.org/2001/XMLSchema#dateTime"));
    }

    /**
     * Add a URI to the set of values.
     * @param iri lexical form
     * @return a pair (early id of the IRI, early id of the lexical form)
     */
    std::pair<unsigned long, unsigned long> addURI(String uri) {
        EarlyValue val;
        val.fillURI(std::move(uri));
        val.earlyLexical = strings.lookup(val.lexical());
        return {values.lookup(val), val.earlyLexical};
    }

    /**
     * Convert a raptor term to a raw value and write the resulting id.
     * @param term
     */
    void writeValue(raptor_term* term) {
        EarlyValue val(term);
        if(val.isPlainWithLang()) {
            val.earlyTag = strings.lookup(val.language());
        } else if(val.isTyped()) {
            std::pair<unsigned long, unsigned long> ids = addURI(val.datatypeLex());
            val.earlyDatatype = ids.first;
            val.earlyTag = ids.second;
        }
        val.earlyLexical = strings.lookup(val.lexical());
        triples->writeVarInt(values.lookup(val));
    }

    void parseTriple(raptor_statement* triple) {
        writeValue(triple->subject);
        writeValue(triple->predicate);
        writeValue(triple->object);
    }

private:
    TempFile*         triples;
    Lookup<String>    strings;
    Lookup<EarlyValue> values;
};

////////////////////////////////////////////////////////////////////////////////
// Dictionary building

//! Compare two integers
inline int cmpInt(unsigned long a, unsigned long b) {
    return a < b ? -1 : (a > b ? 1 : 0);
}

//! Compare function for big integers
int compareVarInt(Cursor a, Cursor b) {
    return cmpInt(a.readVarInt(), b.readVarInt());
}

/**
 * Build the strings dictionary
 *
 * @param rawStrings (string, early id) mapping. Will be discarded
 * @param strings will contain the sorted strings
 * @param earlyMap will contain the (early id, id) mapping ordered by early id
 * @param map will contain the sequence of offsets ordered by string id
 *            (usable by StringResolver)
 * @param hashes will contain a sequence of (hash, offset) pairs ordered by hash
 * @param[out] count number of strings
 */
void buildStrings(TempFile& rawStrings, TempFile& strings, TempFile& earlyMap,
                  TempFile& map, TempFile& hashes, unsigned& count) {
    // sort strings
    TempFile sortedStrings(rawStrings.baseName());
    FileSorter::sort(rawStrings, sortedStrings,
                     [](Cursor& cur) { String::skip(cur); cur.skipVarInt(); },
                     [](Cursor a, Cursor b) { return String(a).compare(String(b)); });
    rawStrings.discard();

    // construct string list without duplicates, assign ids and remember mapping
    TempFile rawEarlyMap(rawStrings.baseName());
    TempFile rawHashes(rawStrings.baseName());
    {
        MMapFile in(sortedStrings.fileName().c_str());
        String last;
        unsigned long offset = 0;
        for(Cursor cur = in.begin(); cur != in.end();) {
            String s(cur);
            unsigned long id = cur.readVarInt();
            if(last.id() == 0 || s != last) {
                s.id(last.id() + 1);
                map.writeLong(offset);
                rawHashes.writeInt(s.hash());
                rawHashes.writeLong(offset);
                offset += strings.writeBuffer(s.serialize());
                last = s;
            }
            rawEarlyMap.writeVarInt(id);
            rawEarlyMap.writeVarInt(last.id());
        }
        count = last.id();
    }
    rawEarlyMap.close();
    sortedStrings.discard();

    // sort the early map
    FileSorter::sort(rawEarlyMap, earlyMap,
                     [](Cursor& cur) { cur.skipVarInt(); cur.skipVarInt(); },
                     compareVarInt);
    rawEarlyMap.discard();

    // sort the hashes map
    FileSorter::sort(rawHashes, hashes,
                     [](Cursor& cur) { cur.skipInt(); cur.skipLong(); },
                     [](Cursor a, Cursor b) { return cmpInt(a.readInt(), b.readInt()); });
    rawHashes.discard();
}

/**
 * Resolve the ids of the strings of the values.
 *
 * @param rawValues early (TempValue, early id) mapping. Will be discarded.
 * @param resolvedValues will contain the resolved (Value, type, id) mapping,
 *                       but still with the early mapping for value ids
 * @param stringMap file with (early id, id) mappings for strings
 */
void resolveStringIds(TempFile& rawValues, TempFile& resolvedValues,
                      TempFile& stringMap) {
    MMapFile map(stringMap.fileName().c_str());

    // sort by lexical
    TempFile sortedLexical(rawValues.baseName());
    FileSorter::sort(rawValues, sortedLexical,
                     [](Cursor& cur) { EarlyValue::skip(cur); cur.skipVarInt(); },
                     [](Cursor a, Cursor b) { return cmpInt(EarlyValue(a).earlyLexical,
                                                            EarlyValue(b).earlyLexical); });
    rawValues.discard();

    // resolve lexical
    TempFile lexicalResolved(rawValues.baseName());
    {
        MMapFile f(sortedLexical.fileName().c_str());
        unsigned long from = 0;
        String::id_t  to   = 0;
        Cursor mapCursor = map.begin();
        for(Cursor cur = f.begin(); cur != f.end();) {
            EarlyValue val(cur);
            unsigned long id = cur.readVarInt();
            while(from < val.earlyLexical) {
                from = mapCursor.readVarInt();
                to   = mapCursor.readVarInt();
            }
            val.lexical(String(to));
            lexicalResolved.writeBuffer(val.serialize());
            lexicalResolved.writeVarInt(id);
        }
    }
    lexicalResolved.close();
    sortedLexical.discard();

    // sort by tag
    TempFile sortedType(rawValues.baseName());
    FileSorter::sort(lexicalResolved, sortedType,
                     [](Cursor& cur) { EarlyValue::skip(cur); cur.skipVarInt(); },
                     [](Cursor a, Cursor b) { return cmpInt(EarlyValue(a).earlyTag,
                                                            EarlyValue(b).earlyTag); });
    lexicalResolved.discard();

    // resolve tag and write out Value structures
    {
        MMapFile f(sortedType.fileName().c_str());
        unsigned long from = 0;
        String::id_t  to   = 0;
        Cursor mapCursor = map.begin();
        for(Cursor cur = f.begin(); cur != f.end();) {
            EarlyValue val(cur);
            unsigned long id = cur.readVarInt();
            while(from < val.earlyTag) {
                from = mapCursor.readVarInt();
                to   = mapCursor.readVarInt();
            }
            if(val.isPlainWithLang())
                val.language(String(to));
            else if(val.isTyped())
                val.datatypeLex(String(to));
            resolvedValues.writeBuffer(reinterpret_cast<Value*>(&val)->serialize());
            resolvedValues.writeVarInt(val.earlyDatatype);
            resolvedValues.writeVarInt(id);
        }
    }
    sortedType.discard();
}

/**
 * Build the dictionary
 *
 * @param rawValues early (value, type, id) mapping. Will be discarded.
 * @param values will contain the sorted values
 * @param earlyMap will contain the (early id, id) mapping ordered by early id
 * @param hashes will contain (hash, id) pairs ordered by hash
 * @param valueEqClasses will contain the equivalence classes boundaries
 * @param categories array that will contain the start ids for each category
 *                   (including virtual last class)
 * @param resolver string resolver
 */
void buildValues(TempFile& rawValues, TempFile& values, TempFile& earlyMap,
                 TempFile& hashes, TempFile& valueEqClasses,
                 Value::id_t* categories, StringMapper& resolver) {
    // sort values using SPARQL order
    TempFile sortedValues(rawValues.baseName());
    FileSorter::sort(rawValues, sortedValues,
                     [](Cursor& cur) { Value::skip(cur); cur.skipVarInt(); cur.skipVarInt(); },
                     [&resolver](Cursor a, Cursor b) {
                         Value va(a);  va.ensureInterpreted(resolver);
                         Value vb(b);  vb.ensureInterpreted(resolver);
                         if(va == vb) return 0;
                         else if(va < vb) return -1;
                         else return 1;
                     });
    rawValues.discard();

    for(Value::Category cat = Value::CAT_BLANK; cat <= Value::CATEGORIES; ++cat)
        categories[cat] = 0;

    // construct values list without duplicates and remember mappings
    // outputs (type, Value) pairs
    TempFile valuesType(rawValues.baseName());
    TempFile rawMap(rawValues.baseName());
    {
        MMapFile in(sortedValues.fileName().c_str());
        Value last;
        last.id(0);
        unsigned eqBuf = 0, eqShift = 0;
        for(Cursor cur = in.begin(); cur != in.end();) {
            Value val(cur);
            unsigned long type = cur.readVarInt();
            unsigned long id = cur.readVarInt();
            val.ensureInterpreted(resolver);
            if(!last.validId() || last != val) {
                val.id(last.id() + 1);
                assert(val.validId());
                valuesType.writeVarInt(type);
                valuesType.writeBuffer(val.serialize());
                eqBuf |= (last.validId() && last.compare(val) == 0 ? 0 : 1) << (eqShift++);
                if(eqShift == 32) {
                    valueEqClasses.writeInt(eqBuf);
                    eqBuf = 0;
                    eqShift = 0;
                }
                if(!last.validId() || last.category() != val.category())
                    categories[val.category()] = val.id();
                last = std::move(val);
            }
            rawMap.writeVarInt(id);
            rawMap.writeVarInt(last.id());
        }
        // terminate equivalence classes boundaries
        eqBuf |= 1 << eqShift;
        valueEqClasses.writeInt(eqBuf);
        // terminate class starts
        categories[Value::CATEGORIES] = last.id() + 1;
        for(Value::Category cat = static_cast<Value::Category>(Value::CATEGORIES);
            cat >= Value::CAT_BLANK; --cat) {
            if(categories[cat] == 0)
                categories[cat] = categories[cat + 1];
        }
    }
    valuesType.close();
    rawMap.close();
    sortedValues.discard();

    // sort the id map
    FileSorter::sort(rawMap, earlyMap,
                     [](Cursor& cur) { cur.skipVarInt(); cur.skipVarInt(); },
                     compareVarInt);
    rawMap.discard();

    // sort values by type
    TempFile sortedValuesType(rawValues.baseName());
    FileSorter::sort(valuesType, sortedValuesType,
                     [](Cursor& cur) { cur.skipVarInt(); Value::skip(cur); },
                     compareVarInt);
    valuesType.discard();

    // resolve datatypes and write hashes
    TempFile resolvedValues(rawValues.baseName());
    TempFile rawHashes(rawValues.baseName());
    {
        MMapFile in(sortedValuesType.fileName().c_str());
        MMapFile map(earlyMap.fileName().c_str());
        unsigned long from = 0;
        Value::id_t   to   = 0;
        Cursor mapCursor = map.begin();
        for(Cursor cur = in.begin(); cur != in.end();) {
            unsigned long type = cur.readVarInt();
            Value val(cur);
            assert(val.validId());
            val.ensureDirectStrings(resolver);
            while(from < type) {
                from = mapCursor.readVarInt();
                to   = mapCursor.readVarInt();
            }
            assert(from == type);
            if(type != 0) {
                assert(to > 0);
                val.datatypeId(to);
            }
            resolvedValues.writeBuffer(val.serialize());
            // TODO: check hash function, lookup strings?
            rawHashes.writeInt(val.hash());
            rawHashes.writeInt(val.id());
        }
    }
    sortedValuesType.discard();

    // final sort of the values
    // There is no need here to interpret the values, as they all have valid ids.
    FileSorter::sort(resolvedValues, values,
                     [](Cursor& cur) { Value::skip(cur); },
                     [](Cursor a, Cursor b) {
                         Value va(a);
                         Value vb(b);
                         if(va == vb) return 0;
                         else if(va < vb) return -1;
                         else return 1;
                     });
    resolvedValues.discard();

    // sort hashes
    FileSorter::sort(rawHashes, hashes,
                     [](Cursor& cur) { cur.skipInt(); cur.skipInt(); },
                     [](Cursor a, Cursor b) { return cmpInt(a.readInt(), b.readInt()); });
    rawHashes.discard();
}

////////////////////////////////////////////////////////////////////////////////
// ID resolving

/**
 * Skip a (int, int, int) triple
 */
void skipTriple(Cursor& cur) {
    cur.skipVarInt();
    cur.skipVarInt();
    cur.skipVarInt();
}

/**
 * Compare function for triples using the specified component order
 */
template <int C1=0, int C2=1, int C3=2>
int compareTriple(Cursor a, Cursor b) {
    BasicTriple<unsigned long> ta, tb;
    for(int i = 0; i < ta.COMPONENTS; i++)
        ta[i] = a.readVarInt();
    for(int i = 0; i < tb.COMPONENTS; i++)
        tb[i] = b.readVarInt();
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
 * @param map file with (early id, id) value mappings
 */
void resolveIdsComponent(TempFile& in, TempFile& out, MMapFile& map) {
    // sort by first component
    TempFile sorted(in.baseName());
    FileSorter::sort(in, sorted, skipTriple, compareVarInt);
    in.discard();

    // resolve first component and shift components
    {
        MMapFile f(sorted.fileName().c_str());
        unsigned long from = 0;
        Value::id_t   to   = 0;
        Cursor mapCursor = map.begin();
        for(Cursor cur = f.begin(), end = f.end(); cur != end;) {
            BasicTriple<unsigned long> t;
            for(int i = 0; i < t.COMPONENTS; i++)
                t[i] = cur.readVarInt();
            while(from < t[0]) {
                from = mapCursor.readVarInt();
                to   = mapCursor.readVarInt();
            }
            assert(to > 0);
            for(int i = 1; i < t.COMPONENTS; i++)
                out.writeVarInt(t[i]);
            out.writeVarInt(to);
        }
    }
    sorted.discard();
}

/**
 * Rewrite triples using the new ids.
 *
 * @param rawTriples the triples with old ids. Will be discarded.
 * @param triples will contain the triples with the new ids
 * @param valueMap file with (early id, id) value mappings
 */
void resolveIds(TempFile& rawTriples, TempFile& triples, TempFile& valueMap) {
    MMapFile map(valueMap.fileName().c_str());

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

    unsigned triplesTable; //!< first page of the triples table

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
     * Strings
     */
    struct {
        unsigned count;      //!< number of strings
        unsigned begin;      //!< first page of table
        unsigned mapping;    //!< first page of mapping
        unsigned index;      //!< index (hash->offset mapping)
    } strings;

    /**
     * Values
     */
    struct {
        unsigned begin;     //!< first page of table
        unsigned index;     //!< index (hash->id mapping)
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
 * Store the triples table.
 *
 * @param b store builder
 * @param triples
 */
void storeTriplesTable(StoreBuilder& b, TempFile& triples) {
    b.triplesTable = b.w.page();
    MMapFile in(triples.fileName().c_str());
    for(Cursor cur = in.begin(); cur != in.end();) {
        if(b.w.remaining() == 0)
            b.w.flush();
        else if(b.w.remaining() < 4)
            throw CastorException() << "Unaligned write in storeTriplesTable";
        b.w.writeInt(cur.readVarInt());
    }
    b.w.flush();
}

/**
 * Store full triples of a particular order.
 *
 * @param b store builder
 * @param triples file with triples
 * @param order order defined by the template parameters
 */
template<int C1, int C2, int C3>
void storeFullTriples(StoreBuilder& b, TempFile& triples,
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
            for(int i = 0; i < t.COMPONENTS; i++) {
                t[i] = static_cast<Value::id_t>(cur.readVarInt());
                assert(t[i] > 0);
            }
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
void storeAggregatedTriples(StoreBuilder& b, TempFile& triples,
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
                t[i] = static_cast<Value::id_t>(cur.readVarInt());
            t = t.reorder<C1,C2,C3>();
            t[2] = 1;
            while(cur != end) {
                Cursor backup = cur;
                Triple t2;
                for(int i = 0; i < t2.COMPONENTS; i++)
                    t2[i] = static_cast<Value::id_t>(cur.readVarInt());
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
void storeFullyAggregatedTriples(StoreBuilder& b, TempFile& triples) {
    BTreeBuilder<WriteAggregatedTriple> tb(&b.w);
    // Construct leaves
    {
        WriteAggregatedTriple last(0);
        MMapFile in(triples.fileName().c_str());
        for(Cursor cur = in.begin(), end = in.end(); cur != end;) {
            // Read triples, reorder and count
            WriteFullyAggregatedTriple t;
            for(int i = 0; i < Triple::COMPONENTS; i++)
                t[i] = static_cast<Value::id_t>(cur.readVarInt());
            t = t.reorder<C1,C2,C3>();
            t[1] = 1;
            while(cur != end) {
                Cursor backup = cur;
                Triple t2;
                for(int i = 0; i < t2.COMPONENTS; i++)
                    t2[i] = static_cast<Value::id_t>(cur.readVarInt());
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
void storeTriplesOrder(StoreBuilder& b, TempFile& triples,
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
void storeTriplesOrderSorted(StoreBuilder& b, TempFile& triples,
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
void storeTriples(StoreBuilder& b, TempFile& triples) {
    // Store raw table
    storeTriplesTable(b, triples);

    // Store B-trees
    storeTriplesOrder             (b, triples, TripleOrder::SPO, true);
    storeTriplesOrderSorted<0,2,1>(b, triples, TripleOrder::SOP, false);
    storeTriplesOrderSorted<1,0,2>(b, triples, TripleOrder::PSO, true);
    storeTriplesOrderSorted<1,2,0>(b, triples, TripleOrder::POS, false);
    storeTriplesOrderSorted<2,0,1>(b, triples, TripleOrder::OSP, true);
    storeTriplesOrderSorted<2,1,0>(b, triples, TripleOrder::OPS, false);
    triples.discard();
}

////////////////////////////////////////////////////////////////////////////////
// Storing strings

struct WriteHashKey : public HashKey {
    WriteHashKey() = default;
    WriteHashKey(Hash::hash_t hash) : HashKey(hash) {}

    void write(PageWriter& writer) const { writer.writeInt(hash); }
};

/**
 * Store the strings
 *
 * @param b store builder
 * @param strings sequence of Strings ordered by id. Will be discarded.
 * @param map sequence of offsets ordered by id. Will be discarded.
 * @param hashes sequence of (hash, offset) pairs ordered by hash. Will be
 *               discarded.
 * @param stringsCount the number of strings
 */
void storeStrings(StoreBuilder& b, TempFile& strings, TempFile& map,
                  TempFile& hashes, unsigned stringsCount) {
    b.strings.count = stringsCount;

    // Store table
    b.strings.begin = b.w.page();
    {
        MMapFile f(strings.fileName().c_str());
        b.w.directWrite(f.begin().get(), f.size());
    }
    strings.discard();

    // Store mapping
    b.strings.mapping = b.w.page();
    {
        MMapFile f(map.fileName().c_str());
        b.w.directWrite(f.begin().get(), f.size());
    }
    map.discard();

    // Store hashmap
    const std::size_t SUBHEADER_SIZE = 4; // additional header size
    const std::size_t ENTRY_SIZE = 12; // size of an entry

    BTreeBuilder<WriteHashKey> tb(&b.w);
    std::vector<std::size_t> offsets;
    MMapFile in(hashes.fileName().c_str());
    WriteHashKey last;
    unsigned count = 0;
    tb.beginLeaf();
    std::size_t countOffset = b.w.offset(); // offset of count header
    b.w.skip(SUBHEADER_SIZE); // keep room for count
    std::size_t headerSize = b.w.offset(); // full header size
    for(Cursor cur = in.begin(); cur != in.end();) {
        // collect identical hash values
        Hash::hash_t hash = cur.readInt();
        offsets.push_back(cur.readLong());
        while(cur != in.end()) {
            if(cur.peekInt() != hash)
                break;
            cur.skipInt();
            offsets.push_back(cur.readLong());
        }

        // start new page?
        if(ENTRY_SIZE * offsets.size() > b.w.remaining()) {
            if(headerSize + ENTRY_SIZE * offsets.size() > PageWriter::PAGE_SIZE) {
                // too big for any page
                throw CastorException() << "Too many collisions in strings hash table";
            }
            // flush page
            b.w.writeInt(count, countOffset);
            tb.endLeaf(last);
            count = 0;
            tb.beginLeaf();
            b.w.skip(SUBHEADER_SIZE);
        }

        for(unsigned long offset : offsets) {
            b.w.writeInt(hash);
            b.w.writeLong(offset);
            count++;
        }

        last = hash;
        offsets.clear();
    }

    // flush last page
    b.w.writeInt(count, countOffset);
    tb.endLeaf(last);

    b.strings.index = tb.constructTree();
}

////////////////////////////////////////////////////////////////////////////////
// Storing values

/**
 * Store the values
 *
 * @param b store builder
 * @param values file with ordered values. Will be discarded.
 * @param hashes file with (hash, id) pairs ordered by hash. Will be discarded.
 * @param eqClasses value equivalence classes boundaries. Will be discarded.
 */
void storeValues(StoreBuilder& b, TempFile& values, TempFile& hashes,
                 TempFile& eqClasses) {
    // Store table
    b.values.begin = b.w.page();
    {
        MMapFile f(values.fileName().c_str());
        b.w.directWrite(f.begin().get(), f.size());
    }
    values.discard();

    // Store eqClasses
    b.values.eqClasses = b.w.page();
    {
        MMapFile f(eqClasses.fileName().c_str());
        b.w.directWrite(f.begin().get(), f.size());
    }
    eqClasses.discard();

    // Store hashmap
    const unsigned SUBHEADER_SIZE = 4; // additional header size
    const unsigned ENTRY_SIZE = 8; // size of an entry

    BTreeBuilder<WriteHashKey> tb(&b.w);
    std::vector<Value::id_t> ids;
    MMapFile in(hashes.fileName().c_str());
    WriteHashKey last;
    unsigned count = 0;
    tb.beginLeaf();
    std::size_t countOffset = b.w.offset(); // offset of count header
    b.w.skip(SUBHEADER_SIZE); // keep room for count
    std::size_t headerSize = b.w.offset(); // full header size
    for(Cursor cur = in.begin(); cur != in.end();) {
        // collect identical hash values
        Hash::hash_t hash = cur.readInt();
        ids.push_back(cur.readInt());
        while(cur != in.end()) {
            if(cur.peekInt() != hash)
                break;
            cur.skipInt();
            ids.push_back(cur.readInt());
        }

        // start new page?
        if(ENTRY_SIZE * ids.size() > b.w.remaining()) {
            if(headerSize + ENTRY_SIZE * ids.size() > PageWriter::PAGE_SIZE) {
                // too big for any page
                throw CastorException() << "Too many collisions in value hash table";
            }
            // flush page
            b.w.writeInt(count, countOffset);
            tb.endLeaf(last);
            count = 0;
            tb.beginLeaf();
            b.w.skip(SUBHEADER_SIZE);
        }

        for(unsigned id : ids) {
            b.w.writeInt(hash);
            b.w.writeInt(id);
            count++;
        }

        last = hash;
        ids.clear();
    }

    // flush last page
    b.w.writeInt(count, countOffset);
    tb.endLeaf(last);

    b.values.index = tb.constructTree();
}

////////////////////////////////////////////////////////////////////////////////
// Storing header

/**
 * Store the dictionary and write header
 *
 * @param b store builder
 */
void storeHeader(StoreBuilder& b) {
    b.w.seek(0);

    // Magic number
    b.w.write(Store::MAGIC, sizeof(Store::MAGIC));
    // Format version
    b.w.writeInt(Store::VERSION);

    // Triples count
    b.w.writeInt(b.triplesCount);

    // Triples raw table
    b.w.writeInt(b.triplesTable);

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

    // Strings
    b.w.writeInt(b.strings.count);
    b.w.writeInt(b.strings.begin);
    b.w.writeInt(b.strings.mapping);
    b.w.writeInt(b.strings.index);

    // Values
    b.w.writeInt(b.values.begin);
    b.w.writeInt(b.values.index);
    b.w.writeInt(b.values.eqClasses);
    for(Value::Category cat = Value::CAT_BLANK; cat <= Value::CATEGORIES; ++cat)
        b.w.writeInt(b.values.categories[cat]);

    b.w.flush();
}

} // end of anonymous namespace

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
    TempFile rawTriples(dbpath), rawStrings(dbpath), rawValues(dbpath);
    {
        librdf::RdfParser parser(syntax, rdfpath);
        RDFLoader loader(&rawTriples, &rawStrings, &rawValues);
        parser.parse(&loader);
    }
    rawTriples.close();
    rawStrings.close();
    rawValues.close();

    cout << "Building strings..." << endl;
    unsigned stringsCount;
    TempFile strings(dbpath), stringsEarlyMap(dbpath),
             stringsMap(dbpath), stringsHashes(dbpath);
    buildStrings(rawStrings, strings, stringsEarlyMap,
                 stringsMap, stringsHashes, stringsCount);
    strings.close();
    stringsEarlyMap.close();
    stringsMap.close();
    stringsHashes.close();

    cout << "Resolving string ids in values..." << endl;
    TempFile resolvedValues(dbpath);
    resolveStringIds(rawValues, resolvedValues, stringsEarlyMap);
    stringsEarlyMap.discard();
    resolvedValues.close();

    cout << "Building values..." << endl;
    TempFile values(dbpath), valuesEarlyMap(dbpath), valuesHashes(dbpath),
             valuesEqClasses(dbpath);
    Value::id_t categories[Value::CATEGORIES + 1];
    {
        MMapFile fStrings(strings.fileName().c_str()),
                 fMap(stringsMap.fileName().c_str());
        StringMapper resolver(fStrings.begin(), fMap.begin());
        buildValues(resolvedValues, values, valuesEarlyMap, valuesHashes,
                    valuesEqClasses, categories, resolver);
    }
    values.close();
    valuesEarlyMap.close();
    valuesHashes.close();
    valuesEqClasses.close();

    cout << "Resolving value ids in triples..." << endl;
    TempFile triples(dbpath);
    resolveIds(rawTriples, triples, valuesEarlyMap);
    valuesEarlyMap.discard();
    triples.close();


    StoreBuilder b(dbpath);
    b.w.flush(); // reserve page 0 for header
    memcpy(b.values.categories, categories, sizeof(categories));

    cout << "Storing triples..." << endl;
    storeTriples(b, triples);

    cout << "Storing strings..." << endl;
    storeStrings(b, strings, stringsMap, stringsHashes, stringsCount);

    cout << "Storing values..." << endl;
    storeValues(b, values, valuesHashes, valuesEqClasses);

    cout << "Storing header..." << endl;
    storeHeader(b);

    b.w.close();
    cout << "Done." << endl;
    return 0;
}
