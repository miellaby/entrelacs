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
#include "log.h"
#include "server.h"
#include "entrelacs/entrelacs.h"
#include "session.h"
#include "mongoose.h"
#include <string.h>
#include <sys/stat.h> 
#include <sys/mman.h> /* mmap() is defined in this header */

#ifdef DEBUG
#define SESSION_TTL 120
#define HOUSECLEANING_PERIOD 6
#else
#define SESSION_TTL 800
#define HOUSECLEANING_PERIOD 60
#endif

static Arrow fdatom(int fd) {
 Arrow a = EVE;
 if (fd < 0) return EVE;
 
 struct stat stat;
 int r = fstat (fd, &stat);
 assert (r >= 0);

 size_t size = stat.st_size;
 if (size > 0) {
  char *data = mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
  assert (data != (void*) -1);
  
  a = xl_atomn(size, data);
  munmap(data, size);
 }

 return a;
}

static const char *HTTP_500 = "HTTP/1.0 500 Server Error\r\n\r\n";

static Arrow assimilateUploadedData(struct mg_connection *conn) {
  const char* contentLengthHeader;
  char postData[16 * 1024], path[999], fileName[1024], mimeType[100],
       buf[BUFSIZ], *eop, *s, *p;
  FILE *stream;
  int64_t contentLength, written;
  int n, postDataLength;

  // Figure out total content length. Return if it is not present or invalid.
  contentLengthHeader = mg_get_header(conn, "Content-Length");
  if (contentLengthHeader == NULL)
      return NIL;

  if ((contentLength = strtoll(contentLengthHeader, NULL, 10)) < 0) {
    mg_printf(conn, "%s%s", HTTP_500, "Invalid Content-Length");
    return NIL;
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
      return NIL;
  
  // Finished parsing headers. Now "p" points to the first byte of data.
  // Calculate file size
  contentLength -= p - postData;      // Subtract headers size
  contentLength -= strlen(postData);  // Subtract the boundary marker at the end
  contentLength -= 6;                  // Subtract "\r\n" before and after boundary

 LOGPRINTF(LOG_WARN, "upload content-length %d", contentLength);

  if (contentLength <= 0) {
    // Empty file
    return xl_pair(xl_atom("Content-Typed"),
                        xl_pair(xl_atom(mimeType), EVE));
  }

  stream = tmpfile();
  
  if (stream < 0) {
    LOGPRINTF(LOG_ERROR, "Cannot create tmp file");
    return EVE;
  } else {
  
    // Success. Write data into the file.
    eop = postData + postDataLength;
    n = p + contentLength > eop ? (int) (eop - p) : (int) contentLength;
    (void) fwrite(p, 1, n, stream);
    written = n;
    while (written < contentLength &&
           (n = mg_read(conn, buf, contentLength - written > (int64_t) sizeof(buf) ?
                        sizeof(buf) : contentLength - written)) > 0) {
      (void) fwrite(buf, 1, n, stream);
      written += n;
    }
    rewind(stream);
    Arrow arrow = fdatom(fileno(stream));
    fclose(stream);

    arrow = xl_pair(xl_atom("Content-Typed"),
                        xl_pair(xl_atom(mimeType), arrow));
    return arrow;
  }
}

// Get session object for the connection. Caller must hold the lock
// HTTP Cookie "session" contains the session id.
// Session is valid if "/server+$id" is rooted within some context (whose bottom context is "sessions")

static Arrow get_connection_session(const struct mg_connection *conn) {
    char session_uuid[100];

    mg_get_cookie(conn, "session", session_uuid, sizeof(session_uuid));
    if (*session_uuid == '\0') {
        dputs("No session cookie");
        return EVE;
    }
    session_uuid[32] = '\0';

    // TODO: one should look for any session whatever it's top-level or it's embedded in a upper context.
    // TODO: remove the first parameter of sessionMaybe
    Arrow session = xls_sessionMaybe(EVE, xl_atom("server"), xl_atom(session_uuid));
    if (session == EVE) {
        dputs("Unknown session cookie %s", session_uuid);
    } else {
        dputs("Session %s found", session_uuid);
    }
    return session;
}

static void get_qsvar(const struct mg_request_info *request_info,
                      const char *name, char *dst, size_t dst_len) {
  const char *qs = request_info->query_string;
  mg_get_var(qs, strlen(qs == NULL ? "" : qs), name, dst, dst_len);
  dputs(dst_len == -1 ? "no %s variable in query string" :
         "query string variable %s = '%.*s'", name, dst_len, dst);
}

static void *event_handler(enum mg_event event,
                           struct mg_connection *conn,
                           const struct mg_request_info *request_info) {
    void *processed = "yes";
    xl_begin();
    if (event == MG_NEW_REQUEST) {
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
            session_id[32] = '\0';

            dputs("New session with id %s", session_id);

            // create session
            session = xls_session(EVE, xl_atom("server"), xl_atom(session_id));
        } else {
            session_id = xl_strOf(xl_headOf(xl_headOf(xl_headOf(session))));
        }

        dputs("session arrow is %O", session);
        time_t now = time(NULL) + SESSION_TTL;
        xls_set(session, xl_atom("expire"), xl_atomn(sizeof(time_t), (char *)&now));

        Arrow input = xls_url(session, request_info->uri);
        dputs("input %s assimilated as %O", request_info->uri, input);
        if (input == NIL) {
            free(session_id);
            char* origin = mg_get_header(conn, "Origin") ;
            mg_printf(conn, "HTTP/1.1 %d %s\r\n"
                      "Access-Control-Allow-Origin: %s\r\n"
                      "Access-Control-Allow-Credentials: true\r\n"
                      "Access-Control-Allow-Credentials: true\r\n"
                      "Content-Type: text/plain\r\n"
                      "Content-Length: 0\r\n"
                      "\r\n", 400, origin ? origin : "*",
                      "BAD REQUEST");
            mg_write(conn, "", (size_t)0);
            xl_over();
            return processed;
        }
        Arrow method = xl_atom(request_info->request_method);
        if (method == xl_atom("POST") || method == xl_atom("PUT")) {
            Arrow body = assimilateUploadedData(conn);
            if (body != NIL) {
                input = xl_pair(input, body);
            }
        }
        
        Arrow sessionContext = xls_get(EVE, session);
        if (sessionContext == NIL)
            sessionContext = session;
        Arrow output = xl_eval(sessionContext, xl_pair(method, input), session);

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

        char* output_url = xls_urlOf(session, output, l_depth);
        int isAtomic  = xl_isAtom(output);

#if 0 // FIXME
#define URI_CONTENT_TYPE "text/uri-list"
#else
#define URI_CONTENT_TYPE "text/plain"
#endif
        char* content_type =
                (i_depth == 0
                 ? URI_CONTENT_TYPE
                 : (isAtomic 
                       ? "text/plain"
                       : URI_CONTENT_TYPE));
        char* contentTypeCopy = NULL;
        if (i_depth != 0 && !isAtomic && xl_tailOf(output) == xl_atom("Content-Typed")) {
            contentTypeCopy = xl_strOf(xl_tailOf(xl_headOf(output)));
            content_type = contentTypeCopy;
            output = xl_headOf(xl_headOf(output));
            dputs("Content-Type: %s, output: %O", content_type, output);
//        } else if (isAtomic) {
//            Arrow application = xl_pair(xl_atom("Content-Type"), xl_pair(xl_atom("escape"), output));
//            Arrow rta = xl_eval(session, application);
//            if (rta != EVE && xl_isAtom(rta)) {
//               contentTypeCopy = xl_strOf(rta);
//               content_type = contentTypeCopy;
//            }
        }
        uint32_t content_length;
        char* content = NULL;
        if (i_depth != 0) {
            content = xl_memOf(output, &content_length);
        }
        if (!content) {
            content = xls_urlOf(session, output, i_depth);
            content_length = strlen(content);
        }
        char* origin =  mg_get_header(conn, "Origin");
        mg_printf(conn, "HTTP/1.1 200 OK\r\nCache: no-cache\r\n"
                  "Access-Control-Allow-Origin: %s\r\n"
                  "Access-Control-Allow-Credentials: true\r\n"
                  "Content-Location: %s\r\n"
                  "Content-Length: %d\r\n",
                  origin ? origin : "*",
                  output_url,
                  content_length);
        if (content_type) {
            mg_printf(conn, "Content-Type: %s\r\n", content_type);
        }
        mg_printf(conn, "Set-Cookie: session=%s; max-age=60; path=/; http-only\r\n",  session_id);
        mg_printf(conn, "\r\n");
        mg_write(conn, content, (size_t)content_length);
        free(output_url);
        free(content);
        free(session_id);
        if (contentTypeCopy) free(contentTypeCopy);

    } else {
        processed = NULL;
    }
    //xl_commit();
    xl_over();
    return processed;
}


int main(void) {
  struct mg_context *ctx;
#ifdef DEBUG
  log_init(NULL, "space,session,machine,server=debug");
#else
#ifdef PRODUCTION
  log_init(NULL, "space,session,machine,server=warn");
#else
  log_init(NULL, "session,machine=info,server=trace,space=warn");
#endif
#endif

  xl_init();

  xl_begin();
  Arrow get = xls_get(EVE, xl_atom("GET"));
  if (get == NIL) {
      xls_set(EVE, xl_atom("GET"), xl_uri("/paddock//x+x+"));
      xls_set(EVE, xl_atom("PUT"), xl_uri("/paddock//x/arrow/set+/var+x+"));
      xls_set(EVE, xl_atom("POST"), xl_uri("/paddock//x+x+"));
      xls_set(EVE, xl_atom("DELETE"), xl_uri("/paddock//x/arrow/unset+/var+x+"));
      xl_commit();
  }

  {   // store secret as server-secret<->"/session-secret-pair-uri" hidden pair
      char* server_secret_sha1 = getenv("ENTRELACS_SECRET"); // TODO better solution
      if (!server_secret_sha1) server_secret_sha1 = "8f84b95af52fbfae67209b6cfd3ab7dd1f1e0b12";
      // meta-user do : mudo
      xls_set(EVE, xl_pair(xl_atom("mudo"), xl_atom(server_secret_sha1)), xl_atom("eval"));
      // TODO unroot while house cleaning
      xl_commit();
  }
  xl_over();

  // Initialize random number generator. It will be used later on for
  // the session identifier creation.
  srand((unsigned) time(0));

  // Setup and start Mongoose
  ctx = mg_start(&event_handler, NULL, server_options);
  assert(ctx != NULL);

  dputs("server started on ports %s.",
         mg_get_option(ctx, "listening_ports"));


  // TODO deep house cleaning at start-up
  while (1) {
      sleep(HOUSECLEANING_PERIOD);
      
      xl_begin();

      time_t now = time(NULL);
      dputs("House Cleaning ...");

      Arrow sessionTag = xl_atom("session");
      XLEnum e = xl_childrenOf(sessionTag);

      Arrow next = e && xl_enumNext(e) ? xl_enumGet(e) : EVE;
      while (next != EVE) {
          Arrow session = next;
          next = xl_enumNext(e) ? xl_enumGet(e) : EVE;

          if (xl_tailOf(session) != sessionTag)
              continue; // Not a /session+* arrow
          session = xls_isRooted(EVE, session);
          if (session == EVE)
              continue; // session is not rooted

          Arrow expire = xls_get(session, xl_atom("expire"));
          uint32_t var_size = 0;
          time_t* expire_time = (expire != NIL ? (time_t *)xl_memOf(expire, &var_size) : NULL);

          if (expire == NIL || var_size != sizeof(time_t)) {
              LOGPRINTF(LOG_WARN, "session %O : wrong 'expire'", session);
              xls_close(session);
              // restart loop as deep close may remove in-enum arrow
              xl_freeEnum(e);
              sessionTag =  xl_atom("session");
              e = xl_childrenOf(sessionTag);
              next = e && xl_enumNext(e) ? xl_enumGet(e) : EVE;
          } else if (*expire_time < now) {
              dputs("session %O outdated.", session);
              xls_close(session);
              // restart loop as deep close may remove in-enum arrow
              sessionTag =  xl_atom("session");
              e = xl_childrenOf(sessionTag);
              next = e && xl_enumNext(e) ? xl_enumGet(e) : EVE;
          }
      
      }
      xl_over();
      xl_freeEnum(e);
      dputs("House Cleaning done.");
  }
  dputs("%s", "server stopped.");

  xl_over();

  return EXIT_SUCCESS;
}
