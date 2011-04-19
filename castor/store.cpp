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

#include <cstring>
#include "store.h"

namespace castor {

Store::Store(const char* filename) throw(StoreException) {
    sqlite3_stmt *sql;
    int i, rc;
    const char *str;
    Value *v;
    std::string error;

    nbValues = 0;
    values = NULL;
    nbDatatypes = 0;
    datatypes = NULL;
    nbLanguages = 0;
    languages = NULL;
    sqlVal = NULL;
    sqlValUnkLang = NULL;
    sqlValUnkTypeLang = NULL;
    for(i = 0; i < 8; i++)
        sqlStmts[i] = NULL;

    if(sqlite3_open_v2(filename, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK)
        goto cleanself;

    // Populate datatypes
    if(sqlite3_prepare_v2(db,
                          "SELECT COUNT(*) FROM datatypes",
                          -1, &sql, NULL) != SQLITE_OK)
        goto cleandb;
    if(sqlite3_step(sql) != SQLITE_ROW)
        goto cleansql;
    nbDatatypes = sqlite3_column_int(sql, 0);
    sqlite3_finalize(sql);

    datatypes = new char*[nbDatatypes];
    for(i = 0; i < VALUE_TYPE_FIRST_CUSTOM; i++)
        datatypes[i] = VALUETYPE_URIS[i];
    for(i = VALUE_TYPE_FIRST_CUSTOM; i < nbDatatypes; i++)
        datatypes[i] = NULL;

    if(sqlite3_prepare_v2(db,
                          "SELECT id, uri FROM datatypes "
                          "WHERE id >= ?",
                          -1, &sql, NULL) != SQLITE_OK)
        goto cleandb;
    if(sqlite3_bind_int(sql, 1, VALUE_TYPE_FIRST_CUSTOM) != SQLITE_OK)
        goto cleansql;
    while((rc = sqlite3_step(sql)) != SQLITE_DONE) {
        if(rc != SQLITE_ROW)
            goto cleansql;
        i = sqlite3_column_int(sql, 0);
        str = reinterpret_cast<const char*>(sqlite3_column_text(sql, 1));
        datatypes[i] = new char[strlen(str) + 1];
        strcpy(datatypes[i], str);
    }
    sqlite3_finalize(sql);

    // Populate languages
    if(sqlite3_prepare_v2(db,
                          "SELECT COUNT(*) FROM languages",
                          -1, &sql, NULL) != SQLITE_OK)
        goto cleandb;
    if(sqlite3_step(sql) != SQLITE_ROW)
        goto cleansql;
    nbLanguages = sqlite3_column_int(sql, 0);
    sqlite3_finalize(sql);

    languages = new char*[nbLanguages];
    for(i = 0; i < nbLanguages; i++)
        languages[i] = NULL;

    if(sqlite3_prepare_v2(db,
                          "SELECT id, tag FROM languages",
                          -1, &sql, NULL) != SQLITE_OK)
        goto cleandb;
    while((rc = sqlite3_step(sql)) != SQLITE_DONE) {
        if(rc != SQLITE_ROW)
            goto cleansql;
        i = sqlite3_column_int(sql, 0);
        str = reinterpret_cast<const char*>(sqlite3_column_text(sql, 1));
        languages[i] = new char[strlen(str) + 1];
        strcpy(languages[i], str);
    }
    sqlite3_finalize(sql);

    // Populate values
    if(sqlite3_prepare_v2(db,
                          "SELECT COUNT(*) FROM vals",
                          -1, &sql, NULL) != SQLITE_OK)
        goto cleandb;
    if(sqlite3_step(sql) != SQLITE_ROW)
        goto cleansql;
    nbValues = sqlite3_column_int(sql, 0);
    sqlite3_finalize(sql);

    values = new Value[nbValues];

    if(sqlite3_prepare_v2(db,
                          "SELECT id, type, lexical, language, value FROM vals",
                          -1, &sql, NULL) != SQLITE_OK)
        goto cleandb;
    while((rc = sqlite3_step(sql)) != SQLITE_DONE) {
        if(rc != SQLITE_ROW)
            goto cleansql;
        i = sqlite3_column_int(sql, 0);
        v = &values[i-1];
        v->id = i;
        v->type = (ValueType) sqlite3_column_int(sql, 1);
        v->typeUri = datatypes[v->type];
        str = reinterpret_cast<const char*>(sqlite3_column_text(sql, 2));
        v->lexical = new char[strlen(str) + 1];
        strcpy(v->lexical, str);
        v->cleanup = VALUE_CLEAN_LEXICAL;
        if(v->isPlain()) {
            v->language = sqlite3_column_int(sql, 3);
            v->languageTag = languages[v->language];
        } else if(v->isBoolean()) {
            v->boolean = sqlite3_column_int(sql, 4);
        } else if(v->isInteger()) {
            v->integer = sqlite3_column_int(sql, 4);
        } else if(v->isFloating()) {
            v->floating = sqlite3_column_double(sql, 4);
        } else if(v->isDecimal()) {
            v->decimal = new XSDDecimal(v->lexical);
            v->addCleanFlag(VALUE_CLEAN_DATA);
        }
        //else if(v->isDateTime())
        // TODO: v->datetime
    }
    sqlite3_finalize(sql);

#define SQL(var, sql) \
    if(sqlite3_prepare_v2(db, sql, -1, &var, NULL) != SQLITE_OK) \
        goto cleandb;

    SQL(sqlVal,
        "SELECT id FROM vals "
        "WHERE type = ?1 AND lexical = ?2 AND language = 0")
    SQL(sqlValUnkLang,
        "SELECT vals.id "
        "FROM vals, languages ON languages.id = vals.language "
        "WHERE type = ?1 AND lexical = ?2 AND languages.tag = ?3")
    SQL(sqlValUnkTypeLang,
        "SELECT vals.id "
        "FROM vals, "
        "     datatypes ON datatypes.id = vals.type, "
        "     languages ON languages.id = vals.language "
        "WHERE datatypes.uri = ?1 AND lexical = ?2 AND languages.tag = ?3")

#undef SQL

#define SQL(num, where) \
    if(sqlite3_prepare_v2(db, \
                          "SELECT subject, predicate, object FROM statements " \
                          where, \
                          -1, &sqlStmts[num], NULL) != SQLITE_OK) \
        goto cleandb;

    SQL(0, "")
    SQL(1, "WHERE subject = ?1")
    SQL(2, "WHERE predicate = ?2")
    SQL(3, "WHERE subject = ?1 AND predicate = ?2")
    SQL(4, "WHERE object = ?3")
    SQL(5, "WHERE subject = ?1 AND object = ?3")
    SQL(6, "WHERE predicate = ?2 AND object = ?3")
    SQL(7, "WHERE subject = ?1 AND predicate = ?2 AND object = ?3")

#undef SQL

    sqlStmt = NULL;

    return;

cleansql:
    sqlite3_finalize(sql);
cleandb:
    error.append(sqlite3_errmsg(db));
    if(sqlVal) sqlite3_finalize(sqlVal);
    if(sqlValUnkLang) sqlite3_finalize(sqlValUnkLang);
    if(sqlValUnkTypeLang) sqlite3_finalize(sqlValUnkTypeLang);
    for(i = 0; i < 8; i++) {
        if(sqlStmts[i])
            sqlite3_finalize(sqlStmts[i]);
    }
    sqlite3_close(db);
cleanself:
    if(datatypes) {
        for(i = VALUE_TYPE_FIRST_CUSTOM; i < nbDatatypes; i++)
            if(datatypes[i])
                delete [] datatypes[i];
        delete [] datatypes;
    }
    if(languages) {
        for(i = 0; i < nbLanguages; i++)
            if(languages[i])
                delete [] languages[i];
        delete [] languages;
    }
    if(values)
        delete [] values;
    throw StoreException(error);
}

Store::~Store() {
    sqlite3_finalize(sqlVal);
    sqlite3_finalize(sqlValUnkLang);
    sqlite3_finalize(sqlValUnkTypeLang);

    for(int i = 0; i < 8; i++)
        sqlite3_finalize(sqlStmts[i]);

    sqlite3_close(db);
    if(datatypes) {
        for(int i = VALUE_TYPE_FIRST_CUSTOM; i < nbDatatypes; i++)
            delete [] datatypes[i];
        delete [] datatypes;
    }
    if(languages) {
        for(int i = 0; i < nbLanguages; i++)
            delete [] languages[i];
        delete [] languages;
    }
    if(values)
        delete [] values;
}

int Store::getValueId(const ValueType type, const char *typeUri,
                      const char *lexical, const char *language)
            throw(StoreException) {
    sqlite3_stmt *sql;
    if(type == VALUE_TYPE_UNKOWN)
        sql = sqlValUnkTypeLang;
    else if(language == NULL || language[0] == '\0')
        sql = sqlVal;
    else
        sql = sqlValUnkLang;

    if(language == NULL)
        language = "";

    checkDbResult(sqlite3_reset(sql));

#define BIND_STR(col, s) \
    checkDbResult(sqlite3_bind_text(sql, (col), (s), -1, SQLITE_STATIC));
#define BIND_INT(col, i) \
    checkDbResult(sqlite3_bind_int(sql, (col), (i)));

    if(type == VALUE_TYPE_UNKOWN) {
        BIND_STR(1, typeUri);
    } else {
        BIND_INT(1, type);
    }
    BIND_STR(2, lexical);
    if(sql != sqlVal) {
        BIND_STR(3, language);
    }

#undef BIND_STR
#undef BIND_INT

    switch(sqlite3_step(sql)) {
    case SQLITE_ROW:
        return sqlite3_column_int(sql, 0);
    case SQLITE_DONE:
        return 0;
    default:
        throw StoreException(sqlite3_errmsg(db));
    }
}

void Store::queryStatements(int subject, int predicate, int object)
            throw(StoreException) {
    sqlStmt = sqlStmts[ (subject >= 0) |
                        (predicate >= 0) << 1 |
                        (object >= 0) << 2 ];
    checkDbResult(sqlite3_reset(sqlStmt));

#define BIND(col, var) \
    if(var >= 0) checkDbResult(sqlite3_bind_int(sqlStmt, col, var));

    BIND(1, subject)
    BIND(2, predicate)
    BIND(3, object)

#undef BIND
}

bool Store::fetchStatement(Statement *stmt) throw(StoreException) {
    if(sqlStmt == NULL)
        return false;
    switch(sqlite3_step(sqlStmt)) {
    case SQLITE_ROW:
        if(stmt) {
            stmt->subject = sqlite3_column_int(sqlStmt, 0);
            stmt->predicate = sqlite3_column_int(sqlStmt, 1);
            stmt->object = sqlite3_column_int(sqlStmt, 2);
        }
        return true;
    case SQLITE_DONE:
        return false;
    default:
        throw StoreException(sqlite3_errmsg(db));
    }
}

}
