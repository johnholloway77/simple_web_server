#ifndef FLAGS_H
#define FLAGS_H

/*  Allow execution of CGIs from the given directory (-c <dir>)  */
#define C_FLAG (1u << 0)

/*  Verbose mode (-v): skip daemonizing and log to stdout  */
#define V_FLAG (1u << 1)

/*  Reserved — H_FLAG is defined but -h is not yet handled by setFlags()  */
#define H_FLAG (1u << 2)

/*  Bind IPv4 listener to a specific address (-bind4 <addr>).
 *  If not set, the IPv4 listener binds to INADDR_ANY.  */
#define B4_FLAG (1u << 3)

/*  Bind IPv6 listener to a specific address (-bind6 <addr>).
 *  If not set, the IPv6 listener binds to in6addr_any.  */
#define B6_FLAG (1u << 4)

/*  Log all requests to a file (-l <file>)  */
#define L_FLAG (1u << 5)

/*  Reserved — P_FLAG is defined but setFlags() stores the port in
 *  port_addr directly and never sets this bit  */
#define P_FLAG (1u << 6)

/**
 * @brief Parse command-line arguments and populate global configuration.
 *
 * Processes the argument vector and sets bits in the global @c app_flags
 * word and the associated address globals (@c cgi_addr, @c log_addr,
 * @c bind_addr4, @c bind_addr6, @c port_addr).
 *
 * Supported flags:
 *   -c <dir>       CGI directory (sets C_FLAG; validates dir exists)
 *   -v             Verbose / no-daemon mode (sets V_FLAG)
 *   -bind4 <addr>  IPv4 bind address (sets B4_FLAG; validated with inet_pton)
 *   -bind6 <addr>  IPv6 bind address (sets B6_FLAG; validated with inet_pton)
 *   -l <file>      Log file path (sets L_FLAG; file is opened/created to test
 *                  write access then immediately closed)
 *   -p <port>      Listen port (stored in port_addr; root required for < 1024)
 *
 * Exits the process on any invalid or missing argument.
 *
 * @param argc  Argument count from main()
 * @param argv  Argument vector from main()
 * @return      0 on success (never returns -1; exits on error)
 */
int setFlags(const int argc, char *argv[]);

#endif // FLAGS_H
