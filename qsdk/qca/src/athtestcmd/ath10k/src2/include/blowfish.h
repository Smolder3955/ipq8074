/*
 * Author     :  Paul Kocher
 * E-mail     :  pck@netcom.com
 * Date       :  1997
 * Description:  C implementation of the Blowfish algorithm.
 */
#ifndef _H_BLOWFISH
#define	_H_BLOWFISH

#define MAXKEYBYTES 56          /* 448 bits */



typedef struct {

  unsigned long P[16 + 2];

  unsigned long S[4][256];

} BLOWFISH_CTX;


#if __cplusplus
extern "C" {
#endif

void Blowfish_Init(BLOWFISH_CTX *ctx, unsigned char *key, int keyLen);

void Blowfish_Encrypt(BLOWFISH_CTX *ctx, unsigned long *xl, unsigned long *xr);

void Blowfish_Decrypt(BLOWFISH_CTX *ctx, unsigned long *xl, unsigned long *xr);

int Blowfish_Test(BLOWFISH_CTX *ctx);       /* 0=ok, -1=bad */

#if __cplusplus
}
#endif

#endif	// _H_BLOWFISH


