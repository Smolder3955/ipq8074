#ifndef _BD_ADDR_H
#define _BD_ADDR_H

typedef struct {
        char b[6];
} __attribute__((packed)) bdaddr_t;


int str2ba(const char *str, bdaddr_t *ba);
void baswap(bdaddr_t *dst, const bdaddr_t *src);
int bachk(const char *str);


#endif
