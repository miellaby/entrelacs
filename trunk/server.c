/*
 * Modified copy of Mongoose project example, http://code.google.com/p/mongoose
 *  1. type "make server" in the directory where this file lives
 *  2. run entrelacs.d and make request to https://127.0.0.1:8008
 *
 * NOTE(lsm): this file follows Google style, not BSD style as the rest of
 * Mongoose code.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <pthread.h>
#include "entrelacs/entrelacs.h"
#include "session.h"
#include "mongoose.h"

#define MAX_SESSIONS 20
#define SESSION_TTL 120

// Describes web session.
struct session {
  char session_id[33];      // Session ID, must be unique
  char random[20];          // Random data used (secret?)
  time_t expire;            // Expiration timestamp, UTC
};

static struct session sessions[MAX_SESSIONS];  // Current sessions

// Protects everything.
static pthread_mutex_t mutex;

// Get session object for the connection. Caller must hold the lock.
static struct session *get_session(const struct mg_connection *conn) {
  int i;
  char session_id[33];
  time_t now = time(NULL);
  mg_get_cookie(conn, "session", session_id, sizeof(session_id));
  for (i = 0; i < MAX_SESSIONS; i++) {
    if (sessions[i].expire != 0 &&
        sessions[i].expire > now &&
        strcmp(sessions[i].session_id, session_id) == 0) {
      break;
    }
  }
  return i == MAX_SESSIONS ? NULL : &sessions[i];
}

static void get_qsvar(const struct mg_request_info *request_info,
                      const char *name, char *dst, size_t dst_len) {
  const char *qs = request_info->query_string;
  mg_get_var(qs, strlen(qs == NULL ? "" : qs), name, dst, dst_len);
}

static void my_strlcpy(char *dst, const char *src, size_t len) {
  strncpy(dst, src, len);
  dst[len - 1] = '\0';
}

// Allocate new session object
static struct session *new_session(void) {
  int i;
  time_t now = time(NULL);
  for (i = 0; i < MAX_SESSIONS; i++) {
    if (sessions[i].expire == 0 || sessions[i].expire < now) {
      sessions[i].expire = time(0) + SESSION_TTL;
      break;
    }
  }
  return i == MAX_SESSIONS ? NULL : &sessions[i];
}

// Generate session ID. buf must be 33 bytes in size.
// Note that it is easy to steal session cookies by sniffing traffic.
// This is why all communication must be SSL-ed.
static void generate_session_id(char *buf, const char *random,
                                const char *user) {
  mg_md5(buf, random, user, NULL);
}

static void *event_handler(enum mg_event event,
                           struct mg_connection *conn,
                           const struct mg_request_info *request_info) {
    void *processed = "yes";

    if (event == MG_NEW_REQUEST) {
        pthread_mutex_lock (&mutex);
        struct session *aSession = get_session(conn);
        if (aSession == NULL) {
            aSession = new_session();
            snprintf(aSession->random, sizeof(aSession->random), "%d", rand());
            generate_session_id(aSession->session_id, aSession->random, "session->user");
        }
        Arrow session = xls_session(xl_tag("server"), xl_tag(aSession->session_id));
        Arrow inputHandler = xl_childOf(xl_arrow(session, xl_tag("inputHandler")));
        if (xl_isEve(inputHandler)) {
            inputHandler = xl_eval(xl_Eve(), xl_uri("/let//GET/lambda/x.x"
                                                    "/let//POST/lambda/x/eval/x"
                                                    "/let//PUT/lambda/x/root/x"
                                                    "/let//DELETE/lambda/x/unroot/x"
                                                    "/lambda/x/lambda/y/x/y"));
            xl_root(xl_arrow(session, xl_arrow(xl_arrow(session, xl_tag("inputHandler")), inputHandler)));
        }

        Arrow input = xls_url(session, request_info->uri);
        Arrow method = xl_tag(request_info->request_method);
        Arrow r = xl_eval(xl_Eve(), xl_arrow(xl_arrow(inputHandler, method), input));

        int iDepth = -1;
        int lDepth = 0;

        char iDepthBuf[255], lDepthBuf[255];
        get_qsvar(request_info, "iDepth", iDepthBuf, 254);
        if (iDepthBuf[0]) { // Non empty string
            iDepth = atoi(iDepthBuf);
        }

        get_qsvar(request_info, "lDepth", lDepthBuf, 254);
        if (lDepthBuf[0]) {
            lDepth = atoi(lDepthBuf);
        }

        char* outputUrl = xls_urlOf(session, r, lDepth);
        XLType rt = xl_typeOf(r);
        char* contentType =
                (iDepth == 0
                 ? "text/uri-list"
                 : (rt == XL_BLOB
                    ? NULL /* No Content-Type */
                    : (rt == XL_TAG
                       ? "text/plain"
                       : "text/uri-list")));
        uint32_t contentLength;
        char* content =
                (iDepth == 0
                 ? NULL
                 : (rt == XL_BLOB
                    ?xl_blobOf(r, &contentLength)
                      : (rt == XL_TAG
                         ? xl_btagOf(r, &contentLength)
                         : NULL)));
        if (!content) {
            char* content = xls_urlOf(session, r, iDepth);
            contentLength = strlen(content);
        }
        pthread_mutex_unlock (&mutex);

        mg_printf(conn, "HTTP/1.1 200 OK\r\nCache: no-cache\r\n"
                  "Content-Location: %s\r\n"
                  "Content-Length: %d\r\n",
                  outputUrl,
                  contentLength);
        if (contentType) {
            mg_printf(conn, "Content-Type: %s\r\n", contentType);
        }
        mg_printf(conn, "Set-Cookie: session=%s; max-age=3600; http-only\r\n",  aSession->session_id);
        mg_printf(conn, "\r\n");
        mg_write(conn, content, (size_t)contentLength);

    } else {
        processed = NULL;
    }

    return processed;
}

static const char *options[] = {
  "document_root", "html",
  "listening_ports", "8008,4433s", // "8008,4433s" activates SSL redirection
  "ssl_certificate", "ssl_cert.pem",
  "num_threads", "5",
  NULL
};

int main(void) {
  struct mg_context *ctx;
  pthread_mutex_init(&mutex, NULL);

  xl_init();

  // Initialize random number generator. It will be used later on for
  // the session identifier creation.
  srand((unsigned) time(0));

  // Setup and start Mongoose
  ctx = mg_start(&event_handler, NULL, options);
  assert(ctx != NULL);

  // Wait until enter is pressed, then exit
  printf("server started on ports %s, press enter to quit.\n",
         mg_get_option(ctx, "listening_ports"));
  getchar();
  mg_stop(ctx);
  printf("%s\n", "server stopped.");

  pthread_mutex_destroy(&mutex);

  return EXIT_SUCCESS;
}

// vim:ts=2:sw=2:et
