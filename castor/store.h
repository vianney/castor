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
#ifndef CASTOR_STORE_H
#define CASTOR_STORE_H

#include <sqlite3.h>
#include <string>
#include <exception>
#include "model.h"

namespace castor {

class StoreException : public std::exception {
    std::string msg;

public:
    StoreException(std::string msg) : msg(msg) {}
    StoreException(const char* msg) : msg(msg) {}
    StoreException(const StoreException &o) : msg(o.msg) {}
    ~StoreException() throw() {}

    const char *what() const throw() { return msg.c_str(); }
};

/**
 * Store interface. Implementations may extend this structure.
 */
class Store {
public:
    /**
     * Open an existing SQLite store.
     *
     * @param filename location of the store
     * @throws StoreException on database error
     */
    Store(const char* filename) throw(StoreException);
    ~Store();

    /**
     * Number of values in the store. The ids of the values will always be
     * between 1 and the returned value included.
     *
     * @return the number of values in the store or -1 if error
     */
    int getValueCount() { return nbValues; }

    /**
     * Get a value from the store
     *
     * @param id identifier of the value (within range 1..getValueCount())
     * @return the value corresponding to id or NULL if error
     */
    Value* getValue(int id) {
        if(id <= 0 || id > nbValues)
            return NULL;
        return &values[id-1];
    }

    /**
     * Get the id of a value from the store.
     *
     * @param type datatype of the value
     * @param typeUri URI of the datatype if type is VALUE_TYPE_UNKOWN
     * @param lexical lexical form
     * @param language language tag or NULL if none
     * @return the id of the value or 0 if not found
     * @throws StoreException on database error
     */
    int getValueId(const Value::Type type, const char* typeUri,
                   const char* lexical, const char* language)
            throw(StoreException);

    /**
     * Query the store for statements. Use fetchStatement to get the results.
     *
     * @param subject subject id or -1 for wildcard
     * @param predicate predicate id or -1 for wildcard
     * @param object object id or -1 for wildcard
     * @throws StoreException on database error
     */
    void queryStatements(int subject, int predicate, int object)
            throw(StoreException);

    /**
     * Fetch the next result statement from the query initiated by
     * queryStatement().
     *
     * @param[out] stmt structure in which to write the result or NULL to ignore
     * @return true if stmt contains the next result, false if there are no more
     *         results
     * @throws StoreException on database error
     */
    bool fetchStatement(Statement *stmt) throw(StoreException);

private:
    /**
     * Check the result of a SQLite command and throw an exception if failed.
     *
     * @param rc result of the SQLite command
     * @throws StoreException on database error
     */
    void checkDbResult(int rc) throw(StoreException) {
        if(rc != SQLITE_OK) throw StoreException(sqlite3_errmsg(db));
    }

private:
    /**
     * Handle to the database
     */
    sqlite3* db;
    /**
     * Number of values in the store
     */
    int nbValues;
    /**
     * Value cache
     */
    Value* values;
    /**
     * Number of datatypes
     */
    int nbDatatypes;
    /**
     * Datatypes cache
     */
    char** datatypes;
    /**
     * Number of languages
     */
    int nbLanguages;
    /**
     * Languages cache
     */
    char** languages;

    /**
     * SQL for value id retrieval. The first one is for known datatype id and
     * language 0, the second is for known datatype id and unknown language,
     * and the last one for unknown datatype and language id.
     */
    sqlite3_stmt *sqlVal, *sqlValUnkLang, *sqlValUnkTypeLang;
    /**
     * SQL for rdf statements currently executed
     */
    sqlite3_stmt* sqlStmt;
    /**
     * SQL for rdf statement queries
     */
    sqlite3_stmt* sqlStmts[8];
};

}

#endif // CASTOR_STORE_H
