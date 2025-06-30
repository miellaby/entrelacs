/* 
 * File:   server.h
 * Author: Garden Sylvain
 */

#ifndef SERVER_H
#define	SERVER_H

#ifdef	__cplusplus
extern "C" {
#endif
    
extern const char *server_options[];

#ifdef SERVER_C
const char *server_options[] = {
  "document_root", "html",
  "listening_ports", "8080", // "8008,4433s" activates SSL redirection
  //"ssl_certificate", "ssl_cert.pem",
  "num_threads", "5",
  NULL
};
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* SERVER_H */

