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

extern "C" SQLITE_EXTENSION_ENTRY_POINT int sqlite3_re2_init(
    sqlite3 *db,
    char **pzErrMsg,
    const sqlite3_api_routines *pApi);


static void re2_consumeN(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv)
{
    assert(argc == 2);
    int non_null = 0;

    // TODO: put in soundness check on the re
    re2::RE2 re(reinterpret_cast<const char *>(sqlite3_value_text(argv[0])));

    // TODO: check that this is not null or malformed
    // We keep track of original_input so that we can calculate offsets of matches from the
    // original input string.
    re2::StringPiece original_input = (reinterpret_cast<const char *>(sqlite3_value_text(argv[1])));
    re2::StringPiece input(original_input);

    int groupSize = re.NumberOfCapturingGroups();
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

    auto group_name_map = re.CapturingGroupNames(); // see if this is zero or one based
    while (re2::RE2::FindAndConsumeN(&input, re, &(args_re2[0]), groupSize))
    {
        // xref https://github.com/google/re2/issues/24#issuecomment-97653183
        // cout << group_name_map[i+1] <<"="<<  << "pos=" << ws_re2[i].data() - original_input.data() << "len=" << ws_re2[i].size() << endl;

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

            j[group_name_map[i + 1]] = jv; // XXX: is group capture map 1-based?
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

    return;
}


int sqlite3_re2_init(
    sqlite3 *db,
    char **pzErrMsg,
    const sqlite3_api_routines *pApi)
{
    int rc = SQLITE_OK;
    SQLITE_EXTENSION_INIT2(pApi);
    (void)pzErrMsg; /* Unused parameter */

    rc = sqlite3_create_function(db, "re2_consume", 2,
                                 SQLITE_UTF8 | SQLITE_DETERMINISTIC,
                                 0, re2_consumeN, 0, 0);
    return rc;
}