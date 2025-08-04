# 🚀 Automatic Index Creation for PostgreSQL Based on Relation Scans

### 🧑‍💻 Submitted By

* Arnab Bhakta (23M0835)
* Pranav Shinde (23M0833)

---

## 💡 Motivation

In PostgreSQL, full table (sequential) scans can lead to inefficient query execution, especially when repeated over large datasets. While adding indexes manually can mitigate this, it requires deep knowledge of the schema and workload, and is not scalable.

Our goal is to automate the creation of indexes based on observed access patterns—specifically, to track frequent full scans and create indexes when beneficial. This brings **self-optimization** to PostgreSQL with **minimal developer intervention**.

---

## ✅ Work Done

* ### 🕵️ Track Full Relation Scans

  * Hooked into `ExecInitSeqScan` to track accesses of attributes involved in sequential scans.
  * Stored scan counts in a persistent file `seq_attr.txt`.

* ### 📈 Cost-Based Index Creation

  * Computed approximate costs for:

    * Full table scans
    * Index creation
    * Index scan
  * Triggered index creation when the cumulative scan cost surpassed index creation cost.

* ### ⚙️ Automatic Index Creation

  * Index creation initiated via **background worker** to avoid blocking user queries.
  * Created B-tree indexes on identified `(relid, attrid)` pairs.

---

## 🧩 Files Changed

### 🔧 Initial Scan Monitoring + Threshold-based Trigger

```text
src/backend/executor/nodeSeqscan.c | +181 lines
```

### 🔨 Background Worker + Integration

```text
src/backend/executor/nodeSeqscan.c             | +35 -10 lines
src/backend/postmaster/Makefile                | +3 lines
src/backend/postmaster/bgworker_create_index.c | +60 lines
src/include/executor/nodeSeqscan.h             | +2 lines
src/include/postmaster/bgworker_create_index.h | +13 lines
```

---

## 🧠 Design Overview

### 📌 Existing Execution Flow

```cpp
main
 └─ PostgresMain
     └─ exec_simple_query
         └─ PortalRun
             └─ ExecutorRun
                 └─ standard_ExecutorRun
                     └─ ExecutePlan
                         └─ ExecProcNode
                             └─ ExecSeqScan
```

### ⚙️ Modified Logic in `ExecInitSeqScan`

```cpp
AttrNumber attnum = qual->steps[0].d.var.attnum;
Oid relid = scanstate->ss.ss_currentRelation->rd_id;

int freq = update_seq_attr_file(attnum, relid);
if (should_create_index(relid, attnum, freq)) {
    elog(WARNING, "Creating index on attribute: %d rel: %d", attnum, relid);
    create_index(relid, attnum);
}
```

---

## 🔬 Function Internals

### 📁 `int update_seq_attr_file(int attrid, int relid)`

* Reads `seq_attr.txt` for `(relid, attrid)` entries.
* Increments count or adds a new one.
* Writes back updated stats.
* Returns current frequency.

### 🧮 `bool should_create_index(int relid, int attrid, int freq)`

* Fetches statistics for the relation.
* Computes:

  * `seq_scan_cost = freq × base_cost`
  * `index_creation_cost = fixed cost`
* Returns true if `seq_scan_cost > index_creation_cost`.

### 🏗️ `void create_index(Oid relid, int attrid)`

* Constructs index name (`idx_<relid>_<attrid>`).
* Builds `IndexInfo` structure.
* Calls `index_create()` with parameters.

## 📚 References

* [📄 Hacking PostgreSQL – Neil Conway & Gavin Sherry (IITB)](https://www.cse.iitb.ac.in/infolab/Data/Courses/CS631/PostgreSQL-Resources/pg_hack_slides.pdf)
* [📘 PostgreSQL Internals Documentation](https://www.postgresql.org/docs/current/internals.html)

---

Let me know if you'd like this in PDF, DOCX, or embedded in your codebase as `README.md`.
