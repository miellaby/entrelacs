#include <stdio.h> // fopen & co
#include <stdlib.h> // free
#include <assert.h>
#include "log.h"

#include "entrelacs/entrelacs.h"

int main(int argc, char **argv) {
    FILE* fd;
    char buffer[1024];
    //log_init(NULL, "server,session,machine,space=debug");
    log_init(NULL, "server,session,machine=debug");

    xl_init();
    struct s_test { char* program; char* result; } *test, tests[] = {
    {"/childrenOf+locked", "= //locked+/%26%21%23+broken%20environment+//locked+%26%21%23+//locked+it+//locked+,+//locked+escalate+//locked+fall+//locked+arrow+//locked+%40M+//locked+continuation+//locked+operator+//locked+paddock+//locked+closure+//locked+macro+//locked+lambda+//locked+eval+//locked+escape+//locked+var+//locked+load+//locked+let+"},
    {"/let//identity/lambda/x+x/identity+42", "42"},
    {"/any/unreducible/expression/is/left/as/is", "/any/unreducible/expression/is/left/as/is"},
    {"unboundedUncastedAtomLeftAsIs", "unboundedUncastedAtomLeftAsIs"},
    {"/let///anything/can/be/a/variable+id/arrow/deal/with+it+/var/anything/can/be/a/variable+id", "/deal/with+it"},
    {"/let//noCastingNeeded+anAtomWorksAsAVariable/noCastingNeeded", "anAtomWorksAsAVariable"},
    {"/lambda/variable+body", "/closure//variable+body+"},
    {"/let//identity/lambda/x+x/identity+42", "42"},
    {"lambda/x/arrow/unboundedCastedVar/evaluated/as/var+x/var+unBoundedCastedVar", "/unboundedCastedVar/evaluated/as+"},
    {"/let//EscapePreventsEvaluation+dude/escape//lambda/x+x/EscapePreventsEvaluation", "//lambda/x+x/EscapePreventsEvaluation"},
    {"//lambda/x/arrow//var+x+//var+x//var+x/var+x+bread", "/bread/bread/bread+bread"},
    {"/let//myJoin/lambda/x/lambda/y/arrow//var+x/var+y//myJoin+join+me", "/join+me"},
    {"/set/myJoin/lambda/x/lambda/y/arrow//var+x/var+y", "/closure+//x+/lambda+/y+/arrow+//var+x+/var+y+"},
    {"/let//escapedProgram/escape//lambda/x+x+bread/eval+escapedProgram", "/bread"},
    {"///lambda/x/lambda/y/arrow//var+x/var+y+join+me", "/join+me"},
    {"/let//x+hot_potato/let//y+x+y", "hot_potato"},
    {"/set/wave/lambda/x/if//equal/x+world///arrow/hello/var+x/no_way", "/closure+//x+/if+//equal+/x+world+//arrow+/hello+/var+x+no_way+"},
    {"/wave+me", "no_way"},
    {"/wave+world", "/hello+world"},
    {"/set//wave+world+surprised?", "surprised?"},
    {"/wave+world", "surprised?"},
    {"/unset+wave", "wave"},
    {"//lambda/x+x/let//myHeadOf/lambda/x/headOf+x/myHeadOf/escape/1/2/3/4+5", "/2/3/4+5"},
    {"/arrow//var+unBoundedCastedAsVar+2", "/+2"},
    {"/load//escape/x+bound/arrow/x/equal//var+x", "/x/equal+bound"},
    {"@M", "/@M+/+"}, // (p (e k) with p = @M, e=EVE, k=EVE
    {"/let//x+1/let//y+2/let//state+@M//lambda/x+x+state", "//let+//state+@M+//lambda+/x+x+state+///y+2+//x+1++"}, // environnement loaded but no continuation
    {"/let//x+build/arrow/I/can//var+x/any/arrow/even/with/lambda/or+such", "/I/can/build/any/arrow/even/with/lambda/or+such"},
    {"/let//x+foo/arrow//I/can/even/use//escape+escape/to/get///escape/var+x", "/I/can/even/use/escape/to/get/var+x"},
    {"let//x+1/arrow/x+2", "/x+2"},
    {"let//x+1/arrow//var+x+2", "/1+2"},
    {"/say/headOf+", "/say+"},
    {"/say/tailOf+", "/say+"},
    {"/headOf/escape/1/2/3/4+5", "/2/3/4+5"},
    {"/let//x/headOf/escape/1/2/3/4+5+x", "/2/3/4+5"},
    {"/tailOf/escape/1/2/3/4+5", "1"},
    {"/root/escape/I/would/say/hello+world", "/I/would/say/hello+world"},
    {"/childrenOf/escape+hello", "//hello+world+"},
    {"/let//myEscape/macro/raw/arrow//escape+escape/var+raw/myEscape/x+2", "/x+2"},
    {"/if/+/hello+goodbye", "goodbye"},
    {"/if/world/hello+goodbye", "hello"},
    {"/root/escape/////2+%2B+2+=+4", "/////2+%2B+2+=+4"},
    {"/say/isRooted/escape/////2+%2B+2+=+4", "/say/////2+%2B+2+=+4"},
    {"/say/isRooted/escape////2+%2B+2+=+5", "/say+"},
    {"/unroot/escape/////2+%2B+2+=+4", "/////2+%2B+2+=+4"},
    {"/say/isRooted/escape/////2+%2B+2+=+4", "/say+"},
    {"/let//myHeadOf/lambda/x/headOf+x/myHeadOf/escape/1/2/3/4+5", "/2/3/4+5"},
    {"//let//myHeadOf/lambda/x/headOf+x+myHeadOf/escape/1/2/3/4+5", "/2/3/4+5"},
    {"/unroot/something/not+rooted", "/something/not+rooted"},
    // to be rewritten with a fixed point thingy: {"/let//crawlp/arrow/lambda/list/if/list///eval/var+crawlp/headOf+list/+/let//crawl/eval+crawlp/crawl/1/2+3", "/1"},
    {"/say/commit+", "/say+"},
    {"/fall+context/,/set/foo+bar", "bar"},
    {"/say/get+foo", "/say+"},
    {"/fall+context/,/say/get+foo", "/say+bar"},
    {"/set//demo+89e495e7941cf9e40e6980d14a16bf023ccd4c91/paddock//x/arrow//fall+demo/,/var+x+", "/paddock//x/arrow//fall+demo/,/var+x+"},
    {"/fall+context/,/escalate/escape//demo+demo/set/foo+bar", "bar"},
    {"/say/get+foo", "/say+"},
    {"/fall+demo/,/say/get+foo", "/say+bar"},
    {NULL, NULL}
#if 0
        // TODO remake of next expressions

        "/let//crawlp/escape/lambda/list/if/arrow/list/escape//eval+crawlp/headOf+list+Eve/let//crawl/eval+crawlp/crawl/escape/1/2/3/4/5/", // last / => Eve
        "/childrenOf/escape+set",
        "/let//mySet/macro/vv/let//variable/tailOf+vv/let//value/headOf+vv/arrow/root/arrow//var+variable/var+value/mySet/mySet+mySet",
        "/childrenOf/escape+set",
        "set",
        //    "(let (firstRootedp escape lambda list if arrow list escape (let (cond isRooted tailOf list) if arrow cond escape (tailOf list) (eval firstRootedp) headOf list) Eve) \
        //        let (firstRooted eval firstRootedp) \
        //        let (myGet macro variable \
        //            let (links childrenOf variable) \
        //                 let (link firstRooted links) \
        //                          headOf link) ((myGet set) myGet) myGet)",
        //
        "//set+myJoin/lambda/x/lambda/y/arrow/x+y",
        "//myJoin+2+3",
        "///myGet+join+2+3",
#endif
    };
    for (int i = 0; test = &tests[i], test->program; i++) {
        char* programUri = test->program;
        char* wantedUri = test->result;

        fprintf(stderr, "Now evaluating '%s' ; ", programUri);
        Arrow program = xl_uri(programUri);
        Arrow wanted = xl_uri(wantedUri);
        assert(!xl_isEve(program) && !xl_isEve(wanted));

        Arrow result = xl_eval(xl_Eve(), program);
        if (!xl_equal(result, wanted)) {
            fprintf(stderr, "eval(%O) = %O != %O\n", program, result, wanted);
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
