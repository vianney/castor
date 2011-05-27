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

#include <string>
#include <iostream>
#include <sstream>

#include <set>

#include <cstdlib>
#include <cstring>
#include <cassert>
#include <unistd.h>

#include <sqlite3.h>
#include <raptor.h>

#include "librdf.h"
#include "model.h"

using namespace std;
using namespace castor;

////////////////////////////////////////////////////////////////////////////////
// Utilities

#define XSD_PREFIX "http://www.w3.org/2001/XMLSchema#"
#define XSD_PREFIX_LEN (sizeof(XSD_PREFIX) - 1)

ValueType get_type(char *uri) {
    if(uri == NULL || uri[0] == '\0')
        return VALUE_TYPE_PLAIN_STRING;

    if(strncmp(uri, XSD_PREFIX, XSD_PREFIX_LEN) != 0)
        return VALUE_TYPE_UNKOWN;

    char *fragment = &uri[XSD_PREFIX_LEN];
    for(ValueType t = VALUE_TYPE_FIRST_XSD; t <= VALUE_TYPE_LAST_XSD;
        t = (ValueType)(t+1)) {
        if(strcmp(fragment, &VALUETYPE_URIS[t][XSD_PREFIX_LEN]) == 0)
            return t;
    }
    return VALUE_TYPE_UNKOWN;
}

char* convertURI(raptor_uri *uri) {
    char *str = reinterpret_cast<char*>(raptor_uri_as_string(uri));
    char *result = new char[strlen(str) + 1];
    strcpy(result, str);
    return result;
}

////////////////////////////////////////////////////////////////////////////////
// Sqlite wrappers

class SqliteException : public std::exception {
    string msg;

public:
    SqliteException(sqlite3 *db) : msg(sqlite3_errmsg(db)) {}
    SqliteException(sqlite3_stmt *stmt) : msg(sqlite3_errmsg(sqlite3_db_handle(stmt))) {}
    SqliteException(string msg) : msg(msg) {}
    SqliteException(const char* msg) : msg(msg) {}
    SqliteException(const SqliteException &o) : msg(o.msg) {}
    ~SqliteException() throw() {}

    const char *what() const throw() { return msg.c_str(); }
};

class SqliteDb {
    sqlite3 *db;
public:
    SqliteDb(const char* path, bool append) throw(SqliteException) {
        if(sqlite3_open_v2(path, &db,
                           SQLITE_OPEN_READWRITE | (append ? 0 : SQLITE_OPEN_CREATE),
                           NULL) != SQLITE_OK)
            throw SqliteException("Unable to create database.");
    }
    ~SqliteDb() { sqlite3_close(db); }
    operator sqlite3*() { return db; }
    void execute(const char* sql) throw(SqliteException) {
        if(sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK)
            throw SqliteException(db);
    }
};

class SqliteStatement {
    sqlite3_stmt *stmt;
public:
    SqliteStatement(SqliteDb *db, const char* sql) throw(SqliteException) {
        if(sqlite3_prepare_v2(*db, sql, -1, &stmt, NULL) != SQLITE_OK)
            throw SqliteException(*db);
    }
    ~SqliteStatement() {
        sqlite3_finalize(stmt);
    }

    void bindStr(int col, const char *str) throw(SqliteException) {
        if(sqlite3_bind_text(stmt, col, str, -1, SQLITE_STATIC) != SQLITE_OK)
            throw SqliteException(stmt);
    }
    void bindInt(int col, int i) throw(SqliteException) {
        if(sqlite3_bind_int(stmt, col, i) != SQLITE_OK)
            throw SqliteException(stmt);
    }
    void bindFloat(int col, double f) throw(SqliteException) {
        if(sqlite3_bind_double(stmt, col, f) != SQLITE_OK)
            throw SqliteException(stmt);
    }
    void bindNull(int col) throw(SqliteException) {
        if(sqlite3_bind_null(stmt, col) != SQLITE_OK)
            throw SqliteException(stmt);
    }
    void bindVal(int col, ValueType type, const char *lex) throw(SqliteException) {
        if(type == VALUE_TYPE_BOOLEAN) {
            bindInt(col, strcmp(lex, "true") == 0 || strcmp(lex, "1") == 0 ? 1 : 0);
        } else if(type >= VALUE_TYPE_FIRST_INTEGER &&
                  type <= VALUE_TYPE_LAST_INTEGER) {
            bindInt(col, atoi(lex));
        } else if(type >= VALUE_TYPE_FIRST_FLOATING &&
                  type <= VALUE_TYPE_LAST_FLOATING) {
            bindFloat(col, atof(lex));
        } else {
            bindNull(col);
        }
    }

    void reset() throw(SqliteException) {
        if(sqlite3_reset(stmt) != SQLITE_OK)
            throw SqliteException(stmt);
    }

    bool next() throw(SqliteException) {
        switch(sqlite3_step(stmt)) {
        case SQLITE_ROW:
            return true;
        case SQLITE_DONE:
            return false;
        default:
            throw SqliteException(stmt);
        }
    }

    const char* getStr(int col) {
        return reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
    }
    const int getInt(int col) {
        return sqlite3_column_int(stmt, col);
    }
    const double getFloat(int col) {
        return sqlite3_column_double(stmt, col);
    }
};

////////////////////////////////////////////////////////////////////////////////
// Raptor parser wrapper

class RDFParseHandler {
public:
    virtual ~RDFParseHandler() {};
    virtual void parseTriple(raptor_statement* triple) = 0;
};

void stmt_handler(void* user_data, raptor_statement* triple) {
    static_cast<RDFParseHandler*>(user_data)->parseTriple(triple);
}

class ParseException : public std::exception {
    string msg;

public:
    ParseException(string msg) : msg(msg) {}
    ParseException(const char* msg) : msg(msg) {}
    ParseException(const ParseException &o) : msg(o.msg) {}
    ~ParseException() throw() {}

    const char *what() const throw() { return msg.c_str(); }
};

class RDFParser {
    raptor_parser *parser;
    raptor_uri *fileURI;
    unsigned char *fileURIstr;

public:
    RDFParser(const char *syntax, const char *path) {
        parser = raptor_new_parser(librdf::World::instance().raptor, syntax);
        if(parser == NULL)
            throw ParseException("Unable to create parser");
        fileURIstr = raptor_uri_filename_to_uri_string(path);
        fileURI = raptor_new_uri(librdf::World::instance().raptor, fileURIstr);
    }
    ~RDFParser() {
        raptor_free_parser(parser);
        raptor_free_uri(fileURI);
        raptor_free_memory(fileURIstr);
    }
    void parse(RDFParseHandler *handler) {
        raptor_parser_set_statement_handler(parser, handler, stmt_handler);
        raptor_parser_parse_file(parser, fileURI, NULL);
    }
};

////////////////////////////////////////////////////////////////////////////////
// Get values

class ConvertException : public std::exception {
    string msg;

public:
    ConvertException(string msg) : msg(msg) {}
    ConvertException(const char* msg) : msg(msg) {}
    ConvertException(const char* msg, int val) {
        ostringstream str;
        str << msg << " " << val;
        this->msg = str.str();
    }
    ConvertException(const ConvertException &o) : msg(o.msg) {}
    ~ConvertException() throw() {}

    const char *what() const throw() { return msg.c_str(); }
};

struct DereferenceLess {
    template <typename T>
    bool operator()(T a, T b) const { return *a < *b; }
};

typedef set<Value*, DereferenceLess> ValueSet;

class ValuesStore : public RDFParseHandler {
    SqliteDb *db;
    SqliteStatement sqlDatatype, sqlInsertDatatype;
    SqliteStatement sqlLanguage, sqlInsertLanguage;
    SqliteStatement sqlInsertVal;

    ValueSet values;

public:
    ValuesStore(SqliteDb *db) : db(db),
        sqlDatatype(db, "SELECT id FROM datatypes WHERE uri = ?1"),
        sqlInsertDatatype(db, "INSERT INTO datatypes (uri) VALUES (?1)"),
        sqlLanguage(db, "SELECT id FROM languages WHERE tag = ?1"),
        sqlInsertLanguage(db, "INSERT INTO languages (tag) VALUES (?1)"),
        sqlInsertVal(db,
            "INSERT INTO vals (type, lexical, language, value) "
            "    VALUES (?1, ?2, ?3, ?4)") {}

    int getCount() { return values.size(); }

    ValueType getDatatypeId(const char* uri) {
        sqlInsertDatatype.reset();
        sqlInsertDatatype.bindStr(1, uri);
        sqlInsertDatatype.next();
        sqlDatatype.reset();
        sqlDatatype.bindStr(1, uri);
        assert(sqlDatatype.next());
        return static_cast<ValueType>(sqlDatatype.getInt(0));
    }

    int getLanguageId(const char* tag) {
        sqlInsertLanguage.reset();
        sqlInsertLanguage.bindStr(1, tag);
        sqlInsertLanguage.next();
        sqlLanguage.reset();
        sqlLanguage.bindStr(1, tag);
        assert(sqlLanguage.next());
        return sqlLanguage.getInt(0);
    }

    void parseTriple(raptor_statement* triple) {
        insertValue(triple->subject);
        insertValue(triple->predicate);
        insertValue(triple->object);
    }

    void finish() {
        for(ValueSet::iterator it = values.begin(), end = values.end();
            it != end; ++it) {
            Value* val = *it;
            sqlInsertVal.reset();
            sqlInsertVal.bindInt(1, val->type);
            sqlInsertVal.bindStr(2, val->lexical);
            sqlInsertVal.bindInt(3, val->isPlain() ? val->language : 0);
            if(val->isBoolean())
                sqlInsertVal.bindInt(4, val->boolean);
            else if(val->isInteger())
                sqlInsertVal.bindInt(4, val->integer);
            else if(val->isFloating())
                sqlInsertVal.bindFloat(4, val->floating);
            else if(val->isDecimal())
                sqlInsertVal.bindFloat(4, val->decimal->getFloat());
            // TODO datetime
            else
                sqlInsertVal.bindNull(4);
            sqlInsertVal.next();
        }
    }

private:

    void insertValue(raptor_term *term) throw(ConvertException) {
        char *typeUri;
        Value *val = new Value;
        val->id = 0;
        switch(term->type) {
        case RAPTOR_TERM_TYPE_BLANK:
            val->type = VALUE_TYPE_BLANK;
            val->typeUri = NULL;
            val->lexical = new char[term->value.blank.string_len + 1];
            strcpy(val->lexical, reinterpret_cast<char*>(term->value.blank.string));
            val->addCleanFlag(VALUE_CLEAN_LEXICAL);
            break;
        case RAPTOR_TERM_TYPE_URI:
            val->fillIRI(convertURI(term->value.uri), true);
            break;
        case RAPTOR_TERM_TYPE_LITERAL:
            val->lexical = new char[term->value.literal.string_len + 1];
            strcpy(val->lexical, reinterpret_cast<char*>(term->value.literal.string));
            val->addCleanFlag(VALUE_CLEAN_LEXICAL);
            if(term->value.literal.datatype == NULL) {
                val->type = VALUE_TYPE_PLAIN_STRING;
                val->typeUri = NULL;
                if(term->value.literal.language == NULL ||
                   term->value.literal.language_len == 0) {
                    val->language = 0;
                    val->languageTag = NULL;
                } else {
                    val->languageTag = new char[term->value.literal.language_len + 1];
                    strcpy(val->languageTag, reinterpret_cast<char*>(term->value.literal.language));
                    val->addCleanFlag(VALUE_CLEAN_DATA);
                    val->language = getLanguageId(val->languageTag);
                }
            } else {
                typeUri = convertURI(term->value.literal.datatype);
                val->type = get_type(typeUri);
                if(val->type == VALUE_TYPE_UNKOWN) {
                    val->typeUri = typeUri;
                    val->addCleanFlag(VALUE_CLEAN_TYPE_URI);
                    val->type = getDatatypeId(val->typeUri);
                } else {
                    val->typeUri = VALUETYPE_URIS[val->type];
                    delete typeUri;
                }
                if(val->isBoolean()) {
                    val->boolean = strcmp(val->lexical, "true") == 0 ||
                            strcmp(val->lexical, "1") == 0;
                } else if(val->isInteger()) {
                    val->integer = atoi(val->lexical);
                } else if(val->isFloating()) {
                    val->floating = atof(val->lexical);
                } else if(val->isDecimal()) {
                    val->decimal = new XSDDecimal(val->lexical);
                    val->addCleanFlag(VALUE_CLEAN_DATA);
                } else if(val->isDateTime()) {
                    // TODO
                }
            }
            break;
        default:
            throw ConvertException("Unknown term type", term->type);
        }
        if(!values.insert(val).second)
            delete val;
    }
};


////////////////////////////////////////////////////////////////////////////////
// Store class

class AppendStore : public RDFParseHandler {
    SqliteDb *db;
    SqliteStatement sqlInsertStmt;
    SqliteStatement sqlInsertStmtUnkLang;
    SqliteStatement sqlInsertStmtUnkTypeLang;

    int count;

public:
    AppendStore(SqliteDb *db) : db(db),
        sqlInsertStmt(db,
            "INSERT INTO statements (subject, predicate, object)"
            "    SELECT s.id, p.id, o.id"
            "    FROM vals AS s, vals AS p, vals AS o"
            "    WHERE s.lexical = ?1 AND s.type = ?2 AND"
            "          p.lexical = ?3 AND p.type = ?4 AND"
            "          o.lexical = ?5 AND o.type = ?6 AND o.language = ?7"),
        sqlInsertStmtUnkLang(db,
            "INSERT INTO statements (subject, predicate, object)"
            "    SELECT s.id, p.id, o.id"
            "    FROM vals AS s, vals AS p, vals AS o,"
            "         languages ON languages.id = o.language"
            "    WHERE s.lexical = ?1 AND s.type = ?2 AND"
            "          p.lexical = ?3 AND p.type = ?4 AND"
            "          o.lexical = ?5 AND o.type = ?6 AND languages.tag = ?7;"),
        sqlInsertStmtUnkTypeLang(db,
            "INSERT INTO statements (subject, predicate, object)"
            "    SELECT s.id, p.id, o.id"
            "    FROM vals AS s, vals AS p, vals AS o,"
            "         datatypes ON datatypes.id = o.type,"
            "         languages ON languages.id = o.language"
            "    WHERE s.lexical = ?1 AND s.type = ?2 AND"
            "          p.lexical = ?3 AND p.type = ?4 AND"
            "          o.lexical = ?5 AND datatypes.uri = ?6 AND languages.tag = ?7;"),
        count(0) {}

    int getCount() { return count; }

    void parseTriple(raptor_statement* triple) {
        count++;
        if(count % 10000 == 0)
            cerr << '.';

        // Subject value
        ValueType ts;
        char *lexs;
        switch(triple->subject->type) {
        case RAPTOR_TERM_TYPE_BLANK:
            ts = VALUE_TYPE_BLANK;
            lexs = reinterpret_cast<char*>(triple->subject->value.blank.string);
            break;
        case RAPTOR_TERM_TYPE_URI:
            ts = VALUE_TYPE_IRI;
            lexs = reinterpret_cast<char*>(raptor_uri_as_string(triple->subject->value.uri));
            break;
        default:
            throw ConvertException("Unknown subject type", triple->subject->type);
        }

        // Predicate value
        if(triple->predicate->type != RAPTOR_TERM_TYPE_URI)
            throw ConvertException("Unknown predicate type", triple->predicate->type);
        char *lexp = reinterpret_cast<char*>(raptor_uri_as_string(triple->predicate->value.uri));

        // Object value and statement
        ValueType to;
        char *typeUri = NULL;
        char *lexo;
        int lang = 0;
        char *langTag = NULL;
        switch(triple->object->type) {
        case RAPTOR_TERM_TYPE_BLANK:
            to = VALUE_TYPE_BLANK;
            lexo = reinterpret_cast<char*>(triple->object->value.blank.string);
            break;
        case RAPTOR_TERM_TYPE_URI:
            to = VALUE_TYPE_IRI;
            lexo = reinterpret_cast<char*>(raptor_uri_as_string(triple->object->value.uri));
            break;
        case RAPTOR_TERM_TYPE_LITERAL:
            if(triple->object->value.literal.datatype != NULL)
                typeUri = reinterpret_cast<char*>(raptor_uri_as_string(triple->object->value.literal.datatype));
            to = get_type(typeUri);
            lexo = reinterpret_cast<char*>(triple->object->value.literal.string);
            if(triple->object->value.literal.language != NULL &&
               triple->object->value.literal.language[0] != '\0') {
                lang = -1;
                langTag = reinterpret_cast<char*>(triple->object->value.literal.language);
            }
            break;
        default:
            throw ConvertException("Unknown object type", triple->object->type);
        }

        if(to == VALUE_TYPE_UNKOWN) {
            sqlInsertStmtUnkTypeLang.reset();
            sqlInsertStmtUnkTypeLang.bindStr(1, lexs);
            sqlInsertStmtUnkTypeLang.bindInt(2, ts);
            sqlInsertStmtUnkTypeLang.bindStr(3, lexp);
            sqlInsertStmtUnkTypeLang.bindInt(4, VALUE_TYPE_IRI);
            sqlInsertStmtUnkTypeLang.bindStr(5, lexo);
            sqlInsertStmtUnkTypeLang.bindStr(6, typeUri);
            sqlInsertStmtUnkTypeLang.bindStr(7, langTag);
            sqlInsertStmtUnkTypeLang.next();
        } else if(lang == 0) {
            sqlInsertStmt.reset();
            sqlInsertStmt.bindStr(1, lexs);
            sqlInsertStmt.bindInt(2, ts);
            sqlInsertStmt.bindStr(3, lexp);
            sqlInsertStmt.bindInt(4, VALUE_TYPE_IRI);
            sqlInsertStmt.bindStr(5, lexo);
            sqlInsertStmt.bindInt(6, to);
            sqlInsertStmt.bindInt(7, lang);
            sqlInsertStmt.next();
        } else {
            sqlInsertStmtUnkLang.reset();
            sqlInsertStmtUnkLang.bindStr(1, lexs);
            sqlInsertStmtUnkLang.bindInt(2, ts);
            sqlInsertStmtUnkLang.bindStr(3, lexp);
            sqlInsertStmtUnkLang.bindInt(4, VALUE_TYPE_IRI);
            sqlInsertStmtUnkLang.bindStr(5, lexo);
            sqlInsertStmtUnkLang.bindInt(6, to);
            sqlInsertStmtUnkLang.bindStr(7, langTag);
            sqlInsertStmtUnkLang.next();
        }
    }
};

////////////////////////////////////////////////////////////////////////////////
// Main routine

int main(int argc, char* argv[]) {
    // Parse options
    bool force = false;
    bool append = false;
    const char *syntax = "rdfxml";
    int c;
    while((c = getopt(argc, argv, "s:fa")) != -1) {
        switch(c) {
        case 's':
            syntax = optarg;
            break;
        case 'f':
            force = true;
            break;
        case 'a':
            append = true;
            break;
        default:
            return 1;
        }
    }

    if(argc - optind != 2) {
        cout << "Usage: " << argv[0] << " [options] DB RDF" << endl;
        return 1;
    }
    char *dbpath = argv[optind++];
    char *rdfpath = argv[optind++];

    // Create database
    if(!append && access(dbpath, F_OK) == 0) {
        if(force) {
            if(unlink(dbpath)) {
                perror("createsqlite");
                return 2;
            }
        } else {
            cerr << "Database already exists. Exiting." << endl;
            return 1;
        }
    }
    SqliteDb db(dbpath, append);

    if(!append) {
        cout << "Creating database" << endl;
        db.execute("CREATE TABLE datatypes ("
                   "  id INTEGER PRIMARY KEY NOT NULL,"
                   "  uri TEXT UNIQUE ON CONFLICT IGNORE"
                   ");"
                   "CREATE TABLE languages ("
                   "  id INTEGER PRIMARY KEY NOT NULL,"
                   "  tag TEXT UNIQUE ON CONFLICT IGNORE"
                   ");"
                   "CREATE TABLE vals ("
                   "  id INTEGER PRIMARY KEY NOT NULL,"
                   "  type INTEGER NOT NULL REFERENCES datatypes(id),"
                   "  lexical TEXT,"
                   "  language INTEGER REFERENCES languages(id) DEFAULT 0,"
                   "  value,"
                   "  UNIQUE (type, lexical, language) ON CONFLICT IGNORE"
                   ");"
                   "CREATE TABLE statements ("
                   "  subject INTEGER NOT NULL REFERENCES vals(id),"
                   "  predicate INTEGER NOT NULL REFERENCES vals(id),"
                   "  object INTEGER NOT NULL REFERENCES vals(id),"
                   "  PRIMARY KEY (predicate, subject, object) ON CONFLICT IGNORE"
                   ");"
                   "CREATE INDEX statements_spo"
                   "    ON statements (subject, predicate, object);"
                   "CREATE INDEX statements_sop"
                   "    ON statements (subject, object, predicate);"
                   "CREATE INDEX statements_pos"
                   "    ON statements (predicate, object, subject);"
                   "CREATE INDEX statements_osp"
                   "    ON statements (object, subject, predicate);"
                   "CREATE INDEX statements_ops"
                   "    ON statements (object, predicate, subject);"
                   "INSERT INTO languages (id, tag) VALUES (0, '');");

        SqliteStatement stmt(&db,
                             "INSERT INTO datatypes (id, uri) VALUES (?, ?)");
        for(ValueType t = VALUE_TYPE_BLANK; t < VALUE_TYPE_FIRST_CUSTOM;
            t = (ValueType)(t+1)) {
            stmt.reset();
            stmt.bindInt(1, t);
            if(VALUETYPE_URIS[t] == NULL)
                stmt.bindNull(2);
            else
                stmt.bindStr(2, VALUETYPE_URIS[t]);
            stmt.next();
        }
    }

    db.execute("BEGIN TRANSACTION;");

    // Parse RDF dataset
    {
        cout << "Loading values" << endl;
        ValuesStore vals(&db);
        RDFParser parser(syntax, rdfpath);
        parser.parse(&vals);
        cout << "Writing values" << endl;
        vals.finish();
    }

    cout << "Loading triples" << endl;
    AppendStore store(&db);
    RDFParser parser(syntax, rdfpath);
    parser.parse(&store);

    // Finalize
    if(store.getCount() >= 10000)
        cerr << endl;
    cout << "Imported " << store.getCount() << " triples." << endl;

    cout << "Commiting" << endl;
    db.execute("COMMIT;");
    return 0;
}
