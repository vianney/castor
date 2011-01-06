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
#include <string.h>
#include <sqlite3.h>

#include "store_sqlite.h"

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
     * SQL statement currently executed
     */
    sqlite3_stmt* sql;
    /**
     * SQL statements for rdf statement queries
     */
    sqlite3_stmt* sqlStmt[8];
} SqliteStore;

////////////////////////////////////////////////////////////////////////////////
// Implementations

void sqlite_store_close(SqliteStore* self) {
    int i;

    for(i = 0; i < 8; i++)
        sqlite3_finalize(self->sqlStmt[i]);

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
        for(i = 0; i < self->nbValues; i++)
            free(self->values[i].lexical);
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

bool sqlite_store_statement_query(SqliteStore* self, int subject, int predicate,
                                  int object) {
    self->sql = self->sqlStmt[ (subject >= 0) |
                               (predicate >= 0) << 1 |
                               (object >= 0) << 2 ];
    if(sqlite3_reset(self->sql) != SQLITE_OK)
        goto error;

#define BIND(col, var) \
    if(var >= 0 && sqlite3_bind_int(self->sql, col, var+1) != SQLITE_OK) \
        goto error;

    BIND(1, subject)
    BIND(2, predicate)
    BIND(3, object)

#undef BIND

    return true;

error:
    self->sql = NULL;
    return false;
}

bool sqlite_store_statement_fetch(SqliteStore* self, Statement* stmt) {
    if(self->sql == NULL)
        return false;
    if(sqlite3_step(self->sql) != SQLITE_ROW)
        return false;
    if(stmt != NULL) {
        stmt->subject = sqlite3_column_int(self->sql, 0) - 1;
        stmt->predicate = sqlite3_column_int(self->sql, 1) - 1;
        stmt->object = sqlite3_column_int(self->sql, 2) - 1;
    }
    return true;
}

bool sqlite_store_statement_finalize(SqliteStore* self) {
    self->sql = NULL;
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
    for(i = 0; i < 8; i++)
        self->sqlStmt[i] = NULL;

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
        v->language = sqlite3_column_int(sql, 3);
        v->languageTag = self->languages[v->language];
        if(v->type >= VALUE_TYPE_FIRST_INTEGER &&
           v->type <= VALUE_TYPE_LAST_INTEGER)
            v->integer = sqlite3_column_int(sql, 4);
        else if(v->type >= VALUE_TYPE_FIRST_FLOATING &&
                v->type <= VALUE_TYPE_LAST_FLOATING)
            v->floating = sqlite3_column_double(sql, 4);
        //else if(v->type == VALUE_TYPE_DATETIME)
        // TODO: v->datetime
    }
    sqlite3_finalize(sql);

#define SQL(num, where) \
    if(sqlite3_prepare_v2(self->db, \
                          "SELECT subject, predicate, object FROM statements " \
                          where, \
                          -1, &self->sqlStmt[num], NULL) != SQLITE_OK) \
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

    self->sql = NULL;

    return (Store*) self;

cleansql:
    sqlite3_finalize(sql);
cleandb:
    for(i = 0; i < 8; i++) {
        if(self->sqlStmt[i] != NULL)
            sqlite3_finalize(self->sqlStmt[i]);
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
        for(i = 0; i < self->nbValues; i++)
            if(self->values[i].lexical != NULL)
                free(self->values[i].lexical);
        free(self->values);
    }
    free(self);
    return NULL;
}
