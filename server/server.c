/*
 * Modified copy of Mongoose project example, http://code.google.com/p/mongoose
 *  1. type "make server" in the directory where this file lives
 *  2. run entrelacs.d and make request to http://127.0.0.1:8008
 *
 * NOTE(lsm): this file follows Google style, not BSD style as the rest of
 * Mongoose code.
 */
#define _POSIX_SOURCE
#define SERVER_C
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <pthread.h>
#define LOG_CURRENT LOG_SERVER
#include "log/log.h"
#include "server/server.h"
#include "entrelacs/entrelacs.h"
#include "machine/session.h"
#include "machine/context.h"
#include "server/mongoose.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h> /* mmap() is defined in this header */

#ifdef DEBUG
#define SESSION_TTL 120
#define HOUSECLEANING_PERIOD 6
#else
#define SESSION_TTL 3600
#define HOUSECLEANING_PERIOD 60
#endif

 /** assimilate a file descriptor
 */
static Arrow fdatom(int fd) {
    if (fd < 0) return NULL;

    struct stat stat;
    int r = fstat(fd, &stat);
    if (r < 0) return NULL;

    size_t size = stat.st_size;
    if (size < 0) return NULL;

    char* data = NULL;
    if (size > 0) {
        data = mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
        assert(data != (void*)-1);
    }
    Arrow a = xs_atomn(size, (uint8_t*)data);
    munmap(data, size);
    return a;
}

static const char* HTTP_500 = "HTTP/1.0 500 Server Error\r\n\r\n";

/** assimilate a PUT BODY in an arrow /Content-Typed/$mimeType+$body
*/

static Arrow assimilateUploadedData(struct mg_connection* conn) {
    const char* contentLengthHeader;
    char postData[16 * 1024], fileName[1024], mimeType[100],
        buf[BUFSIZ], * eop, * s, * p;
    FILE* stream;
    int64_t contentLength, written;
    int n, postDataLength;

    // Figure out total content length. Return if it is not present or invalid.
    contentLengthHeader = mg_get_header(conn, "Content-Length");
    if (contentLengthHeader == NULL)
        return NULL;

    if ((contentLength = strtoll(contentLengthHeader, NULL, 10)) < 0) {
        mg_printf(conn, "%s%s", HTTP_500, "Invalid Content-Length");
        return NULL;
    }

    // Read the initial chunk into memory. This should be multipart POST data.
    // Parse headers, where we should find file name and content-type.
    postDataLength = mg_read(conn, postData, sizeof(postData));
    fileName[0] = mimeType[0] = '\0';
    for (s = p = postData; p < &postData[postDataLength]; p++) {
        if (p[0] == '\r' && p[1] == '\n') {
            if (s == p) {
                p += 2;
                break;  // End of headers
            }
            p[0] = p[1] = '\0';
            sscanf(s, "Content-Type: %99s", mimeType);
            // TODO(lsm): don't expect filename to be the 3rd field,
            // parse the header properly instead.
            sscanf(s, "Content-Disposition: %*s %*s filename=\"%1023[^\"]",
                fileName);
            s = p + 2;
        }
    }

    if (!fileName[0])
        return NULL;

    // Finished parsing headers. Now "p" points to the first byte of data.
    // Calculate file size
    contentLength -= p - postData;      // Subtract headers size
    contentLength -= strlen(postData);  // Subtract the boundary marker at the end
    contentLength -= 6;                  // Subtract "\r\n" before and after boundary

    LOGPRINTF(LOG_WARN, "upload content-length %d", contentLength);

    if (contentLength <= 0) {
        // Empty file
        return xs_pair(xs_const("Content-Typed"),
            xs_pair(xs_atom(mimeType), xs_eve()));
    }

    stream = tmpfile();

    if (stream == NULL) {
        LOGPRINTF(LOG_ERROR, "Cannot create tmp file");
        return xs_eve();
    }
    else {

        // Success. Write data into the file.
        eop = postData + postDataLength;
        n = p + contentLength > eop ? (int)(eop - p) : (int)contentLength;
        (void)fwrite(p, 1, n, stream);
        written = n;
        while (written < contentLength &&
            (n = mg_read(conn, buf, contentLength - written > (int64_t)sizeof(buf) ?
                sizeof(buf) : (size_t)(contentLength - written))) > 0) {
            (void)fwrite(buf, 1, n, stream);
            written += n;
        }
        rewind(stream);
        Arrow arrow = fdatom(fileno(stream));
        fclose(stream);

        arrow = xs_pair(xs_const("Content-Typed"),
            xs_pair(xs_atom(mimeType), arrow));
        return arrow;
    }
}

// Get session object for the connection. Caller must hold the lock
// HTTP Cookie "session" contains the session id.
// Session is valid if "/server+$id" is rooted within some context (whose bottom context is "sessions")

static Arrow get_connection_session(const struct mg_connection* conn) {
    char session_uuid[100];

    mg_get_cookie(conn, "session", session_uuid, sizeof(session_uuid));
    if (*session_uuid == '\0') {
        dputs("No session cookie");
        return NULL;
    }
    session_uuid[32] = '\0';

    // TODO: one should look for any session whatever it's top-level or it's embedded in a upper context.
    // TODO: remove the first parameter of sessionMaybe
    Arrow session = xs_getSession("server", session_uuid);
    if (session == NULL) {
        dputs("Unknown session cookie %s", session_uuid);
    } else {
        dputs("Session %s found", session_uuid);
    }
    return session;
}

static void get_qsvar(const struct mg_request_info* request_info,
    const char* name, char* dst, size_t dst_len) {
    const char* qs = request_info->query_string;
    mg_get_var(qs, strlen(qs == NULL ? "" : qs), name, dst, dst_len);
    dputs(dst_len == -1 ? "no %s variable in query string" :
        "query string variable %s = '%.*s'", name, dst_len, dst);
}

static void* event_handler(enum mg_event event,
    struct mg_connection* conn,
    const struct mg_request_info* request_info) {
    void* processed = "yes";
    xl_open();
    if (event == MG_NEW_REQUEST) {
        Arrow session = get_connection_session(conn);
        if (session == NULL) {
            // create session
            session = xs_open("server");
            dputs("New session with id %s", session_id);
        }
        char* session_id = xs_session_getId(session);

        dputs("session arrow is %O", session);
        time_t now = time(NULL) + SESSION_TTL;
        xs_context_set(session, xs_const("expire"), xs_atomn(sizeof(time_t), (uint8_t*)&now));

        Arrow input = xs_url(session, request_info->uri);
        dputs("input %s assimilated as %O", request_info->uri, input);
        if (input == NULL) {
            free(session_id);
            const char* origin = mg_get_header(conn, "Origin");
            mg_printf(conn, "HTTP/1.1 %d %s\r\n"
                "Access-Control-Allow-Origin: %s\r\n"
                "Access-Control-Allow-Credentials: true\r\n"
                "Access-Control-Allow-Credentials: true\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 0\r\n"
                "\r\n", 400, origin ? origin : "*",
                "BAD REQUEST");
            mg_write(conn, "", (size_t)0);
            xl_close();
            return processed;
        }
        Arrow method = xs_atom(request_info->request_method);
        if (xs_equal(method, xs_const("POST")) || xs_equal(method, xs_const("PUT"))) {
            Arrow body = assimilateUploadedData(conn);
            if (body != NULL) {
                input = xs_pair(input, body);
            }
        }

        Arrow output = xs_eval(session, xs_pair(method, input), session);

        dputs("Evaluated output is %O", output);

        char i_depthBuf[255];
        char l_depthBuf[255];
        get_qsvar(request_info, "iDepth", i_depthBuf, 254);
        get_qsvar(request_info, "lDepth", l_depthBuf, 254);

        int i_depth = -1;
        int l_depth = 0;
        if (i_depthBuf[0]) { // Non empty string
            i_depth = atoi(i_depthBuf);
        }
        if (l_depthBuf[0]) {
            l_depth = atoi(l_depthBuf);
        }

        char* output_url = xs_urlOf(session, output, l_depth);
        int isAtomic = xs_isAtom(output);

        // TODO
#if 1
#define URI_CONTENT_TYPE "text/plain"
#endif
        char* content_type =
            (i_depth == 0
                ? URI_CONTENT_TYPE
                : (isAtomic
                    ? "application/octet-stream"
                    : URI_CONTENT_TYPE));
        char* contentTypeCopy = NULL;
        if (i_depth != 0 && !isAtomic && xs_equal(xs_getTail(output), xs_const("Content-Typed"))) {
            contentTypeCopy = xl_strOf(xs_getTail(xs_getHead(output)));
            content_type = contentTypeCopy;
            output = xs_getHead(xs_getHead(output));
            dputs("Content-Type: %s, output: %O", content_type, output);
//        } else if (isAtomic) {
//            Arrow application = xs_pair(xs_const("Content-Type"), xs_pair(xs_const("escape"), output));
//            Arrow rta = xs_eval(session, application);
//            if (rta != xs_eve() && xs_isAtom(rta)) {
//               contentTypeCopy = xl_strOf(rta);
//               content_type = contentTypeCopy;
//            }
        }
        uint32_t content_length;
        uint8_t* content = NULL;
        if (i_depth != 0) {
            content = xs_getMem(output, &content_length);
        }
        if (!content) {
            char* url = xs_urlOf(session, output, i_depth);
            content = (uint8_t*)url;
            content_length = strlen(url);
        }
        const char* origin = mg_get_header(conn, "Origin");
        mg_printf(conn, "HTTP/1.1 200 OK\r\nCache: no-cache\r\n"
            "Access-Control-Allow-Origin: %s\r\n"
            "Access-Control-Allow-Credentials: true\r\n"
            "Access-Control-Allow-Headers: X-Entrelacs\r\n"
            "Access-Control-Expose-Headers: X-Entrelacs, Set-Cookie\r\n"
            "Content-Location: %s\r\n"
            "Content-Length: %d\r\n",
            origin ? origin : "*",
            output_url,
            content_length);
        if (content_type) {
            mg_printf(conn, "Content-Type: %s\r\n", content_type);
        }
        mg_printf(conn, "Set-Cookie: session=%s; max-age=60; path=/\r\n", session_id);
        mg_printf(conn, "X-Entrelacs: %s\r\n", session_id);
        mg_printf(conn, "\r\n");
        mg_write(conn, content, (size_t)content_length);
        free(output_url);
        free(content);
        free(session_id);
        if (contentTypeCopy) free(contentTypeCopy);

    }
    else {
        processed = NULL;
    }
    //xl_commit();
    xl_close();
    return processed;
}

int _houseCleaning(void) {
    xl_open();
    Arrow sessionTag = xs_assimilate(xs_const("session"));
    Arrow expireTag = xs_const("expire");
    XLEnum e = xl_childrenOf(xs_getId(sessionTag));
    time_t now = time(NULL);
    dputs("House Cleaning ...");

    Arrow next = e && xl_enumNext(e) ? xl_enumGet(e) : xs_eve();
    int sessionCount = 0;
    int expiredSessionCount = 0;
    int activeSessionCount = 0;

    while (next != xs_eve()) {
        Arrow session = next;
        next = xl_enumNext(e) ? xl_enumGet(e) : xs_eve();
        if (!xs_equal(xs_getTail(session), sessionTag))
            continue; // Not a /session+* arrow
        if (!xs_isRooted(session))
            continue; // maybe not a root session
        sessionCount++;
        Arrow expire = xs_context_get(session, expireTag);
        uint32_t var_size = 0;
        time_t* expire_time = (expire != NULL ? (time_t*)xs_borrowMem(expire, &var_size) : NULL);

        if (expire == NULL || var_size != sizeof(time_t)) {
            LOGPRINTF(LOG_WARN, "session %O : wrong 'expire'", session);
            expiredSessionCount++;
            xs_close(session);
            sessionTag = xs_assimilate(xs_const("session"));
            expireTag = xs_const("expire");
            // restart loop as deep close may remove in-enum arrow
            //xl_enumFree(e);
            //e = xl_childrenOf(sessionTag);
            //next = e && xl_enumNext(e) ? xl_enumGet(e) : xs_eve();
        }
        else if (*expire_time < now) {
            dputs("session %O outdated.", session);
            expiredSessionCount++;
            xs_close(session);
            // restart loop as deep close may remove in-enum arrow
            xl_enumFree(e);
            e = xl_childrenOf(xs_getId(sessionTag));
            next = e && xl_enumNext(e) ? xl_enumGet(e) : xs_eve();
        }
        else {
            activeSessionCount++;
        }
    }
    LOGPRINTF(LOG_WARN, "sessions: active=%d expired=%d total=%d",
        activeSessionCount, expiredSessionCount, sessionCount);
    xl_commit();
    xl_close();
    xl_enumFree(e);
    dputs("House Cleaning done.");
    return 0;
}

int main(void) {
    struct mg_context* ctx;
#ifdef DEBUG
    log_init(NULL, "session,machine,server=debug,space,mem=trace");
#else
#ifdef PRODUCTION
    log_init(NULL, "session,machine,space,mem,server=warn");
#else
    log_init(NULL, "session,machine,space,mem=info,space,server=trace");
#endif
#endif

    xs_init();

    xl_open();
    Arrow get = xs_context_get(xs_eve(), xs_const("GET"));
    if (get == NULL) {
        xs_context_set(xs_eve(), xs_const("GET"), xs_uri("/paddock//x+x+"));
        xs_context_set(xs_eve(), xs_const("PUT"), xs_uri("/paddock//x/arrow/set+/var+x+"));
        xs_context_set(xs_eve(), xs_const("POST"), xs_uri("/paddock//x+x+"));
        xs_context_set(xs_eve(), xs_const("DELETE"), xs_uri("/paddock//x/arrow/unset+/var+x+"));
    }

    {   // store secret as server-secret<->"/session-secret-pair-uri" hidden pair
        char* server_secret_sha1 = getenv("ENTRELACS_SECRET"); // TODO better solution
        if (!server_secret_sha1) server_secret_sha1 = "8f84b95af52fbfae67209b6cfd3ab7dd1f1e0b12";
        // meta-user do : mudo
        xs_context_set(xs_eve(), xs_pair(xs_const("mudo"), xs_atom(server_secret_sha1)), xs_const("eval"));
        // TODO unroot while house cleaning
    }
    xl_close();

    _houseCleaning();

    // Initialize random number generator. It will be used later on for
    // the session identifier creation.
    srand((unsigned)time(0));

    // Setup and start Mongoose
    ctx = mg_start(&event_handler, NULL, server_options);
    assert(ctx != NULL);

    dputs("server started on ports %s.",
        mg_get_option(ctx, "listening_ports"));


    // house cleaning / yield loop
    int sleepCount = 0;
    while (1) {
        sleep(1);

        sleepCount++;
        if (sleepCount == HOUSECLEANING_PERIOD) {
            _houseCleaning();
            sleepCount = 0;
        }
        else {
            // // this idiom leads to mem_yield() which allows other process to lock the persistence file
            // xl_open();
            // xl_close();
        }
    }
    dputs("%s", "server stopped.");
    return EXIT_SUCCESS;
}
