#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>

#include "CURL.h"
#include "POL.h"
#include "PRO.h"
#include "TEST.h"

// MEM-010 repro: link-time (--wrap) fault injection for the request
// allocators, plus live counters for easy handles and curl_slist nodes.
//
// Pre-fix failures this pins, one per defect:
//  1. calloc NULL -> CURLGetTimed writes through a NULL req (SEGV); strdup
//     NULL -> a URL-less request is still handed to libcurl and reports OK;
//     curl_easy_init NULL -> a NULL handle reaches curl_multi_add_handle.
//  2. a failing curl_multi_add_handle frees url+req but leaks the easy
//     handle (easy_live stays 1).
//  3. a POST with a content type leaks its curl_slist, both on the
//     add-handle failure path and on the normal CURLTick completion path.
//
// No sockets, no DNS, no fork: the completion case uses an unsupported URL
// scheme, which libcurl fails before it ever touches the network.

static int fail_calloc = 0;
static int fail_strdup = 0;
static int fail_easy_init = 0;
static int fail_add_handle = 0;

static int easy_live = 0;
static int slist_live = 0;
static int add_handle_calls = 0;

void *__real_calloc(size_t n, size_t sz);
void *__wrap_calloc(size_t n, size_t sz) {
    if (fail_calloc) {
        fail_calloc = 0;
        return NULL;
    }
    return __real_calloc(n, sz);
}

char *__real_strdup(const char *s);
char *__wrap_strdup(const char *s) {
    if (fail_strdup) {
        fail_strdup = 0;
        return NULL;
    }
    return __real_strdup(s);
}

CURL *__real_curl_easy_init(void);
CURL *__wrap_curl_easy_init(void) {
    if (fail_easy_init) {
        fail_easy_init = 0;
        return NULL;
    }
    CURL *easy = __real_curl_easy_init();
    if (easy) easy_live++;
    return easy;
}

void __real_curl_easy_cleanup(CURL *easy);
void __wrap_curl_easy_cleanup(CURL *easy) {
    if (easy) easy_live--;
    __real_curl_easy_cleanup(easy);
}

CURLMcode __real_curl_multi_add_handle(CURLM *multi, CURL *easy);
CURLMcode __wrap_curl_multi_add_handle(CURLM *multi, CURL *easy) {
    add_handle_calls++;
    if (fail_add_handle) {
        fail_add_handle = 0;
        return CURLM_BAD_HANDLE;
    }
    return __real_curl_multi_add_handle(multi, easy);
}

struct curl_slist *__real_curl_slist_append(struct curl_slist *list,
                                            const char *str);
struct curl_slist *__wrap_curl_slist_append(struct curl_slist *list,
                                            const char *str) {
    struct curl_slist *ret = __real_curl_slist_append(list, str);
    if (ret) slist_live++;
    return ret;
}

void __real_curl_slist_free_all(struct curl_slist *list);
void __wrap_curl_slist_free_all(struct curl_slist *list) {
    for (struct curl_slist *node = list; node; node = node->next) slist_live--;
    __real_curl_slist_free_all(list);
}

// An unsupported scheme: libcurl completes it with UNSUPPORTED_PROTOCOL
// without opening a socket, so CURLTick's completion path runs offline.
#define MEM010_URL "mem010://leak/"

static int completions = 0;

static void on_done(CURLreq *req, long status, u8cs body) {
    (void)req;
    (void)status;
    (void)body;
    completions++;
}

static void mem_reset() {
    fail_calloc = fail_strdup = fail_easy_init = fail_add_handle = 0;
    easy_live = slist_live = add_handle_calls = 0;
    completions = 0;
}

// Defect 1: a NULL calloc must not be written through, a NULL easy handle
// and a NULL url must never reach libcurl, and neither may leak a handle.
static ok64 CURLmem_alloc_fail() {
    sane(1);
    call(POLInit, 16);
    call(CURLInit);

    mem_reset();
    fail_calloc = 1;
    want(CURLGet(MEM010_URL, on_done, NULL) != OK);
    want(add_handle_calls == 0);
    want(easy_live == 0);

    mem_reset();
    fail_easy_init = 1;
    want(CURLGet(MEM010_URL, on_done, NULL) != OK);
    want(add_handle_calls == 0);
    want(easy_live == 0);

    mem_reset();
    fail_strdup = 1;
    want(CURLGet(MEM010_URL, on_done, NULL) != OK);
    want(add_handle_calls == 0);
    want(easy_live == 0);

    CURLFree();
    POLFree();
    done;
}

// Defect 2: a failing curl_multi_add_handle must clean up the easy handle.
static ok64 CURLmem_add_handle_fail() {
    sane(1);
    call(POLInit, 16);
    call(CURLInit);

    mem_reset();
    fail_add_handle = 1;
    want(CURLGet(MEM010_URL, on_done, NULL) != OK);
    want(add_handle_calls == 1);
    want(easy_live == 0);

    CURLFree();
    POLFree();
    done;
}

// Defect 3a: the POST content-type slist must be freed on the start-failure
// path too (together with the easy handle).
static ok64 CURLmem_post_slist_fail() {
    sane(1);
    call(POLInit, 16);
    call(CURLInit);

    mem_reset();
    a_cstr(body, "{}");
    fail_add_handle = 1;
    want(CURLPost(MEM010_URL, body, "application/json", on_done, NULL) != OK);
    want(slist_live == 0);
    want(easy_live == 0);

    CURLFree();
    POLFree();
    done;
}

// Defect 3b: the POST content-type slist must be freed when the request
// completes normally through CURLTick.
static ok64 CURLmem_post_slist_done() {
    sane(1);
    call(POLInit, 16);
    call(CURLInit);

    mem_reset();
    a_cstr(body, "{}");
    call(CURLPost, MEM010_URL, body, "application/json", on_done, NULL);
    want(slist_live == 1);

    // The unsupported scheme is already DONE inside the multi, so one tick
    // (what the POL socket/timer callbacks call) reaps it.
    call(CURLTick);

    want(completions == 1);
    want(slist_live == 0);
    want(easy_live == 0);

    CURLFree();
    POLFree();
    done;
}

ok64 CURLmemtest() {
    sane(1);
    call(CURLmem_alloc_fail);
    call(CURLmem_add_handle_fail);
    call(CURLmem_post_slist_fail);
    call(CURLmem_post_slist_done);
    done;
}

TEST(CURLmemtest);
