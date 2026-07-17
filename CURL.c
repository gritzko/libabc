#include "CURL.h"

#include <stdlib.h>
#include <string.h>

#include "POL.h"

// ABC-012: curl_multi/curl_running/timer are process-global while POL state
// is thread_local: drive CURL* only from the one thread that ran CURLInit.
static CURLM *curl_multi = NULL;
static int curl_running = 0;

#ifndef CURL_WRITEFUNC_ERROR
#define CURL_WRITEFUNC_ERROR 0  // pre-7.87 curl: any short count aborts
#endif

// Header callback - accumulates response headers
static size_t curl_header_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    CURLreq *req = (CURLreq *)userdata;
    size_t bytes = size * nmemb;

    // ABC-012: propagate buffer failure; never ack bytes we did not keep
    if (u8bReserve(req->headers, bytes) != OK) return CURL_WRITEFUNC_ERROR;

    u8cs src = {(u8c *)ptr, (u8c *)ptr + bytes};
    if (u8bFeed(req->headers, src) != OK) return CURL_WRITEFUNC_ERROR;
    return bytes;
}

// Write callback - accumulates response data
static size_t curl_write_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    CURLreq *req = (CURLreq *)userdata;
    size_t bytes = size * nmemb;

    // ABC-012: propagate buffer failure; a truncated 200 body must FAIL
    if (u8bReserve(req->response, bytes) != OK) return CURL_WRITEFUNC_ERROR;

    u8cs src = {(u8c *)ptr, (u8c *)ptr + bytes};
    if (u8bFeed(req->response, src) != OK) return CURL_WRITEFUNC_ERROR;
    return bytes;
}

// Forward declaration
ok64 CURLTick();

// POL callback when socket is ready
static short curl_pol_cb(int fd, poller *p) {
    int action = 0;
    if (p->revents & POLLIN) action |= CURL_CSELECT_IN;
    if (p->revents & POLLOUT) action |= CURL_CSELECT_OUT;

    curl_multi_socket_action(curl_multi, fd, action, &curl_running);
    CURLTick();  // Check for completions

    // If no more running handles, return 0 to stop watching
    if (curl_running == 0) return 0;

    // ABC-012: return the POST-action interest mask; socket_action may have
    // re-registered this fd (IN<->OUT) and the pre-action mask would stall it
    short events = 0;
    if (POLEvents(fd, &events) != OK) return 0;  // curl dropped the fd
    return events;
}

// Curl socket callback - register/unregister with POL
static int curl_sock_cb(CURL *easy, curl_socket_t fd, int action,
                        void *userp, void *socketp) {
    if (action == CURL_POLL_REMOVE) {
        // Ignore errors - fd may not be tracked
        (void)POLIgnoreEvents(fd);
    } else {
        short events = 0;
        if (action & CURL_POLL_IN) events |= POLLIN;
        if (action & CURL_POLL_OUT) events |= POLLOUT;

        poller p = {
            .callback = curl_pol_cb,
            .events = events,
            .tofd = fd,
        };
        // ABC-012: an untracked fd would stall the transfer; abort instead
        if (POLTrackEvents(fd, p) != OK) return -1;
    }
    return 0;
}

// Timer state
static u32 curl_timer_ms = 0;

// POL timer callback
static u32 curl_timer_cb(u64 ns) {
    curl_multi_socket_action(curl_multi, CURL_SOCKET_TIMEOUT, 0, &curl_running);
    CURLTick();
    return curl_timer_ms;  // Return next timeout
}

// Curl timer callback - integrate with POL timer
static int curl_multi_timer_cb(CURLM *multi, long timeout_ms, void *userp) {
    if (timeout_ms < 0) {
        curl_timer_ms = 0;
        POLIgnoreTime();
    } else {
        curl_timer_ms = (u32)(timeout_ms ? timeout_ms : 1);
        POLTrackTime(curl_timer_cb);
        POLAddTime(curl_timer_ms);
    }
    return 0;
}

ok64 CURLInit() {
    if (curl_multi) return OK;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl_multi = curl_multi_init();
    if (!curl_multi) return CURLFAIL;

    curl_multi_setopt(curl_multi, CURLMOPT_SOCKETFUNCTION, curl_sock_cb);
    curl_multi_setopt(curl_multi, CURLMOPT_TIMERFUNCTION, curl_multi_timer_cb);

    return OK;
}

ok64 CURLFree() {
    if (curl_multi) {
        curl_multi_cleanup(curl_multi);
        curl_multi = NULL;
    }
    curl_global_cleanup();
    return OK;
}

// Check for completed requests
ok64 CURLTick() {
    if (!curl_multi) return OK;

    CURLMsg *msg;
    int msgs_left;

    while ((msg = curl_multi_info_read(curl_multi, &msgs_left))) {
        if (msg->msg == CURLMSG_DONE) {
            CURL *easy = msg->easy_handle;
            CURLreq *req = NULL;
            curl_easy_getinfo(easy, CURLINFO_PRIVATE, &req);

            if (!req) continue;

            long status = 0;
            curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &status);

            // Call completion callback
            if (req->callback) {
                u8$ data = u8bData(req->response);
                u8cs body = {*data, data[1]};
                req->callback(req, status, body);
            }

            // Cleanup - do this before any further curl operations
            curl_multi_remove_handle(curl_multi, easy);
            curl_easy_cleanup(easy);
            if (req->url) free(req->url);
            u8bFree(req->headers);
            u8bFree(req->response);
            free(req);
        }
    }

    return OK;
}

static ok64 CURLStart(CURLreq *req) {
    curl_easy_setopt(req->easy, CURLOPT_HEADERFUNCTION, curl_header_cb);
    curl_easy_setopt(req->easy, CURLOPT_HEADERDATA, req);
    curl_easy_setopt(req->easy, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(req->easy, CURLOPT_WRITEDATA, req);
    curl_easy_setopt(req->easy, CURLOPT_PRIVATE, req);
    curl_easy_setopt(req->easy, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(req->easy, CURLOPT_TIMEOUT, 30L);
    // Bound the connect so a firewalled/unreachable host fails fast and
    // surfaces as a clean CURL transfer error, independent of any outer
    // ctest TIMEOUT. Caller may override via CURLOPT before CURLStart.
    if (req->connect_ms)
        curl_easy_setopt(req->easy, CURLOPT_CONNECTTIMEOUT_MS, (long)req->connect_ms);
    else
        curl_easy_setopt(req->easy, CURLOPT_CONNECTTIMEOUT, 10L);

    CURLMcode mc = curl_multi_add_handle(curl_multi, req->easy);
    if (mc != CURLM_OK) {
        free(req->url);
        free(req);
        return CURLFAIL;
    }

    // Kick off the request
    curl_multi_socket_action(curl_multi, CURL_SOCKET_TIMEOUT, 0, &curl_running);

    return OK;
}

ok64 CURLGet(const char *url, CURLcb cb, void *userdata) {
    return CURLGetTimed(url, 0, cb, userdata);
}

ok64 CURLGetTimed(const char *url, u32 connect_ms, CURLcb cb, void *userdata) {
    if (!curl_multi) return CURLBAD;

    CURLreq *req = (CURLreq *)calloc(1, sizeof(CURLreq));
    req->easy = curl_easy_init();
    req->url = strdup(url);
    req->callback = cb;
    req->userdata = userdata;
    req->connect_ms = connect_ms;

    curl_easy_setopt(req->easy, CURLOPT_URL, req->url);

    return CURLStart(req);
}

ok64 CURLPost(const char *url, u8cs body, const char *content_type,
              CURLcb cb, void *userdata) {
    if (!curl_multi) return CURLBAD;

    CURLreq *req = (CURLreq *)calloc(1, sizeof(CURLreq));
    req->easy = curl_easy_init();
    req->url = strdup(url);
    req->callback = cb;
    req->userdata = userdata;

    curl_easy_setopt(req->easy, CURLOPT_URL, req->url);
    curl_easy_setopt(req->easy, CURLOPT_POST, 1L);
    curl_easy_setopt(req->easy, CURLOPT_POSTFIELDSIZE, (long)u8csLen(body));
    curl_easy_setopt(req->easy, CURLOPT_COPYPOSTFIELDS, body[0]);

    if (content_type) {
        struct curl_slist *headers = NULL;
        char ct[256];
        snprintf(ct, sizeof(ct), "Content-Type: %s", content_type);
        headers = curl_slist_append(headers, ct);
        curl_easy_setopt(req->easy, CURLOPT_HTTPHEADER, headers);
        // Note: headers leak - would need to store and free in CURLTick
    }

    return CURLStart(req);
}
