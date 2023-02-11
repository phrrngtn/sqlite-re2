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



Building
========
Uses CMake and vcpkgs