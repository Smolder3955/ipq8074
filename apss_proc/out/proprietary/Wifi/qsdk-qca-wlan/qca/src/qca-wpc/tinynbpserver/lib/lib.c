#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include <bdaddr.h>


int bachk(const char *str)
{
        if (!str)
                return -1;

        if (strlen(str) != 17)
                return -1;

        while (*str) {
                if (!isxdigit(*str++))
                        return -1;

                if (!isxdigit(*str++))
                        return -1;

                if (*str == 0)
                        break;

                if (*str++ != ':')
                        return -1;
        }

        return 0;
}
void baswap(bdaddr_t *dst, const bdaddr_t *src)
{
        register unsigned char *d = (unsigned char *) dst;
        register const unsigned char *s = (const unsigned char *) src;
        register int i;

        for (i = 0; i < 6; i++)
                d[i] = s[5-i];
}


int str2ba(const char *str, bdaddr_t *ba)
{
        bdaddr_t b;
        int i;

        if (bachk(str) < 0) {
                memset(ba, 0, sizeof(*ba));
                return -1;
        }

        for (i = 0; i < 6; i++, str += 3)
                b.b[i] = strtol(str, NULL, 16);

//        baswap(ba, &b);
	memcpy(ba,&b,6);

        return 0;
}
int ba2str(const bdaddr_t *ba, char *str)
{
        return sprintf(str, "%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X",
                (unsigned char) ba->b[0], (unsigned char)ba->b[1], (unsigned char)ba->b[2], (unsigned char )ba->b[3], (unsigned char )ba->b[4], (unsigned char) ba->b[5]);
}

int bacmp(bdaddr_t *addr1, bdaddr_t *addr2)
{
    return memcmp(addr1, addr2, 6);
}



