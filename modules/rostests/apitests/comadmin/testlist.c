#define STANDALONE
#include <wine/test.h>

extern void func_comadmin(void);

const struct test winetest_testlist[] =
{
    { "comadmin", func_comadmin },

    { 0, 0 }
};