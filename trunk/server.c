/*
 * Modified copy of Mongoose project example, http://code.google.com/p/mongoose
 *  1. type "make server" in the directory where this file lives
 *  2. run entrelacs.d and make request to http://127.0.0.1:8008
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
#define LOG_CURRENT LOG_SERVER
#include "log.h"
#include "entrelacs/entrelacs.h"
#include "session.h"
#include "mongoose.h"
#include <string.h>
#ifdef DEBUG
#define SESSION_TTL 12
#define HOUSECLEANING_PERIOD 6
#else
#define SESSION_TTL 120
#define HOUSECLEANING_PERIOD 60
#endif

// Protects everything.
static pthread_mutex_t mutex;

// Get session object for the connection. Caller must hold the lock.
// SGA:This is cut&paste code. I should leverage on the arrow space
// session_id-->session_cb
static Arrow get_connection_session(const struct mg_connection *conn) {
    char session_uuid[100];

    mg_get_cookie(conn, "session", session_uuid, sizeof(session_uuid));
    if (*session_uuid == '\0') {
        DEBUGPRINTF("No session cookie");
        return EVE;
    }

    Arrow session = xls_sessionMaybe(EVE, xl_tag("server"), xl_uri(session_uuid));
    if (session == EVE) {
        DEBUGPRINTF("Unknown session cookie %s", session_uuid);
    } else {
        DEBUGPRINTF("Session %s found", session_uuid);
    }
    return session;
}

static void get_qsvar(const struct mg_request_info *request_info,
                      const char *name, char *dst, size_t dst_len) {
  const char *qs = request_info->query_string;
  mg_get_var(qs, strlen(qs == NULL ? "" : qs), name, dst, dst_len);
  DEBUGPRINTF(dst_len == -1 ? "no %s variable in query string" :
         "query string variable %s = '%.*s'", name, dst_len, dst);
}

static void *event_handler(enum mg_event event,
                           struct mg_connection *conn,
                           const struct mg_request_info *request_info) {
    void *processed = "yes";

    if (event == MG_NEW_REQUEST) {
        pthread_mutex_lock (&mutex);
        Arrow session = get_connection_session(conn);
        char* session_id;
        if (session == EVE) {
            // generate session_id
            // Note that it is easy to steal session cookies by sniffing traffic.
            // This is why all communication must be SSL-ed.
            char random[20];
            session_id = (char *)malloc(33 * sizeof(char));
            assert(session_id);
            snprintf(random, sizeof(random), "%d", rand());
            mg_md5(session_id, random, "server", NULL);
            DEBUGPRINTF("New session with id %s", session_id);

            // create session
            session = xls_session(EVE, xl_tag("server"), xl_tag(session_id));

            // store secret as server-secret<->"/session-secret-pair-uri" hidden pair
            // TODO : do as commented root "/%O./%O.%O", server-secret, session, session-secret
            char* sessionUri = xl_uriOf(session);
            char* server_secret_s = getenv("ENTRELACS_SECRET"); // TODO better solution
            if (!server_secret_s) server_secret_s = "chut";
            char secret_s[256];
            snprintf(secret_s, 255, "%s=%s", sessionUri, server_secret_s);
            xl_root(xl_arrow(xl_tag(server_secret_s), xl_tag(secret_s)));
            // TODO unroot while house cleaning
            xl_commit();
            free(sessionUri);
        } else {
            session_id = xl_tagOf(xl_headOf(xl_headOf(xl_headOf(session))));
        }

        DEBUGPRINTF("session arrow is %O", session);
        time_t now = time(NULL) + SESSION_TTL;
        xls_set(session, xl_tag("expire"), xl_btag(sizeof(time_t), (char *)&now));

        Arrow input = xls_url(session, request_info->uri);
        DEBUGPRINTF("input %s assimilated as %O", request_info->uri, input);
        if (input == NIL) {
            pthread_mutex_unlock (&mutex);
            free(session_id);
            mg_printf(conn, "HTTP/1.1 %d %s\r\n"
                      "Content-Type: text/plain\r\n"
                      "Content-Length: 0\r\n"
                      "\r\n", 400, "BAD REQUEST");
            mg_write(conn, "", (size_t)0);
            return processed;
        }

        Arrow method = xl_tag(request_info->request_method);
        Arrow r = xl_eval(session, xl_arrow(method, input));


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

        char* output_url = xls_urlOf(session, r, l_depth);
        XLType rt = xl_typeOf(r);
        char* content_type =
                (i_depth == 0
                 ? "text/uri-list"
                 : (rt == XL_BLOB
                    ? NULL /* No Content-Type */
                    : (rt == XL_TAG
                       ? "text/plain"
                       : "text/uri-list")));
        char* contentTypeCopy = NULL;
        Arrow rta = xl_eval(session, xl_arrow(xl_tag("Content-Type"), r));
        if (rta != EVE) {
            contentTypeCopy = xl_tagOf(rta);
            content_type = contentTypeCopy;
        }
        uint32_t content_length;
        char* content = NULL;
        if (i_depth > 0) {
            if (rt == XL_BLOB) {
                content = xl_blobOf(r, &content_length);
            } else if (rt == XL_TAG) {
                content = xl_btagOf(r, &content_length);
            }
        }
        if (!content) {
            content = xls_urlOf(session, r, i_depth);
            content_length = strlen(content);
        }
        pthread_mutex_unlock (&mutex);

        mg_printf(conn, "HTTP/1.1 200 OK\r\nCache: no-cache\r\n"
                  "Content-Location: %s\r\n"
                  "Content-Length: %d\r\n",
                  output_url,
                  content_length);
        if (content_type) {
            mg_printf(conn, "Content-Type: %s\r\n", content_type);
        }
        mg_printf(conn, "Set-Cookie: session=%s; max-age=60; http-only\r\n",  session_id);
        mg_printf(conn, "\r\n");
        mg_write(conn, content, (size_t)content_length);
        free(output_url);
        free(content);
        free(session_id);
        if (contentTypeCopy) free(contentTypeCopy);

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

#ifndef PRODUCTION
  log_init(NULL, "server,session=debug,machine=warn");
#endif

  xl_init();

  Arrow get = xls_get(EVE, xl_tag("GET"));
  if (get == EVE) {
      xls_set(EVE, xl_tag("GET"), xl_eval(EVE, xl_uri("/lambda/x.x")));
      xls_set(EVE, xl_tag("PUT"), xl_eval(EVE, xl_uri("/lambda/x/root.x")));
      xls_set(EVE, xl_tag("POST"), xl_eval(EVE, xl_uri("/lambda/x/eval.x")));
      xls_set(EVE, xl_tag("DELETE"), xl_eval(EVE, xl_uri("/lambda/x/unroot.x")));
      xl_commit();
  }

  // Initialize random number generator. It will be used later on for
  // the session identifier creation.
  srand((unsigned) time(0));

  // Setup and start Mongoose
  ctx = mg_start(&event_handler, NULL, options);
  assert(ctx != NULL);

  // Wait until enter is pressed, then exit
  DEBUGPRINTF("server started on ports %s.",
         mg_get_option(ctx, "listening_ports"));

  // TODO deep house cleaning at start-up
  while (1) {
      sleep(HOUSECLEANING_PERIOD);
      time_t now = time(NULL);
      DEBUGPRINTF("House Cleaning ...");
      pthread_mutex_lock (&mutex);

      Arrow sessionTag = xl_tag("session");
      XLEnum e = xl_childrenOf(sessionTag);

      Arrow next = e && xl_enumNext(e) ? xl_enumGet(e) : EVE;
      while (next != EVE) {
          Arrow session = next;
          next = xl_enumNext(e) ? xl_enumGet(e) : EVE;

          if (xl_tailOf(session) != sessionTag)
              continue; // Not a /session.* arrow
          session = xls_isRooted(EVE, session);
          if (session == EVE)
              continue; // session is not rooted

          Arrow expire = xls_get(session, xl_tag("expire"));
          uint32_t var_size = 0;
          time_t* expire_time = (expire != EVE ? (time_t *)xl_btagOf(expire, &var_size) : NULL);

          if (expire == EVE || var_size != sizeof(time_t)) {
              LOGPRINTF(LOG_WARN, "session %O : wrong 'expire'", session);
              xls_close(session);
              // restart loop as deep close may remove in-enum arrow
              sessionTag =  xl_tag("session");
              e = xl_childrenOf(sessionTag);
              next = e && xl_enumNext(e) ? xl_enumGet(e) : EVE;
          } else if (*expire_time < now) {
              DEBUGPRINTF("session %O outdated.", session);
              xls_close(session);
              // restart loop as deep close may remove in-enum arrow
              sessionTag =  xl_tag("session");
              e = xl_childrenOf(sessionTag);
              next = e && xl_enumNext(e) ? xl_enumGet(e) : EVE;
          }
      }
      xl_commit();
      pthread_mutex_unlock (&mutex);
      DEBUGPRINTF("House Cleaning done.");
  }
  DEBUGPRINTF("%s", "server stopped.");

  pthread_mutex_destroy(&mutex);

  return EXIT_SUCCESS;
}
