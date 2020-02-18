#include <string.h>

#include "common.h"

const char *
indent(int depth, int header)
{
    static char buf[64];
    int i = 0;

    memset(buf, 0 , sizeof buf);

    for (i = 0; i < depth; i++)
    {
        strcat(buf, "|  ");
    }

    if (header)
        strcat(buf, "+");
    else
        strcat(buf, "");

    return buf;
}


