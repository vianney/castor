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
#include "bool.h"

namespace castor {

bool NotConstraint::propagate() {
    return (x_->contains(RDF_TRUE)  || y_->remove(RDF_FALSE)) &&
           (x_->contains(RDF_FALSE) || y_->remove(RDF_TRUE))  &&
           (y_->contains(RDF_TRUE)  || x_->remove(RDF_FALSE)) &&
           (y_->contains(RDF_FALSE) || x_->remove(RDF_TRUE));
}

bool AndConstraint::propagate() {
    if(!x_->contains(RDF_TRUE) || !y_->contains(RDF_TRUE))
        domcheck(b_->remove(RDF_TRUE));
    if(!x_->contains(RDF_FALSE) && !y_->contains(RDF_FALSE))
        domcheck(b_->remove(RDF_FALSE));
    if((!x_->contains(RDF_ERROR) && !y_->contains(RDF_ERROR)) ||
            (x_->bound() && x_->value() == RDF_FALSE) ||
            (y_->bound() && y_->value() == RDF_FALSE))
        domcheck(b_->remove(RDF_ERROR));

    if(!b_->contains(RDF_TRUE) && x_->bound() && x_->value() == RDF_TRUE)
        domcheck(y_->remove(RDF_TRUE));
    if(!b_->contains(RDF_TRUE) && y_->bound() && y_->value() == RDF_TRUE)
        domcheck(x_->remove(RDF_TRUE));
    if(!b_->contains(RDF_FALSE)) {
        domcheck(x_->remove(RDF_FALSE));
        domcheck(y_->remove(RDF_FALSE));
    }
    if(!b_->contains(RDF_ERROR) &&
            !x_->contains(RDF_FALSE) && !y_->contains(RDF_FALSE)) {
        domcheck(x_->remove(RDF_ERROR));
        domcheck(y_->remove(RDF_ERROR));
    }

    if(b_->bound() && b_->value() == RDF_FALSE) {
        if(!x_->contains(RDF_FALSE)) {
            domcheck(y_->bind(RDF_FALSE));
        } else if(!y_->contains(RDF_FALSE)) {
            domcheck(x_->bind(RDF_FALSE));
        }
    }
    if(b_->bound() && b_->value() == RDF_ERROR) {
        if(!x_->contains(RDF_ERROR)) {
            domcheck(y_->bind(RDF_ERROR));
        } else if(!y_->contains(RDF_ERROR)) {
            domcheck(x_->bind(RDF_ERROR));
        }
    }

    if(x_->bound() + y_->bound() + b_->bound() >= 2)
        done_ = true;
    return true;
}

bool OrConstraint::propagate() {
    if(!x_->contains(RDF_FALSE) || !y_->contains(RDF_FALSE))
        domcheck(b_->remove(RDF_FALSE));
    if(!x_->contains(RDF_TRUE) && !y_->contains(RDF_TRUE))
        domcheck(b_->remove(RDF_TRUE));
    if((!x_->contains(RDF_ERROR) && !y_->contains(RDF_ERROR)) ||
            (x_->bound() && x_->value() == RDF_TRUE) ||
            (y_->bound() && y_->value() == RDF_TRUE))
        domcheck(b_->remove(RDF_ERROR));

    if(!b_->contains(RDF_FALSE) && x_->bound() && x_->value() == RDF_FALSE)
        domcheck(y_->remove(RDF_FALSE));
    if(!b_->contains(RDF_FALSE) && y_->bound() && y_->value() == RDF_FALSE)
        domcheck(x_->remove(RDF_FALSE));
    if(!b_->contains(RDF_TRUE)) {
        domcheck(x_->remove(RDF_TRUE));
        domcheck(y_->remove(RDF_TRUE));
    }
    if(!b_->contains(RDF_ERROR) &&
            !x_->contains(RDF_TRUE) && !y_->contains(RDF_TRUE)) {
        domcheck(x_->remove(RDF_ERROR));
        domcheck(y_->remove(RDF_ERROR));
    }

    if(b_->bound() && b_->value() == RDF_TRUE) {
        if(!x_->contains(RDF_TRUE)) {
            domcheck(y_->bind(RDF_TRUE));
        } else if(!y_->contains(RDF_TRUE)) {
            domcheck(x_->bind(RDF_TRUE));
        }
    }
    if(b_->bound() && b_->value() == RDF_ERROR) {
        if(!x_->contains(RDF_ERROR)) {
            domcheck(y_->bind(RDF_ERROR));
        } else if(!y_->contains(RDF_ERROR)) {
            domcheck(x_->bind(RDF_ERROR));
        }
    }

    if(x_->bound() + y_->bound() + b_->bound() >= 2)
        done_ = true;
    return true;
}

}
