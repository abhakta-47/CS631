#include "postgres_ext.h"

#ifndef BGWORKER_CREATE_INDEX_H
#define BGWORKER_CREATE_INDEX_H

typedef struct {
    Oid relationOid;
    int attributeId;
} WorkerArgs;

void bg_index_create(Oid, int);

#endif // BGWORKER_CREATE_INDEX_H