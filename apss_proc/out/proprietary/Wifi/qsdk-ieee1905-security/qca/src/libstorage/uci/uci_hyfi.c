
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "uci_core.h"
#include "package.h"

enum {
    HYBRIDMODE_HYROUTER=1,
    HYBRIDMODE_HYCLIENT,
    HYBRIDMODE_EXTENDER,
};

struct hyfi_s{
    int changed;
};



struct hy_translate_option{
    char *optname;
    int (*opt2uci)(char *optval, char *ucival);
};

static int opt_hybridmode(char *optval, char *ucival)
{
    int runmode;

    if (!optval || !ucival)
        return -1;

    if (isdigit(*optval))
    {
        runmode = atoi(optval);
        if (runmode == HYBRIDMODE_HYROUTER)
            snprintf(ucival, UCI_MAX_STR_LEN, "HYROUTER");
        else if (runmode == HYBRIDMODE_HYCLIENT)
            snprintf(ucival, UCI_MAX_STR_LEN, "HYCLIENT");
        else if (runmode == HYBRIDMODE_EXTENDER)
            snprintf(ucival, UCI_MAX_STR_LEN, "HYEXTENDER");
        else
            return -1;
    }
    else
    {
        snprintf(ucival, UCI_MAX_STR_LEN, optval);
    }
    return 0;
}


static struct hy_translate_option hyTbl[] =
{
    { "Mode",  opt_hybridmode},
    { NULL}
};


static int hyfi_init(struct package_s *pkg)
{
    struct hyfi_s *hy = (struct hyfi_s *)pkg->priv;
    hy->changed = 0;
    return 0;
}

static int hyfi_set(void *cookie, struct package_s *pkg, char *name, char *value)
{
    char option[UCI_MAX_STR_LEN];
    char ucival[UCI_MAX_STR_LEN];
    char newval[UCI_MAX_STR_LEN];
    struct hyfi_s *hy = (struct hyfi_s *)pkg->priv;
    struct uci_context *ctx = (struct uci_context *)cookie;
    struct hy_translate_option *iopt;
    int ret = 0;


    name += strlen(pkg->flt1);

    iopt = hyTbl;
    while(iopt->optname != NULL)
    {
        if (strcmp(name, iopt->optname) == 0)
            break;
        iopt ++;
    }


    if (iopt->optname && iopt->opt2uci)
    {
        if (iopt->opt2uci(value, newval) != 0)
            return -1;
        value = newval;
    }

    snprintf(option, sizeof(option), "config.%s", name);

    ret = uciGet(ctx, pkg->name, option, ucival);

    if (ret
        || strcmp(value, ucival))
    {
        ret = uciSet(ctx, pkg->name, option, value);
        hy->changed++;
    }

    return ret;
}

static int hyfi_apply(void *cookie, struct package_s *pkg)
{
    struct hyfi_s *hy = (struct hyfi_s *)pkg->priv;
    struct uci_context *ctx = (struct uci_context *)cookie;
    int ret = 0;

    if (!hy->changed)
        return 0;

    ret = uciCommit(ctx, pkg->name);
    system("/etc/init.d/hyd restart");

    return ret;
}


static struct package_s hyfiPkg =
{
    "hyd",              /*name*/
    hyfi_init,          /*init*/
    hyfi_set,           /*set*/
    NULL,               /*get*/
    hyfi_apply,         /*apply*/
    NULL         ,      /*destroy*/
    "HYFI.",            /*flt1*/
    NULL,               /*flt2*/
    NULL,               /*priv*/
    NULL,               /*next*/
    NULL                /*ifaction*/
};


int hyfi_register()
{
    struct hyfi_s *hy = malloc(sizeof(struct hyfi_s));

    if (! hy)
        return -1;

    memset(hy, 0, sizeof(struct hyfi_s));

    hyfiPkg.priv = (void *)hy;

    pkgRegister(&hyfiPkg);

    return 0;
}



