[x] Track Full Relation Scans:
    [x] Modify the code to track the number of full relation scans
    [x] Store this tracking information in a suitable data structure.
    [$] save in pg_stat
[x] Cost Calculation:
    [x] Implement functions to calculate the cost of full scans and the cost of index creation.
    [x] Use fake numbers for these costs initially, with the possibility of refining them later.
    + ML model based cost cal
[!] Threshold Check and Index Creation:
    [x] When the number of full scans crosses a threshold (computed as total cost of scans > cost of index creation), trigger the index creation process.
    [x] Automatic creation of indexes.
    [!] leak close table
    [$] create index using background process
[-] Predicate Evaluation:
    [ ] Extend the implementation to consider the percentage of tuples satisfying the predicat[ ] For example, predicates like Gender='M' should not trigger index creation.
    [ ] Evaluate the selectivity of predicates.
[ ] Configuration and Thresholds:
    [ ] Add configuration parameters for thresholds and fake costs.
[-] Testing and Validation:
    [ ] Write test cases to validate the functionality of the automatic index creation process.
    [ ] Use the isolation tests in isolation to ensure that the implementation does not introduce any concurrency issues.