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

#include "defs.h"
#include "model.h"
#include "store.h"
#include "query.h"

#include "stores/store_sqlite.h"

#define BUFFER_SIZE 1024

int main(int argc, char* argv[]) {
    char *dbpath, *rqpath;
    FILE *f;
    char *queryString, buffer[BUFFER_SIZE];
    size_t read;
    int queryLen;
    Store *store;
    Query *query;

    if(argc < 2 || argc > 3) {
        printf("Usage: %s DB [QUERY]\n", argv[0]);
        return 1;
    }
    dbpath = argv[1];
    rqpath = argc > 2 ? argv[2] : NULL;

    queryString = NULL;
    queryLen = 0;
    f = rqpath == NULL ? stdin : fopen(rqpath, "r");
    if(f == NULL) {
        perror("castor");
        return 2;
    }
    while((read = fread(buffer, sizeof(char), BUFFER_SIZE, f)) > 0) {
        queryString = (char*) realloc(queryString, (queryLen + read + 1) * sizeof(char));
        memcpy(queryString + queryLen, buffer, read);
        queryLen += read;
    }
    if(ferror(f)) {
        perror("castor");
        fclose(f);
        return 2;
    }
    fclose(f);
    if(queryString == NULL) {
        fprintf(stderr, "Empty query\n");
        return 0;
    }
    queryString[queryLen] = '\0';

    printf("Query: %s\n", queryString);

    store = sqlite_store_open(dbpath);
    if(store == NULL) {
        fprintf(stderr, "Unable to open %s\n", dbpath);
        goto error;
    }

    query = new_query(queryString);
    if(query == NULL) {
        fprintf(stderr, "Unable to parse query\n");
        goto cleanstore;
    }

    printf("vars: %d\nrequested: %d\n", query_variables_count(query),
           query_variables_requested(query));

    free_query(query);

    store_close(store);
    free(queryString);
    return 0;

cleanstore:
    store_close(store);
error:
    free(queryString);
    return 2;
}
