
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "uci_core.h"
#include "package.h"


enum {
    WSPLCRUNMODE_REGISTRAR=1,
    WSPLCRUNMODE_ENROLLEE,
};

enum {
    WSPLCCONFIGURESTA_YES=1,
    WSPLCCONFIGURESTA_NO,
};

struct wsplc_s{
    int changed;
};



struct ws_translate_option{
    char *optname;
    int (*opt2uci)(char *optval, char *ucival);
};

static int opt_runmode(char *optval, char *ucival)
{
    int runmode;

    if (!optval || !ucival)
        return -1;

    if (isdigit(*optval))
    {
        runmode = atoi(optval);
        if (runmode == WSPLCRUNMODE_REGISTRAR)
            snprintf(ucival, UCI_MAX_STR_LEN, "REGISTRAR");
        else if (runmode == WSPLCRUNMODE_ENROLLEE)
            snprintf(ucival, UCI_MAX_STR_LEN, "ENROLLEE");
        else
            snprintf(ucival, UCI_MAX_STR_LEN, "NONE");
    }
    else
        snprintf(ucival, UCI_MAX_STR_LEN, optval);

    return 0;
}

static int opt_configsta(char *optval, char *ucival)
{
    int runmode;

    if (!optval || !ucival)
        return -1;

    if (isdigit(*optval))
    {
        runmode = atoi(optval);
        if (runmode == WSPLCCONFIGURESTA_YES)
            snprintf(ucival, UCI_MAX_STR_LEN, "1");
        else if (runmode == WSPLCCONFIGURESTA_NO)
            snprintf(ucival, UCI_MAX_STR_LEN, "0");
    }
    else
        snprintf(ucival, UCI_MAX_STR_LEN, optval);

    return 0;
}

static struct ws_translate_option wsTbl[] =
{
    { "RunMode",  opt_runmode},
    { "ConfigSta",  opt_configsta},
    { NULL,       NULL}
};


static int wsplc_init(struct package_s *pkg)
{
    struct wsplc_s *ws = (struct wsplc_s *)pkg->priv;
    ws->changed = 0;
    return 0;
}

static int wsplc_set(void *cookie, struct package_s *pkg, char *name, char *value)
{
    char option[UCI_MAX_STR_LEN];
    char ucival[UCI_MAX_STR_LEN];
    char newval[UCI_MAX_STR_LEN];
    struct wsplc_s *ws = (struct wsplc_s *)pkg->priv;
    struct uci_context *ctx = (struct uci_context *)cookie;
    struct ws_translate_option *iopt;
    int ret = 0;

    name += strlen(pkg->flt1);

    iopt = wsTbl;
    while(iopt->optname != NULL)
    {
        if (strcmp(name, iopt->optname) == 0)
            break;
        iopt ++;
    }


    if (iopt && iopt->opt2uci)
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
        ws->changed++;
    }

    return ret;
}

static int wsplc_apply(void *cookie, struct package_s *pkg)
{
    struct wsplc_s *ws = (struct wsplc_s *)pkg->priv;
    struct uci_context *ctx = (struct uci_context *)cookie;
    int ret = 0;

    if (!ws->changed)
        return 0;

    ret = uciCommit(ctx, pkg->name);
    system("/etc/init.d/wsplcd restart");

    return ret;
}


static struct package_s wsplcPkg =
{
    "wsplcd",          /*name*/
    wsplc_init,        /*init*/
    wsplc_set,         /*set*/
    NULL,              /*get*/
    wsplc_apply,       /*apply*/
    NULL         ,     /*destroy*/
    "WSPLC.",          /*flt1*/
    NULL,              /*flt2*/
    NULL,              /*priv*/
    NULL,              /*next*/
    NULL               /*ifaction*/
};


int wsplc_register()
{
    struct wsplc_s *ws = malloc(sizeof(struct wsplc_s));

    if (! ws)
        return -1;

    memset(ws, 0, sizeof(struct wsplc_s));

    wsplcPkg.priv = (void *)ws;

    pkgRegister(&wsplcPkg);

    return 0;
}



