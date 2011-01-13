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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sqlite3.h>

#include "store_sqlite.h"

#define PRINT_DB_ERROR \
    fprintf(stderr, "castor sqlite error: %s\n", sqlite3_errmsg(self->db));

/**
 * Internal store structure.
 */
typedef struct {
    /**
     * Inherited public interface implementation
     */
    Store pub;
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
} SqliteStore;

////////////////////////////////////////////////////////////////////////////////
// Implementations

void sqlite_store_close(SqliteStore* self) {
    int i;

    sqlite3_finalize(self->sqlVal);
    sqlite3_finalize(self->sqlValUnkLang);
    sqlite3_finalize(self->sqlValUnkTypeLang);

    for(i = 0; i < 8; i++)
        sqlite3_finalize(self->sqlStmts[i]);

    sqlite3_close(self->db);
    if(self->datatypes != NULL) {
        for(i = VALUE_TYPE_FIRST_CUSTOM; i < self->nbDatatypes; i++)
            free(self->datatypes[i]);
        free(self->datatypes);
    }
    if(self->languages != NULL) {
        for(i = 0; i < self->nbLanguages; i++)
            free(self->languages[i]);
        free(self->languages);
    }
    if(self->values != NULL) {
        for(i = 0; i < self->nbValues; i++) {
            free(self->values[i].lexical);
            if(self->values[i].type == VALUE_TYPE_DECIMAL)
                free_xsddecimal(self->values[i].decimal);
        }
        free(self->values);
    }
    free(self);
}

int sqlite_store_value_count(SqliteStore* self) {
    return self->nbValues;
}

Value* sqlite_store_value_get(SqliteStore* self, int id) {
    if(id < 0 || id >= self->nbValues)
        return NULL;
    return &self->values[id];
}

int sqlite_store_value_get_id(SqliteStore* self, const ValueType type,
                              const char* typeUri, const char* lexical,
                              const char* language) {
    sqlite3_stmt *sql;

    if(type == VALUE_TYPE_UNKOWN)
        sql = self->sqlValUnkTypeLang;
    else if(language == NULL || language[0] == '\0')
        sql = self->sqlVal;
    else
        sql = self->sqlValUnkLang;

    if(language == NULL)
        language = "";

    if(sqlite3_reset(sql) != SQLITE_OK)
        goto error;

#define BIND_STR(col, s) \
    if(sqlite3_bind_text(sql, (col), (s), -1, SQLITE_STATIC) != SQLITE_OK) goto error;
#define BIND_INT(col, i) \
    if(sqlite3_bind_int(sql, (col), (i)) != SQLITE_OK) goto error;

    if(type == VALUE_TYPE_UNKOWN) {
        BIND_STR(1, typeUri);
    } else {
        BIND_INT(1, type);
    }
    BIND_STR(2, lexical);
    if(sql != self->sqlVal) {
        BIND_STR(3, language);
    }

#undef BIND_STR
#undef BIND_INT

    switch(sqlite3_step(sql)) {
    case SQLITE_ROW:
        return sqlite3_column_int(sql, 0) - 1;
    case SQLITE_DONE:
        return -1;
    default:
        goto error;
    }

error:
    PRINT_DB_ERROR
    return -1;
}

bool sqlite_store_statement_query(SqliteStore* self, int subject, int predicate,
                                  int object) {
    self->sqlStmt = self->sqlStmts[ (subject >= 0) |
                                    (predicate >= 0) << 1 |
                                    (object >= 0) << 2 ];
    if(sqlite3_reset(self->sqlStmt) != SQLITE_OK)
        goto error;

#define BIND(col, var) \
    if(var >= 0 && sqlite3_bind_int(self->sqlStmt, col, var+1) != SQLITE_OK) \
        goto error;

    BIND(1, subject)
    BIND(2, predicate)
    BIND(3, object)

#undef BIND

    return true;

error:
    PRINT_DB_ERROR
    self->sqlStmt = NULL;
    return false;
}

bool sqlite_store_statement_fetch(SqliteStore* self, Statement* stmt) {
    if(self->sqlStmt == NULL)
        return false;
    switch(sqlite3_step(self->sqlStmt)) {
    case SQLITE_ROW:
        if(stmt != NULL) {
            stmt->subject = sqlite3_column_int(self->sqlStmt, 0) - 1;
            stmt->predicate = sqlite3_column_int(self->sqlStmt, 1) - 1;
            stmt->object = sqlite3_column_int(self->sqlStmt, 2) - 1;
        }
        return true;
    case SQLITE_DONE:
        return false;
    default:
        PRINT_DB_ERROR
        return false;
    }
}

bool sqlite_store_statement_finalize(SqliteStore* self) {
    self->sqlStmt = NULL;
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Constructors

Store* sqlite_store_open(const char* filename) {
    SqliteStore *self;
    sqlite3_stmt *sql;
    int i, rc;
    const char *str;
    Value *v;

    self = (SqliteStore*) malloc(sizeof(SqliteStore));

    self->pub.close = (void (*)(Store*)) sqlite_store_close;
    self->pub.value_count = (int (*)(Store*)) sqlite_store_value_count;
    self->pub.value_get = (Value* (*)(Store*, int)) sqlite_store_value_get;
    self->pub.value_get_id = (int (*)(Store* self, const ValueType type,
                                      const char* typeUri, const char* lexical,
                                      const char* language))
                             sqlite_store_value_get_id;
    self->pub.statement_query = (bool (*)(Store*, int, int, int))
                                sqlite_store_statement_query;
    self->pub.statement_fetch = (bool (*)(Store*, Statement*))
                                sqlite_store_statement_fetch;
    self->pub.statement_finalize = (bool (*)(Store*))
                                   sqlite_store_statement_finalize;

    self->nbValues = 0;
    self->values = NULL;
    self->nbDatatypes = 0;
    self->datatypes = NULL;
    self->nbLanguages = 0;
    self->languages = NULL;
    self->sqlVal = NULL;
    self->sqlValUnkLang = NULL;
    self->sqlValUnkTypeLang = NULL;
    for(i = 0; i < 8; i++)
        self->sqlStmts[i] = NULL;

    if(sqlite3_open_v2(filename, &self->db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK)
        goto cleanself;

    // Populate datatypes
    if(sqlite3_prepare_v2(self->db,
                          "SELECT COUNT(*) FROM datatypes",
                          -1, &sql, NULL) != SQLITE_OK)
        goto cleandb;
    if(sqlite3_step(sql) != SQLITE_ROW)
        goto cleansql;
    self->nbDatatypes = sqlite3_column_int(sql, 0);
    sqlite3_finalize(sql);

    self->datatypes = (char**) calloc(self->nbDatatypes, sizeof(char*));
    for(i = 0; i < VALUE_TYPE_FIRST_CUSTOM; i++)
        self->datatypes[i] = VALUETYPE_URIS[i];

    if(sqlite3_prepare_v2(self->db,
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
        str = (const char*) sqlite3_column_text(sql, 1);
        self->datatypes[i] = (char*) malloc((strlen(str)+1) * sizeof(char));
        strcpy(self->datatypes[i], str);
    }
    sqlite3_finalize(sql);

    // Populate languages
    if(sqlite3_prepare_v2(self->db,
                          "SELECT COUNT(*) FROM languages",
                          -1, &sql, NULL) != SQLITE_OK)
        goto cleandb;
    if(sqlite3_step(sql) != SQLITE_ROW)
        goto cleansql;
    self->nbLanguages = sqlite3_column_int(sql, 0);
    sqlite3_finalize(sql);

    self->languages = (char**) calloc(self->nbLanguages, sizeof(char*));

    if(sqlite3_prepare_v2(self->db,
                          "SELECT id, tag FROM languages",
                          -1, &sql, NULL) != SQLITE_OK)
        goto cleandb;
    while((rc = sqlite3_step(sql)) != SQLITE_DONE) {
        if(rc != SQLITE_ROW)
            goto cleansql;
        i = sqlite3_column_int(sql, 0);
        str = (const char*) sqlite3_column_text(sql, 1);
        self->languages[i] = (char*) malloc((strlen(str)+1) * sizeof(char));
        strcpy(self->languages[i], str);
    }
    sqlite3_finalize(sql);

    // Populate values
    if(sqlite3_prepare_v2(self->db,
                          "SELECT COUNT(*) FROM vals",
                          -1, &sql, NULL) != SQLITE_OK)
        goto cleandb;
    if(sqlite3_step(sql) != SQLITE_ROW)
        goto cleansql;
    self->nbValues = sqlite3_column_int(sql, 0);
    sqlite3_finalize(sql);

    self->values = (Value*) calloc(self->nbValues, sizeof(Value));

    if(sqlite3_prepare_v2(self->db,
                          "SELECT id, type, lexical, language, value FROM vals",
                          -1, &sql, NULL) != SQLITE_OK)
        goto cleandb;
    while((rc = sqlite3_step(sql)) != SQLITE_DONE) {
        if(rc != SQLITE_ROW)
            goto cleansql;
        i = sqlite3_column_int(sql, 0) - 1;
        v = &self->values[i];
        v->id = i;
        v->type = sqlite3_column_int(sql, 1);
        v->typeUri = self->datatypes[v->type];
        str = (const char*) sqlite3_column_text(sql, 2);
        v->lexical = (char*) malloc((strlen(str)+1) * sizeof(char));
        strcpy(v->lexical, str);
        v->cleanup = VALUE_CLEAN_NOTHING;
        v->language = sqlite3_column_int(sql, 3);
        v->languageTag = self->languages[v->language];
        if(v->type == VALUE_TYPE_BOOLEAN)
            v->boolean = sqlite3_column_int(sql, 4);
        else if(IS_VALUE_TYPE_INTEGER(v->type))
            v->integer = sqlite3_column_int(sql, 4);
        else if(IS_VALUE_TYPE_FLOATING(v->type))
            v->floating = sqlite3_column_double(sql, 4);
        else if(v->type == VALUE_TYPE_DECIMAL) {
            v->decimal = new_xsddecimal();
            xsddecimal_set_string(v->decimal, v->lexical);
        }
        //else if(v->type == VALUE_TYPE_DATETIME)
        // TODO: v->datetime
    }
    sqlite3_finalize(sql);

#define SQL(var, sql) \
    if(sqlite3_prepare_v2(self->db, sql, -1, &var, NULL) != SQLITE_OK) \
        goto cleandb;

    SQL(self->sqlVal,
        "SELECT id FROM vals "
        "WHERE type = ?1 AND lexical = ?2 AND language = 0")
    SQL(self->sqlValUnkLang,
        "SELECT vals.id "
        "FROM vals, languages ON languages.id = vals.language "
        "WHERE type = ?1 AND lexical = ?2 AND languages.tag = ?3")
    SQL(self->sqlValUnkTypeLang,
        "SELECT vals.id "
        "FROM vals, "
        "     datatypes ON datatypes.id = vals.type, "
        "     languages ON languages.id = vals.language "
        "WHERE datatypes.uri = ?1 AND lexical = ?2 AND languages.tag = ?3")

#undef SQL

#define SQL(num, where) \
    if(sqlite3_prepare_v2(self->db, \
                          "SELECT subject, predicate, object FROM statements " \
                          where, \
                          -1, &self->sqlStmts[num], NULL) != SQLITE_OK) \
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

    self->sqlStmt = NULL;

    return (Store*) self;

cleansql:
    sqlite3_finalize(sql);
cleandb:
    PRINT_DB_ERROR
    if(self->sqlVal != NULL) sqlite3_finalize(self->sqlVal);
    if(self->sqlValUnkLang != NULL) sqlite3_finalize(self->sqlValUnkLang);
    if(self->sqlValUnkTypeLang != NULL) sqlite3_finalize(self->sqlValUnkTypeLang);
    for(i = 0; i < 8; i++) {
        if(self->sqlStmts[i] != NULL)
            sqlite3_finalize(self->sqlStmts[i]);
    }
    sqlite3_close(self->db);
cleanself:
    if(self->datatypes != NULL) {
        for(i = VALUE_TYPE_FIRST_CUSTOM; i < self->nbDatatypes; i++)
            if(self->datatypes[i] != NULL)
                free(self->datatypes[i]);
        free(self->datatypes);
    }
    if(self->languages != NULL) {
        for(i = 0; i < self->nbLanguages; i++)
            if(self->languages[i] != NULL)
                free(self->languages[i]);
        free(self->languages);
    }
    if(self->values != NULL) {
        for(i = 0; i < self->nbValues; i++) {
            if(self->values[i].lexical != NULL)
                free(self->values[i].lexical);
            if(self->values[i].type == VALUE_TYPE_DECIMAL)
                free_xsddecimal(self->values[i].decimal);
        }
        free(self->values);
    }
    free(self);
    return NULL;
}
