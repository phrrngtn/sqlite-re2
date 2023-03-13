#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT1

#include <string>
#include <re2/re2.h>
#include <vector>
#include <iostream>
#include <assert.h>

#include <nlohmann/json.hpp>

#ifdef WIN32
#define SQLITE_EXTENSION_ENTRY_POINT __declspec(dllexport)
#else
#define SQLITE_EXTENSION_ENTRY_POINT
#endif

using namespace std;

#define RETURN_AS_DICT 0
#define RETURN_AS_MATCH_DICT 1

extern "C" SQLITE_EXTENSION_ENTRY_POINT int sqlite3_jsonre2_init(
    sqlite3 *db,
    char **pzErrMsg,
    const sqlite3_api_routines *pApi);


void free_re2(void *p) {
    delete (re2::RE2 *)p;
}

static void re2_consumeN(
    int result_style,
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv)
{
    assert(argc == 2);
    int non_null = 0; // this means we have a non-null json value 
    int save_context=0; // if this is 1, then call sqlite3_set_auxdata

    if (sqlite3_value_type(argv[0]) == SQLITE_NULL ||
        sqlite3_value_type(argv[1]) == SQLITE_NULL) {
            sqlite3_result_null(context);
            return;
        }

    // TODO: put in soundness check on the re
    // TODO: argument caching (other than using auxdata)
    // use sqlite3_get_auxdata at the start and
    // sqlite3_set_auxdata at the end of the function
    // as per https://www.sqlite.org/c3ref/get_auxdata.html

    re2::RE2 *re_p = (re2::RE2 *)sqlite3_get_auxdata(context,0);
    if (re_p == nullptr) {
        re_p = new re2::RE2(reinterpret_cast<const char *>(sqlite3_value_text(argv[0])));
        if (!re_p->ok()){
            cerr << "regexp error" << re_p->error();
            // TODO: set error return
        }
        save_context=1;
    }

    // TODO: check that this is not null or malformed
    // We keep track of original_input so that we can calculate offsets of matches from the
    // original input string.
    re2::StringPiece original_input = (reinterpret_cast<const char *>(sqlite3_value_text(argv[1])));
    re2::StringPiece input(original_input);

    int groupSize = re_p->NumberOfCapturingGroups();
    assert(groupSize > 0 && groupSize < 1000); // arbitrary limit

    // copied from example code snippet
    vector<re2::RE2::Arg> argv_re2(groupSize);
    vector<re2::RE2::Arg *> args_re2(groupSize);
    vector<re2::StringPiece> ws_re2(groupSize);

    for (int i = 0; i < groupSize; ++i)
    {
        args_re2[i] = &argv_re2[i];
        argv_re2[i] = &ws_re2[i];
    }

    // STUDY: use 'body' and 'header'? What about match ranges? separate API? 
    // maybe 'header' (array of CapturingGroupNames), 'body' (array of dicts) and 'offsets' (array of array of tuples)
    nlohmann::ordered_json retval;

    auto group_name_map = re_p->CapturingGroupNames(); // see if this is zero or one based
    while (re2::RE2::FindAndConsumeN(&input, *re_p, &(args_re2[0]), groupSize))
    {
        // xref https://github.com/google/re2/issues/24#issuecomment-97653183
        // cout << group_name_map[i+1] <<"="<<  << "pos=" << ws_re2[i].data() - original_input.data() << "len=" << ws_re2[i].size() << endl;

        // xref https://stackoverflow.com/a/11921117/40387

        nlohmann::ordered_json j;
        non_null = 1;
        for (int i = 0; i < groupSize; ++i)
        {
            nlohmann::json jv;
            if (ws_re2[i].data() == nullptr)
            {
                jv = nullptr;
            }
            else
            {
                jv = ws_re2[i];
            }
            switch (result_style){
                case RETURN_AS_DICT:
                    j[group_name_map[i + 1]] = jv; // XXX: is group capture map 1-based?
                    break;
                case RETURN_AS_MATCH_DICT:
                    j["match_group_name"]  = group_name_map[i + 1];
                    j["match_value"] = jv;
                    // xref: https://www.sqlite.org/lang_corefunc.html#substr
                    // "The substr(X,Y,Z) function returns a substring of input string X that begins
                    //  with the Y-th character and which is Z characters long. If Z is omitted then substr(X,Y) returns 
                    //  all characters through the end of the string X beginning with the Y-th.
                    //  The left-most character of X is number 1"
                    
            
                    j["match_start"] = (jv == nullptr) ? -1 : 
                          ws_re2[i].data()
                        - original_input.data()
                        + 1; // we want to be easily usable from sqlite substring()
                    j["match_end"]   = (jv == nullptr) ? -1 : 
                          ws_re2[i].data() 
                        - original_input.data()
                        + ws_re2[i].size()
                        + 1;
                    j["match_length"] = ws_re2[i].size(); // for sqlite substring()
                    break;
                default:
                    assert(0);           
            }
        }
        retval.push_back(j); 
    }

    if (non_null)
    {
        std::string expanded;
        expanded = retval.dump();
        sqlite3_result_text(context, expanded.data(), (int)expanded.length(), SQLITE_TRANSIENT);
    }
    else
    {
        sqlite3_result_null(context);
    }

    if (save_context==1){
        // make sure to only set this the first time around as 
        // "After each call to sqlite3_set_auxdata(C,N,P,X) where X is not NULL, SQLite will invoke the destructor 
        //  function X with parameter P exactly once, when the metadata is discarded.""
        sqlite3_set_auxdata(context, 0, re_p, free_re2);
    } 
    return;
}

static void re2_consumeN_dict(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv)
{
    re2_consumeN(RETURN_AS_DICT, context, argc, argv);
}

static void re2_consumeN_match(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv)
{
    re2_consumeN(RETURN_AS_MATCH_DICT, context, argc, argv);
}

int sqlite3_jsonre2_init(
    sqlite3 *db,
    char **pzErrMsg,
    const sqlite3_api_routines *pApi)
{
    int rc = SQLITE_OK;
    SQLITE_EXTENSION_INIT2(pApi);
    (void)pzErrMsg; /* Unused parameter */

    rc = sqlite3_create_function(db, "re2_consume", 2,
                                 SQLITE_UTF8 | SQLITE_DETERMINISTIC,
                                 0, re2_consumeN_dict, 0, 0);
    rc = sqlite3_create_function(db, "re2_consume_returning_match", 2,
                                 SQLITE_UTF8 | SQLITE_DETERMINISTIC,
                                 0, re2_consumeN_match, 0, 0);                                 
    return rc;
}