#include <netinet/in.h>
#include <sys/stat.h>

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "flags.h"

// load or create global variables
uint32_t app_flags = 0;
uint32_t port_addr = 8080;
char *bind_addr4;
char *bind_addr6;
char *cgi_addr;
char *log_addr;

/**
 * @brief Validate port number string contains only digits
 *
 * Examines each character in the provided string to ensure it contains
 * only numeric digits (0-9). Used for validating port numbers before
 * conversion to integer.
 *
 * @param[in] port Null-terminated string to validate
 *
 * @retval 1 String contains only digits
 * @retval 0 String contains non-digit characters
 *
 * @note Does not validate port number range (1-65535)
 * @note Returns 0 for empty strings
 *
 * @see setFlags()
 */
int
checkPortNumber(char *port)
{
	for (int i = 0; i < (int)strlen(port); i++) {
		if (!isdigit(port[i])) {
			return (0);
		}
	}

	return (1);
}

/**
 * @brief Parse and validate command line arguments
 *
 * Processes command line flags to configure server behavior including
 * port number, CGI directory, log file, and debug mode. Validates all
 * arguments and sets global configuration flags and variables.
 *
 * Supported flags:
 * - `-c <dir>`: Enable CGI execution from specified directory
 * - `-d`: Enable debug mode (no daemon, single connection)
 * - `-l <file>`: Enable logging to specified file
 * - `-p <port>`: Set listen port (default 8080)
 *
 * @param[in] argc Number of command line arguments
 * @param[in] argv Array of command line argument strings
 *
 * @retval 0 Success
 * @retval -1 Invalid arguments (never returned - exits on error)
 *
 * @note Exits program on invalid arguments
 * @note Requires root privileges for ports below 1024
 * @note Validates CGI directory exists and is readable
 * @note Creates log file if it doesn't exist
 *
 * @see checkPortNumber()
 */
int
setFlags(const int argc, char *argv[])
{
	if (argc < 2)
		return (0);

	for (int i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			continue;
		}

		if (strcmp(argv[i], "-c") == 0) {
			if (i == argc - 1) {
				printf("Invalid CGI dir\nProvide the cgi-bin "
				       "address after -c flag. "
				       "Eg: ./simple_server -c ./cgi-bin\n");
				exit(EXIT_FAILURE);
			}

			cgi_addr = argv[i + 1];

			struct stat st;
			if ((stat(cgi_addr, &st) != 0) ||
			    !(S_ISDIR(st.st_mode))) {
				printf("%s is not a valid directory\n",
				    cgi_addr);
				exit(EXIT_FAILURE);
			}

			app_flags |= C_FLAG;

			i++;
			continue;
		}

		else if (strcmp(argv[i], "-v") == 0) {
			app_flags |= V_FLAG;
			continue;
		}

		else if (strcmp(argv[i], "-bind4") == 0) {
			if (i == argc - 1 || argv[i + 1][0] == '-') {
				printf(
				    "Invalid bind address \nProvide the a valid "
				    "IPv4 address for bind "
				    "flag. Eg: -bind4 192.168.1.1\n");
				exit(EXIT_FAILURE);
			}

			i++;
			bind_addr4 = argv[i];

			struct in_addr tmp4;

			if (inet_pton(AF_INET, bind_addr4, &tmp4) != 1) {
				fprintf(stderr, "Invalid IPv4 address\n");
				exit(EXIT_FAILURE);
			}

			app_flags |= B4_FLAG;
		}

		else if (strcmp(argv[i], "-bind6") == 0) {
			if (i == argc - 1 || argv[i + 1][0] == '-') {
				printf(
				    "Invalid bind address \nProvide the a valid "
				    "IPv6 address for bind "
				    "flag. Eg: -bind6 2001:db8::20\n");
				exit(EXIT_FAILURE);
			}

			i++;
			bind_addr6 = argv[i];

			struct in6_addr tmp6;

			if (inet_pton(AF_INET6, bind_addr6, &tmp6) != 1) {
				fprintf(stderr, "Invalid IPv6 address\n");
				exit(EXIT_FAILURE);
			}

			app_flags |= B6_FLAG;
		}

		else if (strcmp(argv[i], "-l") == 0) {
			if (i == argc - 1 || argv[i + 1][0] == '-') {
				printf("Invalid log file \nProvide the a valid "
				       "address for log "
				       "file -l "
				       "flag. Eg: -l ./simple-server.log\n");
				exit(EXIT_FAILURE);
			}

			log_addr = argv[i + 1];
			FILE *log_ptr = fopen(log_addr, "a");
			if (log_ptr == NULL) {
				perror("Unable to create logfile: ");
				exit(EXIT_FAILURE);
			}

			fclose(log_ptr);

			app_flags |= L_FLAG;

			i++;
			continue;
		}

		else if (strcmp(argv[i], "-p") == 0) {
			if (i == argc - 1 ||
			    checkPortNumber(argv[i + 1]) == 0) {
				printf("Invalid port number\nProvide port "
				       "number after -p flag. "
				       "Eg: -p "
				       "8080\n");
				exit(EXIT_FAILURE);
			}

			int port = atoi(argv[i + 1]);

			if (port <= 1024) {
				if (geteuid() != 0) {
					printf("Invalid port number\nOnly root "
					       "can set port below "
					       "1024\n");
					exit(EXIT_FAILURE);
				}
			}

			port_addr = port;

			i++;
			continue;
		}
		else {
			printf("Invalid flag %s\n", argv[i]);
			exit(EXIT_FAILURE);
		}
	}

	return (0);
}
