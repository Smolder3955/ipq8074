
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "uci_core.h"
#include "package.h"


struct plc_s{
    int changed;
};


static int plc_init(struct package_s *pkg)
{
    struct plc_s *plc = (struct plc_s *)pkg->priv;
    plc->changed = 0;
    return 0;
}

static int plc_set_nmk(struct uci_context *ctx, struct package_s *pkg, char *value)
{
    char nmk[UCI_MAX_STR_LEN];
    char selected[UCI_MAX_STR_LEN];
    struct plc_s *plc = (struct plc_s *)pkg->priv;
    int ret = 0;

    ret = uciGet(ctx, pkg->name, "config.Nmk", nmk);
    ret |= uciGet(ctx, pkg->name, "config.NmkSelected", selected);

    if ( ret
        || strcmp(value, nmk)
        || strcmp("true", selected))
    {
        ret = uciSet(ctx, pkg->name, "config.Nmk", value);
        ret |= uciSet(ctx, pkg->name, "config.NmkSelected", "true");
        plc->changed++;
    }

    return ret;
}

static int plc_set(void *cookie, struct package_s *pkg, char *name, char *value)
{
    char option[UCI_MAX_STR_LEN];
    char ucival[UCI_MAX_STR_LEN];
    struct plc_s *plc = (struct plc_s *)pkg->priv;
    struct uci_context *ctx = (struct uci_context *)cookie;
    int ret = 0;


    name += strlen(pkg->flt1);

    if (strcasecmp(name, "NMK") == 0)
        return plc_set_nmk(ctx, pkg, value);

    snprintf(option, sizeof(option), "config.%s", name);

    ret = uciGet(ctx, pkg->name, option, ucival);

    if (ret
        || strcmp(value, ucival))
    {
        ret = uciSet(ctx, pkg->name, option, value);
        plc->changed++;
    }

    return ret;
}

static int plc_apply(void *cookie, struct package_s *pkg)
{
    struct plc_s *plc = (struct plc_s *)pkg->priv;
    struct uci_context *ctx = (struct uci_context *)cookie;
    int ret = 0;

    if (!plc->changed)
    {
        return 0;
    }

    ret = uciCommit(ctx, pkg->name);
    system("/etc/init.d/plc reload");
    plc->changed = 0;

    return ret;
}


static struct package_s plcPkg =
{
    "plc",              /*name*/
    plc_init,           /*init*/
    plc_set,            /*set*/
    NULL,               /*get*/
    plc_apply,          /*apply*/
    NULL         ,      /*destroy*/
    "PLC.",             /*flt1*/
    NULL,               /*flt2*/
    NULL,               /*priv*/
    NULL,               /*next*/
    NULL                /*ifaction*/
};


int plc_register()
{
    struct plc_s *plc = malloc(sizeof(struct plc_s));

    if (! plc)
        return -1;

    memset(plc, 0, sizeof(struct plc_s));

    plcPkg.priv = (void *)plc;

    pkgRegister(&plcPkg);

    return 0;
}



