#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "flags.h"

// load or create global variables
uint32_t app_flags = 0;
uint32_t port_addr = 8080;
char *cgi_addr;
char *log_addr;

int checkPortNumber(char *port)
{
    for (int i = 0; i < (int)strlen(port); i++)
    {
        if (!isdigit(port[i]))
        {
            return 0;
        }
    }

    return 1;
}

int setFlags(const int argc, char *argv[])
{
    if (argc < 2)
        return 0;

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] != '-')
        {
            continue;
        }

        if (strcmp(argv[i], "-c") == 0)
        {
            if (i == argc - 1)
            {
                printf(
                    "Invalid CGI dir\nProvide the cgi-bin address after -c flag. "
                    "Eg: ./simple_server -c ./cgi-bin\n");
                exit(EXIT_FAILURE);
            }

            cgi_addr = argv[i + 1];

            struct stat st;
            if ((stat(cgi_addr, &st) != 0) || !(S_ISDIR(st.st_mode)))
            {
                printf("%s is not a valid directory\n", cgi_addr);
                exit(EXIT_FAILURE);
            }

            app_flags |= C_FLAG;

            i++;
            continue;
        }

        if (strcmp(argv[i], "-d") == 0)
        {
            app_flags |= D_FLAG;
            continue;
        }

        if (strcmp(argv[i], "-l") == 0)
        {
            if (i == argc - 1 || argv[i + 1][0] == '-')
            {
                printf(
                    "Invalid log file \nProvide the a valid address for log file -l "
                    "flag. Eg: -l ./simple-server.log\n");
                exit(EXIT_FAILURE);
            }

            log_addr = argv[i + 1];
            FILE *log_ptr = fopen(log_addr, "a");
            if (log_ptr == NULL)
            {
                perror("Unable to create logfile: ");
                exit(EXIT_FAILURE);
            }

            fclose(log_ptr);

            app_flags |= L_FLAG;

            i++;
            continue;
        }

        if (strcmp(argv[i], "-p") == 0)
        {
            if (i == argc - 1 || checkPortNumber(argv[i + 1]) == 0)
            {
                printf(
                    "Invalid port number\nProvide port number after -p flag. Eg: -p "
                    "8080\n");
                exit(EXIT_FAILURE);
            }

            int port = atoi(argv[i + 1]);

            if (port <= 1024)
            {
                if (geteuid() != 0)
                {
                    printf("Invalid port number\nOnly root can set port below 1024\n");
                    exit(EXIT_FAILURE);
                }
            }

            port_addr = port;

            i++;
            continue;
        }
    }

    return 0;
}
