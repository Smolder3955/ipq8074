#ifndef _PACKAGE_H_
#define _PACKAGE_H_

struct package_s{
    char *name;
    int (*init)(struct package_s *pkg);
    int (*set)(void *cookie, struct package_s *pkg, char *name, char *value);
    int (*get)(void *cookie, struct package_s *pkg, char *name, char *value);
    int (*apply)(void *cookie, struct package_s *pkg);
    int (*destroy)(struct package_s *pkg);
    char *flt1;
    char *flt2;
    void *priv;
    struct package_s *next;
    int (*ifaction)(void *cookie, struct package_s *pkg, const char *name, const char *value);
};

struct pkgmgr_s{
    struct package_s *head;
    int isInit;
    void *cookie;
    int semid;
};

int pkgInit(struct pkgmgr_s *pkgm);
int pkgSet(struct pkgmgr_s *pkgm, char *name, char *value);
int pkgApply(struct pkgmgr_s *pkgm);
int pkgDestroy(struct pkgmgr_s *pkgm);
int pkgRegister(struct package_s *pkg);
struct pkgmgr_s *pkgGetPkgmgr();
int pkgIfAction(struct pkgmgr_s *pkgm, const char *name, const char *value);
int wlanif_init(void);
void wlanif_deinit(void);
#endif
