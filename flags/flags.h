#ifndef FLAGS_H
#define FLAGS_H

/*  Allow execution of CGIs from the given directory. See CGIs for details  */
#define C_FLAG (1u << 0)

/*  Enter Verbose mode. That is, do not daemonize, only accept one
 *  connection at a time and enable logging to stdout. */
#define V_FLAG (1u << 1)

/*  Print a short usage summary and exit  */
#define H_FLAG (1u << 2)

/*  Bind to the given IPv4 address. If not provided, sws will
 *  listen on all IPv4 addresses on this host  */
#define B4_FLAG (1u << 3)

/*  Bind to the given IPv4 address. If not provided, sws will
 *  listen on all IPv4 addresses on this host  */
#define B6_FLAG (1u << 4)

/*  Log all requests to the given file  */
#define L_FLAG (1u << 5)

/*  listen on the given port. If not provided, sws will listen on port 8080 */
#define P_FLAG (1u << 6)

int setFlags(const int argc, char *argv[]);

#endif // FLAGS_H
