#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "mp4tree.h"
#include "options.h"

/*
 ******************************************************************************
 *                           Main functionality                               *
 ******************************************************************************
 */

int
process_file(const char * filename)
{
    uint8_t *   buf     = NULL;
    struct stat sb      = {0};
    int         fd      = -1;
    ssize_t     len     = 0;

    printf("Reading file %s\n", filename);
    if (stat(filename, &sb) < 0)
    {
        perror("stat");
        exit(EXIT_FAILURE);
    }

    buf = malloc(sb.st_size);

    if (buf == NULL)
    {
        printf("Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }

    fd = open(filename, 0);

    if (fd < 0)
    {
        perror("open");
        goto errout;
    }

    len = read(fd, buf, sb.st_size);

    printf("Read %zd bytes \n", len);
    if (len < 0)
    {
        perror("read");
        goto errout;
    }

    printf("File Content:\n");
    mp4tree_print(buf, len, 0);

    if (buf != NULL)
        free(buf);

    return EXIT_SUCCESS;

errout:

    if (buf != NULL)
        free(buf);

    return EXIT_FAILURE;
}


static int
mp4tree_parse_options(
    int         argc,
    char **     argv)
{
    int optix = 0;
    int c;
    static struct option options[] =
        {
            {"truncate", required_argument, 0, 't'},
            {"filter",   required_argument, 0, 'f'},
            {"help",     0,                 0, 'h'},
            {"selftest", 0,                 0, 's'},
            {0,          0,                 0,  0}
        };

    /* Set default options */
    memset(&g_options, 0, sizeof(g_options));
    g_options.truncate = 256;

    while (1)
    {
        c = getopt_long(argc, argv, "t:f:i:hs",
                        options, &optix);

        if (c == -1)
            break;

        switch (c)
        {
        case 't':
            g_options.truncate = atoi(optarg);
            break;
        case 'f':
            g_options.filter = optarg;
            break;
        case 's':
            g_options.selftest = true;
            return 0;
        case 'i':
            g_options.initseg = optarg;
            break;
        case 'h':
        default:
            return -1;
        }
    }

    /* File name */
    if (optind < argc)
        g_options.filename = argv[optind];
    else
        return -1;

    return 0;
}


static void
mp4tree_usage_print(const char * binary)
{
    printf("Description:\n");
    printf(" This program parses and prints the content of an mp4 file.\n");
    printf("Usage: %s [OPTION]... [FILE]\n", binary);
    printf("  Available OPTIONs:\n");
    printf("  -t, --truncate=N          Truncate boxes larger N bytes (default N=256)\n");
    printf("  -s, --selftest            Run self test\n");
    printf("  -i, --initseg=<path>      Also parse init segment at <path>\n");
    printf("\n");
}


int
main(int argc, char **argv)
{
    int status;

    if (mp4tree_parse_options(argc, argv) < 0)
    {
        mp4tree_usage_print(argv[0]);
        exit(EXIT_FAILURE);
    }

    if (g_options.selftest)
    {
        return mp4tree_selftest();
    }

    if (g_options.initseg)
    {
        status = process_file(g_options.initseg);
        if (status != EXIT_SUCCESS)
        {
            fprintf(stderr, "Error parsing init segment %s\n", g_options.initseg);
            return status;
        }
    }

    status = process_file(g_options.filename);
    return status;
}
