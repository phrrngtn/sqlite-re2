# jsonre2
SQLite extension to expose Google's [re2 regular expression library](https://github.com/google/re2) 
via the `re2_consume` scalar function which takes a regular expression (with named matching groups) 
and an input string and returns a JSON array-of-dicts of the matches of the regular expression 
against the input string.

Note that each row will be a JSON array of dicts, one dict per match, NULL if no match.
```sql
sqlite> .load ./jsonre2 sqlite3_jsonre2_init

sqlite> WITH T AS (
   ...> select re2_consume('(?P<foo>[a-z]+at.*)', name) as res FROM pragma_function_list
   ...> )
   ...> SELECT * FROM T where res is not null;
[{"foo":"concat"}]
[{"foo":"concat"}]
[{"foo":"patch"}]
[{"foo":"date"}]
[{"foo":"format"}]
[{"foo":"datetime"}]
[{"foo":"date"}]
[{"foo":"rate_limit"}]
[{"foo":"matchinfo"}]
[{"foo":"matchinfo"}]
[{"foo":"match"}]
[{"foo":"template_render"}]
[{"foo":"template_render"}]
[{"foo":"date"}]
```

One can make it into a single array by unpacking it via `json_each` and repacking via `json_group_array`

```sql
select json_group_array(je.value)  FROM pragma_function_list,  json_each(re2_consume('(?P<foo>[a-z]+at.*)', name)) as je;
[{"foo":"concat"},{"foo":"concat"},{"foo":"patch"},{"foo":"date"},{"foo":"format"},{"foo":"datetime"},{"foo":"date"},{"foo":"rate_limit"},{"foo":"matchinfo"},{"foo":"matchinfo"},{"foo":"match"},{"foo":"template_render"},{"foo":"template_render"},{"foo":"date"}]
```

There is also a variant, `re2_consume_returning_match` which returns a match dict inspired by
the Python `re` module with keys of:
  * `match_group_name`
  * `match_value`
  * `match_start`
  * `match_end`
  * `match_length`

```sql
 WITH T AS (
   select name as function_name,
          re2_consume_returning_match('[a-z]+(?P<foo>at\S*)',name) as res
     FROM pragma_function_list
 ) SELECT * FROM T where res is not null LIMIT 5;

 function_name|res
group_concat|[{"match_group_name":"foo","match_value":"at","match_start":11,"match_end":13,"match_length":2}]
group_concat|[{"match_group_name":"foo","match_value":"at","match_start":11,"match_end":13,"match_length":2}]
json_patch|[{"match_group_name":"foo","match_value":"atch","match_start":7,"match_end":11,"match_length":4}]
date|[{"match_group_name":"foo","match_value":"ate","match_start":2,"match_end":5,"match_length":3}]
format|[{"match_group_name":"foo","match_value":"at","match_start":5,"match_end":7,"match_length":2}]

```
I hope to add in other variants over time that use specialized json schemas such as a two-key
dict with keys 

`header` (value being a list of column names) 

`body` (value an array of arrays)

Building
========
Uses CMake and vcpkgs