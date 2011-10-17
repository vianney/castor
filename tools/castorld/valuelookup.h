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

#ifndef CASTOR_TOOLS_CASTORLD_VALUELOOKUP_H
#define CASTOR_TOOLS_CASTORLD_VALUELOOKUP_H

#include <cstdint>

#include "model.h"

#include "tempfile.h"

namespace castor {

/**
 * Lookup cache for early string aggregation
 */
class ValueLookup {
    static const unsigned SIZE = 1009433;  //!< hash table size

    TempFile *file; //!< file for storing the mappings

    Value *values;  //!< strings seen so far
    uint64_t *ids;  //!< ids for the strings
    uint64_t next;  //!< next id

public:
    ValueLookup(TempFile *file);
    ~ValueLookup();

    /**
     * Lookup a value. Generate an id if necessary and write the mapping
     * to the file.
     *
     * @param val the value the look for. Must have a lexical form
     * @return the id (!= 0)
     */
    uint64_t lookup(const Value &val);
};

}

#endif // CASTOR_TOOLS_CASTORLD_VALUELOOKUP_H
