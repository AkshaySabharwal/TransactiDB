# TransactiDB 🚀

**TransactiDB** is a high-performance, in-memory transactional relational database engine implemented in C. 

Initially started as an in-memory key-value prototype, it has now evolved into a **relational database engine** featuring **WHERE filtering**, **multi-table schemas**, **relational INNER JOINs**, and **ACID transactions with full rollback capability via an in-memory Undo Log**.

---

## 🌟 Key Features

1. **WHERE Filtering & Relational Querying** ⭐⭐⭐⭐⭐
   - Filter rows with comparative operators (`=`, `!=`, `>`, `<`, `>=`, `<=`, `LIKE`).
   - Projection support: `SELECT *`, `SELECT ID, NAME`, etc.
   - Example:
     ```sql
     SELECT * WHERE ID = 1
     SELECT * WHERE NAME = AKSHAY
     SELECT * FROM STUDENT WHERE DEPT_ID = 10
     ```

2. **Multiple Tables & Relational JOINs** 🔥
   - Full support for multiple relational tables with typed columns (`INT`, `VARCHAR`).
   - Dynamic table creation (`CREATE TABLE`) and schema inspection (`SHOW TABLES`, `DESCRIBE <table>`).
   - **Cross-table relational INNER JOINs**:
     ```sql
     SELECT STUDENT.NAME, DEPARTMENT.DEPT_NAME
     FROM STUDENT
     JOIN DEPARTMENT
     ON STUDENT.DEPT_ID = DEPARTMENT.DEPT_ID
     ```
     **Output:**
     ```text
     +--------------+----------------------+
     | STUDENT.NAME | DEPARTMENT.DEPT_NAME |
     +--------------+----------------------+
     | Akshay       | CSE                  |
     | John         | IT                   |
     | Sarah        | CSE                  |
     | Emily        | ECE                  |
     +--------------+----------------------+
     (4 rows found)
     ```

3. **ACID Transactions (`BEGIN`, `COMMIT`, `ROLLBACK`)** 🔥
   - Full transaction isolation with an **In-Memory Undo Log Journal**.
   - Reversible `INSERT`, `UPDATE`, and `DELETE` operations.
   - Interactive prompt changes to `TransactiDB [TX#1]>` while inside an active transaction.
   - **Demonstration:**
     ```text
     TransactiDB> SELECT * WHERE ID = 1
     +----+--------+---------+
     | ID | NAME   | DEPT_ID |
     +----+--------+---------+
     | 1  | Akshay | 10      |
     +----+--------+---------+

     TransactiDB> BEGIN
     Transaction #1 started.

     TransactiDB [TX#1]> UPDATE WHERE ID = 1 SET NAME = RAHUL
     Updated 1 row in 'STUDENT'.

     TransactiDB [TX#1]> ROLLBACK
     Transaction rolled back.

     TransactiDB> SELECT * WHERE ID = 1
     +----+--------+---------+
     | ID | NAME   | DEPT_ID |
     +----+--------+---------+
     | 1  | Akshay | 10      |
     +----+--------+---------+
     ```

4. **Concurrency Control & Deadlock Detection**
   - Record-level lock manager supporting **Shared (S)** and **Exclusive (X)** lock modes.
   - Directed Wait-For Graph (WFG) with **Depth-First Search (DFS) cycle detection** for deadlock analysis (`DEADLOCK` / `GRAPH`).

5. **Built-in Query Planner & Diagnostics**
   - Real-time execution plan tracer (`DEBUG ON` / `DEBUG OFF`) showing index probes, slot allocations, and nested-loop join operations.

---

## 🛠️ Project Structure

```text
TransactiDB/
├── main.c           # REPL entry point, prompt state, and database initialization
├── db.h / db.c      # Multi-table relational storage engine & schema management
├── index.h / index.c# Hash index with open addressing and tombstone deletion
├── parser.h / parser.c # SQL tokenizer, WHERE evaluator, and ASCII table renderer
├── transaction.h / transaction.c # ACID transaction engine & Undo Log stack
├── lock.h / lock.c  # Shared & Exclusive lock manager
├── deadlock.h / deadlock.c # Wait-For-Graph with DFS cycle detection
├── test_suite.py    # Automated test suite verifying all 3 major features
├── makefile.txt     # GCC build configuration
└── transactid.exe   # Compiled executable for Windows cmd
```

---

## 💻 How to Build and Run on CMD

### 1. Compile with GCC
Run in your Windows Command Prompt / PowerShell:
```cmd
gcc -Wall -Wextra -std=c11 -O2 -o transactid.exe main.c db.c index.c parser.c transaction.c lock.c deadlock.c
```
*Or using make:*
```cmd
make -f makefile.txt
```

### 2. Run the Interactive CLI
```cmd
transactid.exe
```

### 3. Run the Automated Test Suite
```cmd
python test_suite.py
```

---

## 📖 Command Reference

| Category | Command Example | Description |
|---|---|---|
| **Schema** | `SHOW TABLES` | Lists all tables in the database |
| | `DESCRIBE STUDENT` | Displays column names, types, and primary key info |
| | `CREATE TABLE COURSE (ID INT, TITLE VARCHAR, CREDITS INT)` | Creates a new relational table |
| **Queries** | `SELECT * FROM STUDENT` | Selects all rows from a table |
| | `SELECT * WHERE ID = 1` | Filters active table by condition |
| | `SELECT * WHERE NAME = AKSHAY` | Case-insensitive string search |
| | `SELECT S.NAME, D.DEPT_NAME FROM STUDENT JOIN DEPARTMENT ON STUDENT.DEPT_ID = DEPARTMENT.DEPT_ID` | Multi-table relational INNER JOIN |
| **DML** | `INSERT INTO STUDENT VALUES (5, 'David', 20)` | Inserts a new row |
| | `UPDATE WHERE ID = 1 SET NAME = RAHUL` | Updates matching row values |
| | `DELETE WHERE ID = 2` | Deletes matching rows |
| **Transactions** | `BEGIN` | Starts an isolated transaction |
| | `COMMIT` | Persists changes permanently |
| | `ROLLBACK` | Reverts all changes made since `BEGIN` |
| | `STATUS` | Shows active transaction ID & undo log depth |
| **Diagnostics** | `DEBUG ON` / `DEBUG OFF` | Toggles query execution plan tracing |
| | `LOCKS` | Displays active record locks |
| | `DEADLOCK` | Analyzes Wait-For Graph for circular deadlocks |
| | `HELP` | Prints the interactive cheat sheet |
| | `EXIT` | Closes the database engine |
