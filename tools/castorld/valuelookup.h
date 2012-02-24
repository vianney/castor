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
public:
    ValueLookup(TempFile* file_);
    ~ValueLookup();

    //! Non-copyable
    ValueLookup(const ValueLookup&) = delete;
    ValueLookup& operator=(const ValueLookup&) = delete;

    /**
     * Lookup a value. Generate an id if necessary and write the mapping
     * to the file.
     *
     * @param val the value the look for. Must have a lexical form
     * @return the id (!= 0)
     */
    uint64_t lookup(const Value& val);

private:
    static constexpr unsigned SIZE = 1009433;  //!< hash table size

    TempFile* file_;   //!< file for storing the mappings
    Value*    values_; //!< values seen so far
    uint64_t* ids_;    //!< ids for the strings
    uint64_t  next_;   //!< next id
};

}

#endif // CASTOR_TOOLS_CASTORLD_VALUELOOKUP_H
