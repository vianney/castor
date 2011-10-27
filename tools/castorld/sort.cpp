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
#include "sort.h"

#include <vector>
#include <algorithm>
#include <cstring>

using namespace std;

namespace castor {

/**
 * Maximum amount of usable memory.
 * TODO detect at runtime
 */
static const unsigned MEM_LIMIT = sizeof(void*) * (1 << 27);

namespace {

/**
 * A memory range
 */
struct Range {
    Cursor from, to;

    Range(Cursor from, Cursor to) : from(from),to(to) {}

    bool operator ==(const Range& o) const {
        return ((to - from) == (o.to - o.from))
                && (memcmp(from.get(), o.from.get(), to-from) == 0);
    }
    bool operator !=(const Range& o) const { return !(*this == o); }

    /**
     * Write this item to a temporary file
     */
    void write(TempFile &out) {
        out.write(to - from, reinterpret_cast<const char*>(from.get()));
    }
};

/**
 * Sort wrapper that colls the comparison function
 */
struct CompareSorter
{
    typedef int (*func)(Cursor,Cursor);
    const func compare; //!< Comparison function

    CompareSorter(func compare) : compare(compare) {}

    bool operator()(const Range& a, const Range& b) const {
       return compare(a.from, b.from) < 0;
    }
};

/**
 * Spool items to disk
 *
 * @param out spool file
 * @param items items to write
 * @param eliminateDuplicates should we eliminate the duplicates?
 * @return the number of bytes written
 */
static unsigned spool(TempFile &out, const vector<Range> &items,
                      bool eliminateDuplicates) {
    unsigned len = 0;
    Range last(0, 0);
    for (vector<Range>::const_iterator it=items.begin(), end=items.end();
         it != end; ++it) {
        if(!eliminateDuplicates || last != *it) {
            last = *it;
            last.write(out);
            len += last.to - last.from;
        }
    }
    return len;
}

}

void FileSorter::sort(TempFile &in, TempFile &out,
                      void (*skip)(Cursor &),
                      int (*compare)(Cursor, Cursor),
                      bool eliminateDuplicates) {
    in.close();

    // Produce runs
    vector<Range> runs;
    TempFile intermediate(out.getBaseName());
    {
        MMapFile fin(in.getFileName().c_str());
        Cursor cur = fin.begin(), limit = fin.end();
        Cursor ofs(0);
        while (cur < limit) {
            // Collect items
            vector<Range> items;
            Cursor maxCur = cur + MEM_LIMIT;
            while (cur < limit) {
                Cursor start = cur;
                skip(cur);
                items.push_back(Range(start, cur));

                // Memory Overflow?
                if((cur + (sizeof(Range)*items.size())) > maxCur)
                    break;
            }

            // Sort the run
            std::sort(items.begin(), items.end(), CompareSorter(compare));

            // Did everything fit?
            if(cur == limit && runs.empty()) {
                spool(out, items, eliminateDuplicates);
                break;
            }

            // No, spool to intermediate file
            unsigned len = spool(intermediate, items, eliminateDuplicates);
            runs.push_back(Range(ofs, ofs + len));
            ofs += len;
        }
    }
    intermediate.close();

    // Do we have to merge runs?
    if (!runs.empty()) {
        // Map the ranges
        MMapFile tempIn(intermediate.getFileName().c_str());
        for (vector<Range>::iterator it = runs.begin(), end=runs.end();
             it != end; ++it) {
            (*it).from += tempIn.begin().get() - static_cast<const unsigned char*>(0);
            (*it).to += tempIn.begin().get() - static_cast<const unsigned char*>(0);
        }

        // Sort the run heads
        std::sort(runs.begin(), runs.end(), CompareSorter(compare));

        // And merge them
        Range last(0,0);
        while (!runs.empty()) {
            // Write the first entry if no duplicate
            Cursor cur = runs.front().from;
            skip(cur);
            Range head(runs.front().from, cur);
            if(!eliminateDuplicates || last != head)
                head.write(out);
            last = head;

            // Update the first entry. First entry done?
            if((runs.front().from = head.to) == runs.front().to) {
                runs[0] = runs[runs.size() - 1];
                runs.pop_back();
            }

            // Check the heap condition
            unsigned pos = 0, size = runs.size();
            while(pos < size) {
                unsigned left = 2 * pos + 1,
                         right = left + 1;
                if(left >= size)
                    break;
                if(right < size) {
                    if(compare(runs[pos].from, runs[left].from) > 0) {
                        if(compare(runs[pos].from, runs[right].from) > 0) {
                            if(compare(runs[left].from, runs[right].from) < 0) {
                                std::swap(runs[pos], runs[left]);
                                pos = left;
                            } else {
                                std::swap(runs[pos], runs[right]);
                                pos = right;
                            }
                        } else {
                          std::swap(runs[pos], runs[left]);
                          pos=left;
                        }
                    } else if(compare(runs[pos].from, runs[right].from) > 0) {
                        std::swap(runs[pos], runs[right]);
                        pos = right;
                    } else {
                        break;
                    }
                } else {
                    if(compare(runs[pos].from, runs[left].from) > 0) {
                        std::swap(runs[pos], runs[left]);
                        pos = left;
                    } else {
                        break;
                    }
                }
            }
        }
    }

    out.close();
}

}