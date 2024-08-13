#ifndef FLAGS_H
#define FLAGS_H

#define C_FLAG                                                                 \
  0x1 // Allow execution of CGIs from the given directory. See CGIs for details.

#define D_FLAG                                                                 \
  0x2 // Enter debugging mode. That is, do not daemonize, only accept one
      // connection at a time and enagble logging to stdout.

#define H_FLAG 0x4 // Print a short usage summary and exit

#define I_FLAG                                                                 \
  0x8 // Bind to the given IPv4 or IPv6 address. If not provided, sws will
      // listen on all IPv4 and IPv6 addresses on this host

#define L_flag 0x10 // Log all requests to the given file

#define P_FLAG                                                                 \
  0x20 // listen on the given port. If not provided, sws will listen on port
       // 8080

#endif // FLAGS_H
