#include "postgres.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "postmaster/bgworker.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/proc.h"
#include "utils/memutils.h"
#include "utils/elog.h"
#include "catalog/pg_type.h"
#include "access/heapam.h"
#include "access/relscan.h"
#include "access/xact.h"
#include "commands/vacuum.h"
#include "utils/rel.h"
#include "utils/builtins.h"
#include "executor/execExpr.h"
#include "nodes/execnodes.h"
#include "nodes/pathnodes.h"
#include "optimizer/cost.h"
#include "storage/bufmgr.h"
#include "access/table.h"
#include "catalog/pg_am_d.h"
#include "catalog/index.h"
#include "commands/defrem.h"
#include "postmaster/postmaster.h"
#include "postmaster/bgworker_create_index.h"

PG_MODULE_MAGIC;

void _PG_init(void);
void worker_main(Datum main_arg);

void bg_index_create(Oid relationOid, int attributeId) {
    elog(INFO, "Register index_creator on attribute: %d rel: %u\n", attributeId, relationOid);
    BackgroundWorker worker;
    BackgroundWorkerHandle *handle;
    WorkerArgs *args;

    args = (WorkerArgs *) palloc(sizeof(WorkerArgs));
    args->relationOid = relationOid;
    args->attributeId = attributeId;

    MemSet(&worker, 0, sizeof(BackgroundWorker));
    worker.bgw_flags = BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
    worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
    worker.bgw_restart_time = BGW_NEVER_RESTART;
    snprintf(worker.bgw_library_name, BGW_MAXLEN, "postgres");
    snprintf(worker.bgw_function_name, BGW_MAXLEN, "auto_create_index_main");
    snprintf(worker.bgw_name, BGW_MAXLEN, "auto index creator");
    snprintf(worker.bgw_type, BGW_MAXLEN, "auto_index_creator");
    worker.bgw_main_arg = PointerGetDatum(args);
    worker.bgw_notify_pid = MyProcPid;

    if(!RegisterDynamicBackgroundWorker(&worker, &handle))
        ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_RESOURCES),
					 errmsg("could not register background process"),
					 errhint("You may need to increase max_worker_processes.")));
}