This table uses Pandoc captions and attributes

| Key | Value |
| --- | :---: |
| one |   1   |
| two |   2   |

: Key value table {:.testing}



This table combines both rowspan and colspan features:

[Employee Performance Q4 2025]
| Department  | Employee | Q1-Q2 Average | Q3     | Q4  | Overall |
| ----------- | -------- | ------------- | ------ | --- | ------- |
| Engineering | Alice    | 93.5          | 94     | 96  | 94.25   |
| ^^          | Bob      | 89.0          | 87     | 91  | 89.00   |
| Marketing   | Charlie  | 92.0          | Absent |     | 92.00   |
| Sales       | Diana    | 87.5          | 88     | 90  | 88.50   |
| ^^          | Eve      | 93.0          | 95     | 93  | 93.50   |
{: .performance-table #q4-results}

More colspan testing:

|       | This    | Has   | Colspans       |
| ----: | ------- | ----- | -------------- |
| row 1 |         | blank | cell           |
| row 2 | colspan |||
| row 3 | cell    || &larr; colspan |
| row 4 | forced colspan | << | &larr; colspan |
| row 5 | raw \<< | \<< | No colspan |

: Just some colspan testing

---

Use `^^` to merge cells vertically (rowspan):

| Name  | Department  | Project  | Status |
| ----- | ----------- | -------- | ------ |
| Frank | Malarkey    | Alpha    | Active |
| ^^    | ^^          | Beta     | ^^     |
| ^^    | ^^          | Gamma    | ^^     |
| Ron   | Advertising | Campaign | Active |
| Chuck | Hooliganism | Q4       | Active |

---


| Department  | Employee | Q1-Q2 Average | Q3      | Q4      | Overall   |
| ----------- | -------- | ------------- | ------- | ------- | --------- |
| Engineering | Alice    | 93.5          | 94      | 96      | 94.25     |
| Foogling    | Jake     | 60.3          | 20      | 20      | 30.1      |
| boggling    | 24       |               |         |         |           |
| =====       | =======  | =========     | =====   | ======= | ========= |
| footer      | row      | with          | colspan |         |           |

[colspans]
| header |      |      |
| ------ | ---- | ---- |
| data   | data | data |
| data   | data | data |
| ====   | ===  | ===  |
| footer |      |      |


| h1  |  h2   | h3  |
| --- | :---: | --- |
| d1  |  d2   | d3  |
| d1  |  d2   | d3  |
| === |  ===  | === |
| d-4 |  d-5  | d-6 |
[table with footer]

| h1  | h2 asdfasdf asdf | h3  |
| --- | :--------------: | --- |
| d1  |       : d2       | d3  |
| d1  |       d2 :       | d3  |

Table: Table with Pandoc caption


| ----: | :-----: | :----- |
| aa | bb | cc |
| 1     | 2       | 3      |

Table: Table without header row

[Relaxed table]
| a      | b      | c      |
| 1d | 2b | 3c |
| data 1 | data 2 | data 3 |
