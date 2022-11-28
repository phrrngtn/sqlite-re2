# sqlite-re2
SQLite extension to expose Google's [re2 regular expression library](https://github.com/google/re2) 
via the `re2_consume` scalar function which takes a regular expression (with named matching groups) 
and an input string and returns a JSON array-of-dicts of the matches of the regular expression 
against the input string.

```sql
sqlite> .load ./libsqlite_re2.so sqlite3_re2_init

sqlite> WITH T AS (select re2_consume('(?P<foo>[a-z]+at.*)', name) as res FROM pragma_function_list) SELECT * FROM T where res is not null;
```


Building
========
Uses CMake and vcpkgs