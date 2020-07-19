struct bridge;
struct port;
struct bridge {
    struct bridge *next;
    int ifindex;
    char ifname[IFNAMSIZ];
    struct port *firstport;
    struct port *ports[256];
};

struct port {
    struct port *next;
    int index;
    int ifindex;
    char ifname[IFNAMSIZ];
    struct bridge *parent;
};
uint32_t
acfg_get_br_name(uint8_t *ifname, char *brname);      

#if ACFG_DEBUG
#undef ACFG_DEBUG_ERROR
#define ACFG_DEBUG_ERROR 1
#define acfg_print(fmt, ...) _acfg_print(fmt, ##__VA_ARGS__)
#else
#define acfg_print(fmt, ...)
#endif

