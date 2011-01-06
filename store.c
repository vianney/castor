/* This file is part of Castor
 *
 * Author: Vianney le Clément de Saint-Marcq <vianney.leclement@uclouvain.be>
 * Copyright (C) 2010 - UCLouvain
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

#include "store.h"

inline void store_close(Store* self) {
    self->close(self);
}
inline int store_value_count(Store* self) {
    return self->value_count(self);
}
inline Value* store_value_get(Store* self, int id) {
    return self->value_get(self, id);
}
//inline Value* store_value_create(Store* self, ValueType type, char* typeUri,
//                                 char* lexical, char* language) {
//    return self->value_create(self, type, typeUri, lexical, language);
//}
//inline bool store_statement_add(Store* self, Value* source, Value* predicate,
//                                Value* object) {
//    return self->statement_add(self, source, predicate, object);
//}
inline bool store_statement_query(Store* self, int source, int predicate,
                                  int object) {
    return self->statement_query(self, source, predicate, object);
}
inline bool store_statement_fetch(Store* self, Statement* stmt) {
    return self->statement_fetch(self, stmt);
}
inline bool store_statement_finalize(Store* self) {
    return self->statement_finalize(self);
}
