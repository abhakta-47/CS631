# Automatic Index Creation for PostgreSQL Based on Relation Scans

### Submitted BY
- Arnab Bhakta (23m0835)
- Pranav Shinde (23m0833)

## Motivation
In PostgreSQL, full table scans can be inefficient, particularly when an index could greatly enhance query performance. Manually creating indexes can be tedious and demands a thorough understanding of the database schema and query patterns. Automating the index creation process can improve query performance without manual intervention, making the database more efficient and easier to use.

## Work Done
- [x] Track Full Relation Scans:
    - [x] Modify the code to monitor the number of full relation scans.
    - [x] Store this tracking information in an appropriate data structure.
- [x] Cost Calculation:
    - [x] Implement functions to calculate the cost of full scans and the cost of index creation.
    - [x] Initially use placeholder values for these costs, with the option to refine them later.
- [x] Threshold Check and Index Creation:
    - [x] Trigger the index creation process when the number of full scans exceeds a threshold (calculated as total cost of scans > cost of index creation).
    - [x] Automatically create indexes.

## Files Changed
### Auto Create index on threshold breach
```
 src/backend/executor/nodeSeqscan.c | 181 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 1 files changed, 181 insertions(+)
```
### Create index using BackGroundWorker
```
 src/backend/executor/nodeSeqscan.c             | 35 +++++++++++++++++++++++++----------
 src/backend/postmaster/Makefile                |  3 ++-
 src/backend/postmaster/bgworker_create_index.c | 60 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 src/include/executor/nodeSeqscan.h             |  2 ++
 src/include/postmaster/bgworker_create_index.h | 13 +++++++++++++
 5 files changed, 102 insertions(+), 11 deletions(-)
```

## Design Choices
### Existing Flow
```cpp
main(int argc, char ** argv)
PostgresSingleUserMain(int argc, char ** argv, const char * username)
PostgresMain(const char * dbname, const char * username)
exec_simple_query(const char * query_string)
PortalRun(Portal portal, long count, _Bool isTopLevel, _Bool run_once, DestReceiver * dest, DestReceiver * altdest, QueryCompletion * qc)
PortalRunSelect(Portal portal, _Bool forward, long count, DestReceiver * dest)
ExecutorRun(QueryDesc * queryDesc, ScanDirection direction, uint64 count, _Bool execute_once)
standard_ExecutorRun(QueryDesc * queryDesc, ScanDirection direction, uint64 count, _Bool execute_once)
ExecutePlan(EState * estate, PlanState * planstate, _Bool use_parallel_mode, CmdType operation, _Bool sendTuples, uint64 numberTuples, ScanDirection direction, DestReceiver * dest, _Bool execute_once)
ExecProcNode(PlanState * node)
ExecProcNodeFirst(PlanState * node)
ExecSeqScan(PlanState * pstate)
ExecScan(ScanState * node, ExecScanAccessMtd accessMtd, ExecScanRecheckMtd recheckMtd)
ExecScanFetch(ScanState * node, ExecScanAccessMtd accessMtd, ExecScanRecheckMtd recheckMtd)
```
### Change Flow
```
main(int argc, char ** argv)
PostgresSingleUserMain(int argc, char ** argv, const char * username)
PostgresMain(const char * dbname, const char * username)
exec_simple_query(const char * query_string)
PortalRun(Portal portal, long count, _Bool isTopLevel, _Bool run_once, DestReceiver * dest, DestReceiver * altdest, QueryCompletion * qc)
PortalRunSelect(Portal portal, _Bool forward, long count, DestReceiver * dest)
ExecutorRun(QueryDesc * queryDesc, ScanDirection direction, uint64 count, _Bool execute_once)
standard_ExecutorRun(QueryDesc * queryDesc, ScanDirection direction, uint64 count, _Bool execute_once)
ExecutePlan(EState * estate, PlanState * planstate, _Bool use_parallel_mode, CmdType operation, _Bool sendTuples, uint64 numberTuples, ScanDirection direction, DestReceiver * dest, _Bool execute_once)
ExecProcNode(PlanState * node)
ExecProcNodeFirst(PlanState * node)
```
#### ExecInitSeqScan(SeqScan *node, EState *estate, int eflags):
```cpp
AttrNumber attnum = qual->steps[0].d.var.attnum;
Oid relid = scanstate->ss.ss_currentRelation->rd_id;
printf("attrid: %d relid %d\n", attnum, relid);
int freq = update_seq_attr_file(attnum, relid);
if(should_create_index(relid,attnum, freq)){
    elog(WARNING, "Creating index on attribute: %d rel: %d",attnum, relid);
    create_index(relid, attnum);
}
```
```
ExecSeqScan(PlanState * pstate)
ExecScan(ScanState * node, ExecScanAccessMtd accessMtd, ExecScanRecheckMtd recheckMtd)
ExecScanFetch(ScanState * node, ExecScanAccessMtd accessMtd, ExecScanRecheckMtd recheckMtd)
```


### Indepth funtion designs

## In Action

## Benchmarks

