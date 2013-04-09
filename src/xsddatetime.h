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
#ifndef CASTOR_XSDDATETIME_H
#define CASTOR_XSDDATETIME_H

#include "librdfwrapper.h"

namespace castor {

/**
 * Wrapper for an xsd:dateTime value.
 */
class XSDDateTime {
public:
    /**
     * Construct a new datetime from a lexical form.
     */
    XSDDateTime(const char* lexical) {
        val_ = rasqal_new_xsd_datetime(librdf::World::instance().rasqal,
                                       lexical);
    }

    ~XSDDateTime() {
        rasqal_free_xsd_datetime(val_);
    }

    /**
     * @return lexical form of the datetime
     */
    std::string getString() const {
        size_t n;
        char* str = rasqal_xsd_datetime_to_counted_string(val_, &n);
        std::string result = std::string(str, n);
        rasqal_free_memory(str);
        return result;
    }

    // FIXME: handle incomparible results

    /**
     * Compare two datetimes
     *
     * @param o second datetime
     * @return <0 if this < o, 0 if this == o and >0 if this > o
     */
    int compare(const XSDDateTime& o) const { return rasqal_xsd_datetime_compare2(val_, o.val_, nullptr); }

    bool operator==(const XSDDateTime& o) const { return  rasqal_xsd_datetime_equals2(val_, o.val_, nullptr); }
    bool operator!=(const XSDDateTime& o) const { return !rasqal_xsd_datetime_equals2(val_, o.val_, nullptr); }
    bool operator< (const XSDDateTime& o) const { return compare(o) <  0; }
    bool operator> (const XSDDateTime& o) const { return compare(o) >  0; }
    bool operator<=(const XSDDateTime& o) const { return compare(o) <= 0; }
    bool operator>=(const XSDDateTime& o) const { return compare(o) >= 0; }

private:
    rasqal_xsd_datetime* val_;
};

}

#endif // CASTOR_XSDDATETIME_H
