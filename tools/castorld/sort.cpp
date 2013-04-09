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
#include "sort.h"

#include <vector>
#include <algorithm>
#include <cstring>

using namespace std;

namespace castor {

/**
 * Maximum amount of usable memory.
 * TODO: detect at runtime
 */
static constexpr std::size_t MEM_LIMIT = sizeof(void*) * (1 << 27);

namespace {

/**
 * A memory range
 */
struct Range {
    Cursor from, to;

    Range(Cursor from, Cursor to) : from(from), to(to) {}

    bool operator==(const Range& o) const {
        return ((to - from) == (o.to - o.from))
                && (memcmp(from.get(), o.from.get(), to-from) == 0);
    }
    bool operator !=(const Range& o) const { return !(*this == o); }

    /**
     * Write this item to a temporary file
     */
    void write(TempFile& out) const {
        out.write(from.get(), to - from);
    }
};

/**
 * Sort wrapper that colls the comparison function
 */
class CompareSorter {
public:
    CompareSorter(std::function<int(Cursor, Cursor)> compare) :
        compare_(compare) {}

    bool operator()(const Range& a, const Range& b) const {
       return compare_(a.from, b.from) < 0;
    }

private:
    std::function<int(Cursor, Cursor)> compare_; //!< Comparison function
};

/**
 * Spool items to disk
 *
 * @param out spool file
 * @param items items to write
 * @param eliminateDuplicates should we eliminate the duplicates?
 * @return the number of bytes written
 */
static unsigned spool(TempFile& out, const vector<Range>& items,
                      bool eliminateDuplicates) {
    unsigned len = 0;
    Range last(0, 0);
    for(Range r : items) {
        if(!eliminateDuplicates || last != r) {
            last = r;
            last.write(out);
            len += last.to - last.from;
        }
    }
    return len;
}

}

void FileSorter::sort(TempFile& in, TempFile& out,
                      std::function<void(Cursor&)> skip,
                      std::function<int(Cursor, Cursor)> compare,
                      bool eliminateDuplicates) {
    in.close();

    // Produce runs
    vector<Range> runs;
    TempFile intermediate(out.baseName());
    {
        MMapFile fin(in.fileName().c_str());
        Cursor cur = fin.begin(), limit = fin.end();
        Cursor ofs(0);
        while (cur < limit) {
            // Collect items
            vector<Range> items;
            Cursor begin = cur;
            while (cur < limit) {
                Cursor start = cur;
                skip(cur);
                items.push_back(Range(start, cur));

                // Memory Overflow?
                if(cur - begin + items.size()*sizeof(Range) > MEM_LIMIT)
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
        MMapFile tempIn(intermediate.fileName().c_str());
        for(Range& r : runs) {
            r.from += tempIn.begin().get() - static_cast<const unsigned char*>(nullptr);
            r.to   += tempIn.begin().get() - static_cast<const unsigned char*>(nullptr);
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
