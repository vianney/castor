#include <stdlib.h>
#include <sqlite3.h>

#include "store_sqlite.h"

/**
 * Linked array-list of cached values
 */
typedef struct TValueCache ValueCache;
struct TValueCache {
    /**
     * Next block of cache
     */
    ValueCache* next;
    /**
     * Size of this block
     */
    int size;
    Value* values;
};

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
    ValueCache* cache;
} SqliteStore;

////////////////////////////////////////////////////////////////////////////////
// Implementations

void sqlite_store_free(SqliteStore* self) {
    sqlite3_close(self->db);
    free(self);
}

int sqlite_store_value_count(SqliteStore* self) {
    sqlite3_stmt *sql;

    if(self->nbValues != -1)
        return self->nbValues;

    if(sqlite3_prepare_v2(self->db,
                          "SELECT COUNT(*) FROM values",
                          -1, &sql, NULL) != SQLITE_OK)
        return -1;
    if(sqlite3_step(sql) != SQLITE_ROW) {
        sqlite3_finalize(sql);
        return -1;
    }
    self->nbValues = sqlite3_column_int(sql, 0);
    sqlite3_finalize(sql);
    return self->nbValues;
}

Value* sqlite_store_value_get(SqliteStore* self, int id) {
    // TODO
}

Value* sqlite_store_value_create(SqliteStore* self, ValueType type,
                                 char* lexical, char* language) {
    // TODO
}

bool sqlite_store_statement_add(SqliteStore* self, Value* source,
                                Value* predicate, Value* object) {
    // TODO
}

bool sqlite_store_statement_query(SqliteStore* self, int source, int predicate,
                                  int object) {
    // TODO
}

bool sqlite_store_statement_fetch(SqliteStore* self, Statement* stmt) {
    // TODO
}

bool sqlite_store_statement_finalize(SqliteStore* self) {
    // TODO
}

////////////////////////////////////////////////////////////////////////////////
// Constructors

/**
 * Create a new Store structure and fill in the public interface
 *
 * @return the new store structure
 */
SqliteStore* sqlite_store_new() {
    SqliteStore* self;

    self = (SqliteStore*) malloc(sizeof(SqliteStore));
    self->pub.free = (void (*)(Store*)) sqlite_store_free;
    self->pub.value_count = (int (*)(Store*)) sqlite_store_value_count;
    self->pub.value_get = (Value* (*)(Store*, int)) sqlite_store_value_get;
    self->pub.value_create = (Value* (*)(Store*, ValueType, char*, char*))
                             sqlite_store_value_create;
    self->pub.statement_add = (bool (*)(Store*, Value*, Value*, Value*))
                              sqlite_store_statement_add;
    self->pub.statement_query = (bool (*)(Store*, int, int, int))
                                sqlite_store_statement_query;
    self->pub.statement_fetch = (bool (*)(Store*, Statement*))
                                sqlite_store_statement_fetch;
    self->pub.statement_finalize = (bool (*)(Store*))
                                   sqlite_store_statement_finalize;
}

/**
 * Common initialization of the store structure after the database connection
 * has been initialized.
 *
 * @param self the store instance
 * @return true if all went well, false if error
 */
bool sqlite_store_common_init(SqliteStore *self) {
    // TODO
    return true;
}

Store* sqlite_store_create(const char* filename) {
    SqliteStore* self;
    ValueType t;
    sqlite3_stmt *sql;

    self = sqlite_store_new();
    if(sqlite3_open_v2(filename, &self->db,
                       SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK)
        goto cleanself;

    if(sqlite3_exec(self->db,
                    "CREATE TABLE datatypes ("
                    "  id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
                    "  uri TEXT UNIQUE,"
                    ");"
                    "CREATE TABLE languages ("
                    "  id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
                    "  value CHAR(5) UNIQUE"
                    ");"
                    "CREATE TABLE values ("
                    "  id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
                    "  type INTEGER NOT NULL REFERENCES datatypes(id),"
                    "  lexical TEXT,"
                    "  language INTEGER REFERENCES languages(id),"
                    "  value,"
                    "  UNIQUE (type, lexical, language)"
                    ");"
                    "CREATE TABLE statements ("
                    "  subject INTEGER NOT NULL REFERENCES values(id),"
                    "  predicate INTEGER NOT NULL REFERENCES values(id),"
                    "  object INTEGER NOT NULL REFERENCES values(id),"
                    "  PRIMARY KEY (predicate, subject, object)"
                    ");",
                    NULL, NULL, NULL) != SQLITE_OK)
        goto cleandb;

    if(sqlite3_prepare_v2(self->db,
                          "INSERT INTO datatypes (id, uri) VALUES (?, ?)",
                          -1, &sql, NULL) != SQLITE_OK)
        goto cleandb;
    for(t = VALUE_TYPE_BLANK; t < VALUE_TYPE_FIRST_CUSTOM; t++) {
        if(sqlite3_reset(sql) != SQLITE_OK)
            goto cleansql;
        if(sqlite3_bind_int(sql, 1, t) != SQLITE_OK)
            goto cleansql;
        if(VALUETYPE_URIS[t] == NULL)
            if(sqlite3_bind_null(sql, 2) != SQLITE_OK)
                goto cleansql;
        else
            if(sqlite3_bind_text(sql, 2, VALUETYPE_URIS[t], -1, SQLITE_STATIC) != SQLITE_OK)
                goto cleansql;
        if(sqlite3_step(sql) != SQLITE_DONE)
            goto cleansql;
    }
    sqlite3_finalize(sql);

    self->nbValues = 0;

    return self;

cleansql:
    sqlite3_finalize(sql);
cleandb:
    sqlite3_close(self->db);
cleanself:
    free(self);
    return NULL;
}

Store* sqlite_store_open(const char* filename) {
    SqliteStore* self;

    self = sqlite_store_new();
    if(sqlite3_open_v2(filename, &self->db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        free(self);
        return NULL;
    }

    self->nbValues = -1; // lazy initialization

    return self;
}
