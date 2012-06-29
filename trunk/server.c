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
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <pthread.h>
#include "log.h"
#include "entrelacs/entrelacs.h"
#include "session.h"
#include "mongoose.h"
#include <string.h>
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
// SGA:This is cut&paste code. I should leverage on the arrow space
// session_id-->session_cb
static struct session *get_session(const struct mg_connection *conn) {
    struct session* s = NULL;
    int i;
    char session_id[33];
    time_t now = time(NULL);
    mg_get_cookie(conn, "session", session_id, sizeof(session_id));
    for (i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].expire != 0 &&
                strcmp(sessions[i].session_id, session_id) == 0) {
            sessions[i].expire = now + SESSION_TTL; // expire counter renewal
            s = &sessions[i];
            break;
        }
    }
    DEBUGPRINTF(s == NULL ? "conn %p invalid session %s\n" : "conn %p session %s found\n" , conn, session_id);
    return s;
}

static void get_qsvar(const struct mg_request_info *request_info,
                      const char *name, char *dst, size_t dst_len) {
  const char *qs = request_info->query_string;
  mg_get_var(qs, strlen(qs == NULL ? "" : qs), name, dst, dst_len);
  DEBUGPRINTF(dst_len == -1 ? "no %s variable in query string\n" :
         "query string variable %s = '%.*s'\n", name, dst_len, dst);
}

static void my_strlcpy(char *dst, const char *src, size_t len) {
  strncpy(dst, src, len);
  dst[len - 1] = '\0';
}

// Allocate new session object
static struct session *new_session(void) {
  time_t now = time(NULL);
  struct session* s = NULL;
  int i = 0;
  for (i = 0; i < MAX_SESSIONS; i++) {
    if (sessions[i].expire == 0) {
      sessions[i].expire = now + SESSION_TTL;
      s = &sessions[i];
      break;
    }
  }
  DEBUGPRINTF(s == NULL ? "WARNING: can't allocate session\n" : "new session %p\n", s);
  return s;
}

// Generate session ID. buf must be 33 bytes in size.
// Note that it is easy to steal session cookies by sniffing traffic.
// This is why all communication must be SSL-ed.
static void generate_session_id(char *buf, const char *random,
                                const char *user) {
  mg_md5(buf, random, user, NULL);
  DEBUGPRINTF("generated session ID = %s\n", buf);
}

static void *event_handler(enum mg_event event,
                           struct mg_connection *conn,
                           const struct mg_request_info *request_info) {
    void *processed = "yes";
    Arrow session;

    if (event == MG_NEW_REQUEST) {
        pthread_mutex_lock (&mutex);
        // TODO: Here there could be a global server super-session. Use the API
        struct session *aSession = get_session(conn);
        if (aSession == NULL) {
            aSession = new_session();
            snprintf(aSession->random, sizeof(aSession->random), "%d", rand());
            generate_session_id(aSession->session_id, aSession->random, "server");
            session = xls_session(EVE, xl_tag("server"), xl_tag(aSession->session_id));
            char* sessionUri = xl_uriOf(session);
            char* server_secret_s = getenv("ENTRELACS_SECRET"); // TODO better solution
            if (!server_secret_s) server_secret_s = "chut";
            char secret_s[256];
            snprintf(secret_s, 255, "%s=%s", sessionUri, server_secret_s);
            xl_root(xl_arrow(xl_tag(server_secret_s), xl_tag(secret_s)));
            xl_commit();
            free(sessionUri);
        } else
            session = xls_session(EVE, xl_tag("server"), xl_tag(aSession->session_id));


        DEBUGPRINTF("session arrow is %O\n", session);
        Arrow get = xls_get(session, xl_tag("GET"));
        if (get == EVE) {
            xls_set(session, xl_tag("GET"), xl_eval(EVE, xl_uri("/lambda/x.x")));
            xls_set(session, xl_tag("PUT"), xl_eval(EVE, xl_uri("/lambda/x/root.x")));
            xls_set(session, xl_tag("POST"), xl_eval(EVE, xl_uri("/lambda/x/eval.x")));
            xls_set(session, xl_tag("DELETE"), xl_eval(EVE, xl_uri("/lambda/x/unroot.x")));
            xl_commit();
        }

        Arrow input = xls_url(session, request_info->uri);
        DEBUGPRINTF("input %s assimilated as %O\n", request_info->uri, input);

        Arrow method = xl_tag(request_info->request_method);
        Arrow r = xl_eval(session, xl_arrow(method, input));

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
        char* contentTypeCopy = NULL;
        Arrow rtas = xl_arrowMaybe(xl_tag("Content-Type"), r);
        if (rtas != EVE) {
            Arrow rta = xls_get(session, rta);
            if (rta != EVE) {
                contentTypeCopy = xl_tagOf(rta);
                contentType = contentTypeCopy;
            }
        }
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
            content = xls_urlOf(session, r, iDepth);
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
        if (contentTypeCopy) free(contentTypeCopy);
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
  "listening_ports", "8008", // "8008,4433s" activates SSL redirection
  //"ssl_certificate", "ssl_cert.pem",
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
  DEBUGPRINTF("server started on ports %s.\n",
         mg_get_option(ctx, "listening_ports"));

  while (1) {
      sleep(60);
      time_t now = time(NULL);
      int i;
      for (i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].expire != 0 && sessions[i].expire < now) {
            sessions[i].expire = 0;
            DEBUGPRINTF("session %s outdated.\n", sessions[i].session_id);
            pthread_mutex_lock (&mutex);
            Arrow session = xls_session(EVE, xl_tag("server"), xl_tag(sessions[i].session_id));
            xls_reset(session);
            pthread_mutex_unlock (&mutex);
        }
      }
      xl_commit();
  }
  DEBUGPRINTF("%s\n", "server stopped.");

  pthread_mutex_destroy(&mutex);

  return EXIT_SUCCESS;
}

// vim:ts=2:sw=2:et
