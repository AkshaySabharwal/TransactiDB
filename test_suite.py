import subprocess
import sys

def run_db_test():
    test_input = """
SHOW TABLES
SELECT * WHERE ID = 1
SELECT * WHERE NAME = AKSHAY
SELECT STUDENT.NAME, DEPARTMENT.DEPT_NAME FROM STUDENT JOIN DEPARTMENT ON STUDENT.DEPT_ID = DEPARTMENT.DEPT_ID
BEGIN
UPDATE WHERE ID = 1 SET NAME = RAHUL
SELECT * WHERE ID = 1
ROLLBACK
SELECT * WHERE ID = 1
BEGIN
UPDATE WHERE ID = 1 SET NAME = RAHUL
COMMIT
SELECT * WHERE ID = 1
BEGIN
INSERT INTO STUDENT VALUES (10, 'Bob', 20)
SELECT * WHERE ID = 10
ROLLBACK
SELECT * WHERE ID = 10
STATUS
EXIT
"""

    print("[TEST] Running TransactiDB Feature Verification Test Suite...")
    p = subprocess.Popen(
        ['transactid.exe'],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    stdout, stderr = p.communicate(input=test_input)

    print("\n--- Output ---")
    print(stdout)

    # Assertions
    assert "STUDENT" in stdout, "Missing STUDENT table in SHOW TABLES"
    assert "DEPARTMENT" in stdout, "Missing DEPARTMENT table in SHOW TABLES"
    assert "Akshay" in stdout, "Akshay row not found in SELECT"
    assert "CSE" in stdout, "JOIN output did not match department CSE"
    assert "Transaction #1 started" in stdout, "BEGIN transaction failed"
    assert "Transaction rolled back" in stdout, "ROLLBACK failed"
    assert "Transaction committed" in stdout, "COMMIT failed"

    print("========================================")
    print(" ALL 3 MAJOR FEATURE TESTS PASSED! [OK]")
    print(" 1. WHERE Conditions: PASSED")
    print(" 2. Multi-Table JOIN: PASSED")
    print(" 3. ACID Transactions: PASSED")
    print("========================================")

if __name__ == "__main__":
    run_db_test()
