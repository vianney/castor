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

#include "mongoose.h"

#include <iostream>
#include <sstream>

#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <csignal>

#include "store.h"
#include "query.h"

using namespace std;
using namespace castor;

namespace {

////////////////////////////////////////////////////////////////////////////////
// Default parameters

static const char* DEFAULT_PORT = "8000";
static const char* PATH = "/sparql";

static constexpr size_t MAX_QUERY_LEN = 32768;
static constexpr size_t MAX_POST_LEN = MAX_QUERY_LEN * 2;

////////////////////////////////////////////////////////////////////////////////
// Global variables

static const char* progname;
static bool verbose;

////////////////////////////////////////////////////////////////////////////////
// HTTP handler

static void start_response(mg_connection* conn, unsigned status,
                           const char* content_type) {
    if(verbose)
        cout << status << endl;
    mg_printf(conn,
              "HTTP/1.0 %d OK\r\n"
              "Content-Type: %s\r\n"
              "\r\n",
              status, content_type);
}

static int send_error(mg_connection* conn, unsigned status, const char* body) {
    start_response(conn, status, "text/plain");
    mg_write(conn, body, strlen(body));
    return 1;
}

static int handler(mg_connection* conn) {
    const mg_request_info* req = mg_get_request_info(conn);
    Store* store = reinterpret_cast<Store*>(req->user_data);

    if(verbose)
        cout << req->request_method << " " << req->uri << " ";

    if(strcmp(req->uri, PATH) != 0)
        return send_error(conn, 404, "Not found.");

    char querystr[MAX_QUERY_LEN];
    int ret;
    if(strcmp(req->request_method, "GET") == 0) {
        ret = mg_get_var(req->query_string, strlen(req->query_string), "query",
                         querystr, sizeof(querystr));
    } else if(strcmp(req->request_method, "POST") == 0) {
        char data[MAX_POST_LEN];
        int len = mg_read(conn, data, sizeof(data));
        ret = mg_get_var(data, len, "query", querystr, sizeof(querystr));
    } else {
        return send_error(conn, 405, "Unsupported method.");
    }
    if(ret == -1)
        return send_error(conn, 400, "Need to specify query.");
    else if(ret == -2)
        return send_error(conn, 500, "Query too long.");

    try {
        Query query(store, querystr);
        start_response(conn, 200, "application/sparql-results+xml");
        // FIXME: escape strings
        mg_printf(conn,
                  "<?xml version=\"1.0\"?>\n"
                  "<sparql xmlns=\"http://www.w3.org/2005/sparql-results#\">\n"
                  "  <head>\n");
        for(unsigned i = 0; i < query.requested(); ++i) {
            mg_printf(conn, "    <variable name=\"%s\"/>\n",
                      query.variable(i)->name().c_str());
        }
        mg_printf(conn, "  </head>\n");
        if(query.requested() == 0) {
            query.next();
            mg_printf(conn, "  <boolean>%s</boolean>\n",
                      query.count() == 0 ? "false" : "true");
        } else {
            mg_printf(conn, "  <results distinct=\"%s\" ordered=\"%s\">\n",
                      query.isDistinct() ? "true" : "false",
                      query.orders().empty() ? "false" : "true");
            while(query.next()) {
                mg_printf(conn, "    <result>\n");
                for(unsigned i = 0; i < query.requested(); ++i) {
                    Variable* var = query.variable(i);
                    Value::id_t id = var->valueId();
                    if(id != 0) {
                        mg_printf(conn, "      <binding name=\"%s\">",
                                  var->name().c_str());
                        Value val = store->lookupValue(id);
                        val.ensureDirectStrings(*store);
                        switch(val.category()) {
                        case Value::CAT_BLANK:
                            mg_printf(conn, "<bnode>%s</bnode>",
                                      val.lexical().str());
                            break;
                        case Value::CAT_URI:
                            mg_printf(conn, "<uri>%s</uri>",
                                      val.lexical().str());
                            break;
                        case Value::CAT_SIMPLE_LITERAL:
                            mg_printf(conn, "<literal>%s</literal>",
                                      val.lexical().str());
                            break;
                        case Value::CAT_PLAIN_LANG:
                            mg_printf(conn,
                                      "<literal xml:lang=\"%s\">%s</literal>",
                                      val.language().str(),
                                      val.lexical().str());
                            break;
                        default:
                            mg_printf(conn,
                                      "<literal datatype=\"%s\">%s</literal>",
                                      val.datatypeLex().str(),
                                      val.lexical().str());
                        }
                        mg_printf(conn, "</binding>\n");
                    }
                }
                mg_printf(conn, "    </result>\n");
            }
            mg_printf(conn, "  </results>\n");
        }
        mg_printf(conn, "</sparql>");
    } catch(CastorException e) {
        return send_error(conn, 400, e.what());
    }
    return 1;
}

////////////////////////////////////////////////////////////////////////////////
// CLI

static void usage() {
    cout << "Usage: " << progname << " [options] -d DB" << endl;
    cout << endl << "Options:" << endl;
    cout << "  -d DB         Dataset to load" << endl;
    cout << "  -p PORT       Port to listen on (default: " << DEFAULT_PORT << ")" << endl;
    cout << "  -v            Be verbose" << endl;
    exit(1);
}

}

int main(int argc, char* argv[]) {
    progname = argv[0];
    int c;
    char* dbpath = nullptr;
    const char* port = DEFAULT_PORT;
    verbose = false;
    while((c = getopt(argc, argv, "d:p:v")) != -1) {
        switch(c) {
        case 'd': dbpath = optarg;                   break;
        case 'p': port = optarg;                     break;
        case 'v': verbose = true;                    break;
        default: usage();
        }
    }

    // Load database
    if(dbpath == nullptr)
        usage();
    if(verbose)
        cout << "Loading " << dbpath << "." << endl;
    Store store(dbpath);

    // Start HTTP server
    mg_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.begin_request = handler;

    const char* options[] = {"listening_ports", port,
                             "num_threads", "1",
                             nullptr};

    mg_context* ctx = mg_start(&callbacks, &store, options);
    if(ctx == nullptr)
        return 2;
    if(verbose)
        cout << "Listening on :" << port << "." << endl;

    // Wait for SIGINT or SIGTERM
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    sigprocmask(SIG_BLOCK, &set, nullptr);
    int sig;
    sigwait(&set, &sig);

    // Clean up
    if(verbose)
        cout << "Exiting." << endl;
    mg_stop(ctx);
    return 0;
}
