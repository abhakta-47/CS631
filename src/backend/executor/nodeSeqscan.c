/*-------------------------------------------------------------------------
 *
 * nodeSeqscan.c
 *	  Support routines for sequential scans of relations.
 *
 * Portions Copyright (c) 1996-2023, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeSeqscan.c
 *
 *-------------------------------------------------------------------------
 */
/*
 * INTERFACE ROUTINES
 *		ExecSeqScan				sequentially scans a relation.
 *		ExecSeqNext				retrieve next tuple in sequential order.
 *		ExecInitSeqScan			creates and initializes a seqscan node.
 *		ExecEndSeqScan			releases any storage allocated.
 *		ExecReScanSeqScan		rescans the relation
 *
 *		ExecSeqScanEstimate		estimates DSM space needed for parallel scan
 *		ExecSeqScanInitializeDSM initialize DSM for parallel scan
 *		ExecSeqScanReInitializeDSM reinitialize DSM for fresh parallel scan
 *		ExecSeqScanInitializeWorker attach to DSM info in parallel worker
 */
#include "postgres.h"

#include "access/relscan.h"
#include "access/tableam.h"
#include "executor/execdebug.h"
#include "executor/nodeSeqscan.h"
#include "utils/rel.h"
#include "executor/execExpr.h"
#include "nodes/execnodes.h"
#include "nodes/pathnodes.h"
#include "optimizer/cost.h"
#include "storage/bufmgr.h"
#include "access/table.h"
#include "catalog/pg_am_d.h"
#include "catalog/index.h"
#include "commands/defrem.h"

static TupleTableSlot *SeqNext(SeqScanState *node);

/* ----------------------------------------------------------------
 *						Scan Support
 * ----------------------------------------------------------------
 */

/* ----------------------------------------------------------------
 *		SeqNext
 *
 *		This is a workhorse for ExecSeqScan
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
SeqNext(SeqScanState *node)
{
	TableScanDesc scandesc;
	EState	   *estate;
	ScanDirection direction;
	TupleTableSlot *slot;

	/*
	 * get information from the estate and scan state
	 */
	scandesc = node->ss.ss_currentScanDesc;
	estate = node->ss.ps.state;
	direction = estate->es_direction;
	slot = node->ss.ss_ScanTupleSlot;

	if (scandesc == NULL)
	{
		/*
		 * We reach here if the scan is not parallel, or if we're serially
		 * executing a scan that was planned to be parallel.
		 */
		scandesc = table_beginscan(node->ss.ss_currentRelation,
								   estate->es_snapshot,
								   0, NULL);
		node->ss.ss_currentScanDesc = scandesc;
	}

	/*
	 * get the next tuple from the table
	 */
	if (table_scan_getnextslot(scandesc, direction, slot))
		return slot;
	return NULL;
}

/*
 * SeqRecheck -- access method routine to recheck a tuple in EvalPlanQual
 */
static bool
SeqRecheck(SeqScanState *node, TupleTableSlot *slot)
{
	/*
	 * Note that unlike IndexScan, SeqScan never use keys in heap_beginscan
	 * (and this is very bad) - so, here we do not check are keys ok or not.
	 */
	return true;
}

/* ----------------------------------------------------------------
 *		ExecSeqScan(node)
 *
 *		Scans the relation sequentially and returns the next qualifying
 *		tuple.
 *		We call the ExecScan() routine and pass it the appropriate
 *		access method functions.
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecSeqScan(PlanState *pstate)
{
	SeqScanState *node = castNode(SeqScanState, pstate);

	return ExecScan(&node->ss,
					(ExecScanAccessMtd) SeqNext,
					(ExecScanRecheckMtd) SeqRecheck);
}

#define MAX_LINE_LENGTH 256

int update_seq_attr_file(int attrid, int relid)
{
    FILE *file;
    char line[MAX_LINE_LENGTH];
    int found = 0;

    // Open the file for reading and writing
    file = fopen("seq_attr.txt", "r+");
    if (file == NULL)
    {
        // If the file doesn't exist, create it
        file = fopen("seq_attr.txt", "w+");
        if (file == NULL)
        {
            elog(ERROR, "Could not open seq_attr.txt");
            return -1;
        }
    }

    // Read the file contents into a data structure
    struct Entry
    {
        int attrid;
        int relid;
        int counter;
    };
    struct Entry entries[100];
    int entry_count = 0;
	int freq=-1;

    while (fgets(line, sizeof(line), file))
    {
        struct Entry entry;
        if (sscanf(line, "%d %d %d", &entry.attrid, &entry.relid, &entry.counter) == 3)
        {
            entries[entry_count++] = entry;
        }
    }

    // Check if the combination of attrid and relid exists
    for (int i = 0; i < entry_count; i++)
    {
        if (entries[i].attrid == attrid && entries[i].relid == relid)
        {
            entries[i].counter++;
            found = 1;
			freq=entries[i].counter;
            break;
        }
    }

    // If not found, add a new entry
    if (!found)
    {
        entries[entry_count].attrid = attrid;
        entries[entry_count].relid = relid;
        entries[entry_count].counter = 1;
		freq=1;
        entry_count++;
    }

    // Write the updated data back to the file
    freopen("seq_attr.txt", "w", file);
    for (int i = 0; i < entry_count; i++)
    {
        fprintf(file, "%d %d %d\n", entries[i].attrid, entries[i].relid, entries[i].counter);
    }

    fclose(file);
	return freq;
}

bool should_create_index(int relid, int attrid, int freq){
	Relation rel = table_open(relid, AccessShareLock);
    RelOptInfo *relopt = makeNode(RelOptInfo);
	relopt->relid = relid;
    relopt->tuples = RelationGetNumberOfBlocks(rel);
    relopt->rows = rel->rd_rel->reltuples;
    relopt->pages = RelationGetNumberOfBlocks(rel);

	double seq_scan_cost = relopt->rows * DEFAULT_CPU_TUPLE_COST;
	double index_creation_cost = relopt->rows;
	double index_scan_cost = relopt->rows * DEFAULT_CPU_INDEX_TUPLE_COST;
	// freq*seq
	if( freq > 1/3*index_creation_cost ) return true;
	return false;
}

void create_index(Oid relationOid, int attributeId) {
	char indexName[NAMEDATALEN];
    snprintf(indexName, NAMEDATALEN, "auto_%u_%d", relationOid, attributeId);
    Relation rel;
    IndexInfo *indexInfo;
    List *indexColNames;
    Oid indexOid;
    Oid classObjectId[INDEX_MAX_KEYS];
	Oid collationObjId[INDEX_MAX_KEYS];
    int16 coloptions[INDEX_MAX_KEYS];
    Datum reloptions;
    bool isnull;

    // Open the relation
    rel = table_open(relationOid, ShareLock);

    // Initialize index information
    indexInfo = makeNode(IndexInfo);
    indexInfo->ii_NumIndexAttrs = 1;
    indexInfo->ii_NumIndexKeyAttrs = 1;
	indexInfo->ii_IndexAttrNumbers[0] = attributeId;
    indexInfo->ii_Expressions = NIL;
    indexInfo->ii_ExpressionsState = NIL;
    indexInfo->ii_Predicate = NIL;
    indexInfo->ii_PredicateState = NULL;
    indexInfo->ii_ExclusionOps = NULL;
    indexInfo->ii_ExclusionProcs = NULL;
    indexInfo->ii_ExclusionStrats = NULL;
    indexInfo->ii_Unique = false;
    indexInfo->ii_ReadyForInserts = true;
    indexInfo->ii_Concurrent = false;

    // Set index column names
    indexColNames = list_make1(makeString("index_col"));

    // Set class object ID and column options
	classObjectId[0] = GetDefaultOpClass(rel->rd_att->attrs[attributeId - 1].atttypid, BTREE_AM_OID);

    coloptions[0] = 0;
	collationObjId[0] = InvalidOid;

    // Set relation options
    reloptions = (Datum)0; // transformRelOptions((Datum) 0, NULL, NULL, NULL, false, false);
    // (void) heap_reloptions(rel->rd_rel->relkind, reloptions, true);

    // Create the index
    indexOid = index_create(rel,
                            indexName,
                            InvalidOid,
                            InvalidOid,
                            InvalidOid,
							InvalidRelFileNumber,
                            indexInfo,
                            indexColNames,
                            BTREE_AM_OID,
                            rel->rd_rel->reltablespace,
							collationObjId,
                            classObjectId,
                            coloptions,
                            reloptions,
                            INDEX_CREATE_IF_NOT_EXISTS,
                            0,
                            true,
                            false,
                            NULL);

    // Close the relation
    table_close(rel, NoLock);
}

/* ----------------------------------------------------------------
 *		ExecInitSeqScan
 * ----------------------------------------------------------------
 */
SeqScanState *
ExecInitSeqScan(SeqScan *node, EState *estate, int eflags)
{
	SeqScanState *scanstate;

	/*
	 * Once upon a time it was possible to have an outerPlan of a SeqScan, but
	 * not any more.
	 */
	Assert(outerPlan(node) == NULL);
	Assert(innerPlan(node) == NULL);

	/*
	 * create state structure
	 */
	scanstate = makeNode(SeqScanState);
	scanstate->ss.ps.plan = (Plan *) node;
	scanstate->ss.ps.state = estate;
	scanstate->ss.ps.ExecProcNode = ExecSeqScan;

	/*
	 * Miscellaneous initialization
	 *
	 * create expression context for node
	 */
	ExecAssignExprContext(estate, &scanstate->ss.ps);

	/*
	 * open the scan relation
	 */
	scanstate->ss.ss_currentRelation =
		ExecOpenScanRelation(estate,
							 node->scan.scanrelid,
							 eflags);

	/* and create slot with the appropriate rowtype */
	ExecInitScanTupleSlot(estate, &scanstate->ss,
						  RelationGetDescr(scanstate->ss.ss_currentRelation),
						  table_slot_callbacks(scanstate->ss.ss_currentRelation));

	/*
	 * Initialize result type and projection.
	 */
	ExecInitResultTypeTL(&scanstate->ss.ps);
	ExecAssignScanProjectionInfo(&scanstate->ss);

	/*
	 * initialize child expressions
	 */
	scanstate->ss.ps.qual =
		ExecInitQual(node->scan.plan.qual, (PlanState *) scanstate);

	ExprState *qual = scanstate->ss.ps.qual;
	if(qual){
		// for now idx 0 is hardcoded; works for only one attribute in where clause
		AttrNumber attnum = qual->steps[0].d.var.attnum;
		Oid relid = scanstate->ss.ss_currentRelation->rd_id;
		printf("attrid: %d relid %d\n", attnum, relid);
		int freq = update_seq_attr_file(attnum, relid);
		if(should_create_index(relid,attnum, freq)){
			elog(WARNING, "Creating index on attribute: %d rel: %d",attnum, relid);
			create_index(relid, attnum);
		}
	}

	return scanstate;
}

/* ----------------------------------------------------------------
 *		ExecEndSeqScan
 *
 *		frees any storage allocated through C routines.
 * ----------------------------------------------------------------
 */
void
ExecEndSeqScan(SeqScanState *node)
{
	TableScanDesc scanDesc;

	/*
	 * get information from node
	 */
	scanDesc = node->ss.ss_currentScanDesc;

	/*
	 * Free the exprcontext
	 */
	ExecFreeExprContext(&node->ss.ps);

	/*
	 * clean out the tuple table
	 */
	if (node->ss.ps.ps_ResultTupleSlot)
		ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
	ExecClearTuple(node->ss.ss_ScanTupleSlot);

	/*
	 * close heap scan
	 */
	if (scanDesc != NULL)
		table_endscan(scanDesc);
}

/* ----------------------------------------------------------------
 *						Join Support
 * ----------------------------------------------------------------
 */

/* ----------------------------------------------------------------
 *		ExecReScanSeqScan
 *
 *		Rescans the relation.
 * ----------------------------------------------------------------
 */
void
ExecReScanSeqScan(SeqScanState *node)
{
	TableScanDesc scan;

	scan = node->ss.ss_currentScanDesc;

	if (scan != NULL)
		table_rescan(scan,		/* scan desc */
					 NULL);		/* new scan keys */

	ExecScanReScan((ScanState *) node);
}

/* ----------------------------------------------------------------
 *						Parallel Scan Support
 * ----------------------------------------------------------------
 */

/* ----------------------------------------------------------------
 *		ExecSeqScanEstimate
 *
 *		Compute the amount of space we'll need in the parallel
 *		query DSM, and inform pcxt->estimator about our needs.
 * ----------------------------------------------------------------
 */
void
ExecSeqScanEstimate(SeqScanState *node,
					ParallelContext *pcxt)
{
	EState	   *estate = node->ss.ps.state;

	node->pscan_len = table_parallelscan_estimate(node->ss.ss_currentRelation,
												  estate->es_snapshot);
	shm_toc_estimate_chunk(&pcxt->estimator, node->pscan_len);
	shm_toc_estimate_keys(&pcxt->estimator, 1);
}

/* ----------------------------------------------------------------
 *		ExecSeqScanInitializeDSM
 *
 *		Set up a parallel heap scan descriptor.
 * ----------------------------------------------------------------
 */
void
ExecSeqScanInitializeDSM(SeqScanState *node,
						 ParallelContext *pcxt)
{
	EState	   *estate = node->ss.ps.state;
	ParallelTableScanDesc pscan;

	pscan = shm_toc_allocate(pcxt->toc, node->pscan_len);
	table_parallelscan_initialize(node->ss.ss_currentRelation,
								  pscan,
								  estate->es_snapshot);
	shm_toc_insert(pcxt->toc, node->ss.ps.plan->plan_node_id, pscan);
	node->ss.ss_currentScanDesc =
		table_beginscan_parallel(node->ss.ss_currentRelation, pscan);
}

/* ----------------------------------------------------------------
 *		ExecSeqScanReInitializeDSM
 *
 *		Reset shared state before beginning a fresh scan.
 * ----------------------------------------------------------------
 */
void
ExecSeqScanReInitializeDSM(SeqScanState *node,
						   ParallelContext *pcxt)
{
	ParallelTableScanDesc pscan;

	pscan = node->ss.ss_currentScanDesc->rs_parallel;
	table_parallelscan_reinitialize(node->ss.ss_currentRelation, pscan);
}

/* ----------------------------------------------------------------
 *		ExecSeqScanInitializeWorker
 *
 *		Copy relevant information from TOC into planstate.
 * ----------------------------------------------------------------
 */
void
ExecSeqScanInitializeWorker(SeqScanState *node,
							ParallelWorkerContext *pwcxt)
{
	ParallelTableScanDesc pscan;

	pscan = shm_toc_lookup(pwcxt->toc, node->ss.ps.plan->plan_node_id, false);
	node->ss.ss_currentScanDesc =
		table_beginscan_parallel(node->ss.ss_currentRelation, pscan);
}
