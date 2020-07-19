/*
 * Copyright (c) 2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#include <fw_pack.h>
#include <fw_certs.h>
/*
 * This tool accepts multiple optional arguments and few mandatory. 
 * Following are optional 
 *  File Type (-a)      ? File is binary or text. Default binary, as there are
 *                        no text file in use at the time of this tool
 *                        development.
 *  host resident (-r)  ? Usually all files this tool deals with are target
 *                        downloaded except the code swap file.
 *  compressed (-c)     ? For future use, it is transperent to this tool at this
 *                        point. User can compress outside, and continue to use
 *                        the file as uncompressed. In future, if we like to
 *                        enable compress libraries, we can enabgle them and
 *                        use this option. Right now tool ignores this option.
 * firmware release (-t)? This identifies the release quality, is this 
 *                        production or test version, majorly use to chose Keys.
 * -p product dev id (xx)  ? PCIe device Id. 
 * -o  ofile  (name)     ? Output filename, if not mentioned, it would be named 
 *                        with extension .xsig, eg, athwlan.bin would become
 *                        athwlan.bin.xisg
 * input file  (name)   ? File to wrap with signatures and headers 
 * -s signature file (name)? Hex signature in a file 
 *  
 * Image type (fw_img_file_magic_t) is mostly derived from filenames themselves 
 * at this point of time. 
*/
int fw_debug = 0;
static char *fw_pack_errs[] = {
    "inernal error",
    "invalid version string",
    "invalid device id",
    "invalid input",
    "invalid input file",
    "invalid file format"
    "incorrect file access",
    "can't create output file",
    "unsupported chipset",
    "error writing to file",
    "invalid file magic",
    "error while reading input file",
    "sign version mismatch",
    "sign algorithm mismatch"
} ;
char **cmd_args = NULL;
void usage()
{
    printf("%s [-a] [-r] [-c] {-v version_string } [-t] [-o out_file_name]"
            " {-p product_id} {-s sig_file} {-i input_filename}\n",
            cmd_args[0]);
    printf("\t\t-a  . optional - if file is text file\n"
           "\t\t-r  . optional - if file is host resident\n"
           "\t\t-c  . optional - if the file is compressed[NOT IMPLEMENTED]\n"
           "\t\t-t  . optional - if the file is test version\n"
           "\t\t-p  . required - pci product id\n"
           "\t\t-o  . required - output file [ as per other options ]\n"
           "\t\t-s  . required - signature file is provided on cli\n"
           "\t\t-v  . required - version string [ eg. CNSS.BL.3.0-00141-S-1]\n"
           "\t\t-g  . generate header only and attach for signing process\n"
           "\t\t-G  . Sign only - no header generation\n"
           "\t\t-dD . debug options, 1 - less details, 2 - hex dumps\n"
           "\t\t-U  . Unpack test, [ unpack only, no signature validation]\n"
           "\t\thH  . Get this help message\n"
          );
    exit(-1);
}

void
fw_hex_dump(unsigned char *p, int len)
{
    int i=0;

    for (i=0; i<len; i++) {
        if (!(i%16)) {
            printf("\n");
        }
        printf("%2x ", p[i]);
    }
}
/* unpacking and parsing routine for driver to work. 
 * Input is the loaded firmware file. It is assumed
 * that file is already loaded in memory. In general
 * request_firmware is usd get the file loaded into
 * memory. 
 */
inline int 
fw_check_img_magic(unsigned int img_magic)
{
    int i=0;

    for (i = 0; 
         ((i < NELEMENTS(fw_auth_supp_devs)) && (fw_auth_supp_devs[i].img_magic != img_magic) &&
            (fw_auth_supp_devs[i].dev_id != 0)) ; 
         i++)
       ;
    if (fw_auth_supp_devs[i].dev_id == 0) {
        return -1;
    }
    return i;
}

static inline int
fw_pack_check_file_magic(unsigned int file_type)
{
    switch (file_type) {
    case FW_IMG_FILE_MAGIC_TARGET_BOARD_DATA:
    case FW_IMG_FILE_MAGIC_TARGET_WLAN:
    case FW_IMG_FILE_MAGIC_TARGET_OTP:
    case FW_IMG_FILE_MAGIC_TARGET_CODE_SWAP:
        return 0;
    case FW_IMG_FILE_MAGIC_INVALID:
        return -FW_PACK_ERR_INVALID_FILE_MAGIC;
    default:
        return -FW_PACK_ERR_INVALID_FILE_MAGIC;
    }
    return 0;
}
static inline int 
fw_check_chip_id(unsigned int chip_id)
{
    int i=0;

    for (i = 0; 
         ((i < NELEMENTS(fw_auth_supp_devs)) && (fw_auth_supp_devs[i].dev_id != chip_id) &&
            (fw_auth_supp_devs[i].dev_id != 0)) ; 
         i++)
       ;
    if (fw_auth_supp_devs[i].dev_id == 0) return -1;
    return i;
}
/*
 *
*/
int
fw_pack_init_globals(struct fw_pack_cmd_args *pargs)
{
    if (!pargs) {
        return -FW_PACK_ERR_INTERNAL;
    }
    pargs->in_file_type     = FW_IMG_FLAGS_FILE_FORMAT_BIN;
    pargs->t_resident       = FW_IMG_FLAGS_TARGRES;
    pargs->compressed       = FW_IMG_FLAGS_UNCOMPRESSED;
    memset(pargs->fw_ver_string, 0, sizeof(pargs->fw_ver_string));
    pargs->fw_release       = FW_IMG_VER_REL_PROD;
    pargs->ofile            = NULL;                        /* initialize later*/
    pargs->prd_dev_id       = 0;                                /* UNSUPPORTED*/
    pargs->sig_file         = NULL;
    pargs->in_file          = NULL;

    return 0;
}
/*
 * Parse the command line arguements and fill the global command line args 
 * structure for further processing. 
 * passed arguments.  
 *
 * 
*/
int
fw_pack_parse_args(int argc, char *argv[], struct fw_pack_cmd_args *pargs)
{
    char c=0;
    int g_set = 0, s_set = 0, G_set = 0;
    /*
     * fw_pack [-a] [-r] [-c] {-v version_string } [-t] [-o out_file_name] {-p product_id} {-s signature file} [-i] input_filename
     */
    while ((c = getopt(argc, argv, "dDUtgGHharcv:i:o:p:s:")) != -1) {
        switch (c) {
        case 'H':
        case 'h':
            usage();
            return 0;
        case 'a':
            pargs->in_file_type = FW_IMG_FLAGS_FILE_FORMAT_TEXT;
            break;
        case 'r':
            pargs->t_resident = FW_IMG_FLAGS_HOSTRES;
            break;
        case 'c':
            pargs->compressed = FW_IMG_FLAGS_COMPRESSED;
            break;
        case 'v':
            if (optarg &&  optarg[0]) {
                memcpy(pargs->fw_ver_string, optarg, ((strlen(optarg) > 256) ?
                         256: strlen(optarg)));
            } else {
                return -FW_PACK_ERR_INV_VER_STRING;
            }
            break;
        case 't':
            pargs->fw_release       = FW_IMG_VER_REL_TEST;
            break;
        case 'o':
            pargs->ofile = optarg;
            break;
        case 'p':
            if (0 != get_number(optarg, &pargs->prd_dev_id)) {
                return -FW_PACK_ERR_INV_DEV_ID;
            }
            break;
        case 's':
            pargs->sig_file = optarg;
            s_set = 1;
            break;
        case 'g':
            g_set = 1;
            break;
        case 'G':
            G_set = 1;
            break;
        case 'i':
            pargs->in_file = optarg;
            break;
        case 'U':
            pargs->test_file = 1;
            break;
        case 'd':
        case 'D':
            fw_debug=1;
            break;
        default:
            usage();
            break;
        }
    }
    if (pargs->test_file) return 0;
    if (!g_set && !G_set) {
        fprintf(stderr, "Either -g or -G/s need to be set\n");
        usage();
        return -FW_PACK_ERR_INPUT;
    }
    if (g_set && (s_set || G_set)) {
        fprintf(stderr, "-g to generate partial image with header\n-s|G to add sign\n"
                "they can't be used to gether\n");
        usage();
        return -FW_PACK_ERR_INPUT;
    }
    if (G_set && !s_set) {
        fprintf(stderr, "-G to add signature along with -s signature_file required\n");
        usage();
        return -FW_PACK_ERR_INPUT;
    }
    if (g_set) pargs->header_only = 1;
    if (G_set && s_set) pargs->sign_only = 1;
    return 0;
}

/* fw_pack_cmd_args_check
 * Do sanity check on all the input and default parameters. This would include 
 * all the files and its access levels. 
 */
int
fw_pack_cmd_args_check(struct fw_pack_cmd_args *pargs)
{
    int i=0, found=0;
    struct stat fs={0};

    if (!pargs) return -FW_PACK_ERR_INTERNAL;
    if (pargs->compressed) {
        fprintf(stderr, "Compression is *NOT* supported\n, file would be in\n");
        fprintf(stderr, "uncomressed mode\nOR\nno action taken on file format\n");
        fprintf(stderr, "Resetting to uncompressed....\n");
        pargs->compressed = FW_IMG_FLAGS_UNCOMPRESSED;
    }
    /* check the device ID*/
    if (pargs->prd_dev_id) 
        for (i = 0; i < NELEMENTS(fw_auth_supp_devs); i++) {
            if (fw_auth_supp_devs[i].dev_id == pargs->prd_dev_id){
                found = 1;
                break;
            }
        }

    if (!found) {
        return -FW_PACK_ERR_INV_DEV_ID;
    }

    /* if -g, no need of signature file
       if -G and -s, intermediate file with header as input file 
    */
    if ((pargs->header_only && !pargs->in_file) ||
        (pargs->sign_only && (!pargs->in_file || !pargs->sig_file))) {
        return -FW_PACK_ERR_INPUT_FILE;
    }

    /* check the file access permissions on all input files*/
    if (pargs->in_file && stat(pargs->in_file, &fs) < 0 ) {
            return -FW_PACK_ERR_INFILE_READ;
    } else {
        /* copy the file size to struct */
        pargs->in_file_size = fs.st_size;
    }
    /* check write permissions on output file if specified and file is existing
     * if file is not present, no need to check here, the file create time
     * module should receive write error 
     */ 
    if (pargs->ofile) {
        if (stat(pargs->ofile, &fs) >= 0) {
            if (!(fs.st_mode & S_IWUSR)) {
                return -FW_PACK_ERR_FILE_WRITE;
            }
        }
    }
    return 1;
}

/* following functions are helper functions, to return the version strings
    maj_ver = fw_get_maj_ver(pargs->fw_ver_string);
    ent_rel = fw_get_ent(pargs->fw_ver_string);
    ent_rel_ver = fw_get_ent_rel(pargs->fw_ver_string);
    min_ver = fw_get_min_ver(pargs->fw_ver_string);
    build_id = fw_get_build_id(pargs->fw_ver_string);
*/
/* This is local structure, to reduce number of parameters*/
struct _fw_ver {
    uint32 majv;
    uint32 minv;
    uint32 bldid;
    uint32 entrel;
    uint32 entrelv;
    uint32 minsubv;
};
/* from the pargs, to get file type
 * Ideally this can be supplied by the command line, here we parse the
 * input file and try to find file type. Most of the files will have the
 * following way.
 * OTP files - otp.bin
 * board Data file - boardData or boarddata or board
 * host resident code swap files - code
 * target wlan files, athwlan
 * if any changes in this need to go through this function
 */
int
fw_get_img_type(struct fw_pack_cmd_args *pargs)
{
    char *f = basename(pargs->in_file);
    if (f) {
        if (strstr(f, "board")) {
            pargs->img_type = FW_IMG_FILE_MAGIC_TARGET_BOARD_DATA;
        } else if (strstr(f, "codeswap")) {
            pargs->img_type = FW_IMG_FILE_MAGIC_TARGET_CODE_SWAP;
        } else if (strstr(f, "athwlan")) {
            pargs->img_type = FW_IMG_FILE_MAGIC_TARGET_WLAN;
        } else if (strstr(f, "otp")) {
            pargs->img_type = FW_IMG_FILE_MAGIC_TARGET_OTP;
        } else return -FW_PACK_ERR_INVALID_FILE_MAGIC;
    }
    return 0;
}
/*
 * parse the version string and find the version numbers
*/
int
fw_get_ver_str2vals(const char *vstr, struct _fw_ver *ver)
{
    /* CNSS.BL.1.1.1-00028-S-1 */
    const char *s = vstr;
    char vv[8]={0};
    int i=0;

    if (!s) {
        return -FW_PACK_ERR_INV_VER_STRING;
    }

    /* ignore all white spaces*/
    while (s && (*s== ' ' || *s == '\t'))
        s++;

    /* ingore all letters and . until we see first number */
    while (s && (toupper(*s) >= 'A' && toupper(*s) <= 'Z' || (*s == '.'))){
        s++;
    }
    /* major.minor.subversion*/
   /* read until next ., as a major version*/ 
    if (!isdigit(*s)) return -1;
    while (s && isdigit(*s)) {
        vv[i++] = *s;
        s++;
    }
    ver->majv = atoi(vv); 
    memset(vv, 0, sizeof(vv));
    i = 0;
    /*s=.minor*/
    /* skip a . and check for number, if not error*/
    s++; 
    if (s && !isdigit(*s)) return -1;
    while (s && isdigit(*s)) {
        vv[i++]=*s; 
        s++;
    }
    ver->minv = atoi(vv);
    memset(vv, 0, sizeof(vv));
    i = 0;

    /* 
     * .minor subv OR a '-', if '-' there is no minor subversion else 
     * this is  build number
     */
    if (s && *s == '.') {
        s++;
        if (s && !isdigit(*s)) return -1;
        while (s && isdigit(*s)) {
            vv[i++]=*s; 
            s++;
        }
        ver->minsubv = atoi(vv);
        memset(vv, 0, sizeof(vv));
        i = 0;
    }
    /* actual build number if there is no sub version*/
    if (s && *s == '-') {
        s++;
        if (s && !isdigit(*s)) return -1;
        while (s && isdigit(*s)) {
            vv[i++]=*s; 
            s++;
        }
        ver->bldid = atoi(vv);
        memset(vv, 0, sizeof(vv));
        i = 0;
    }
    /* parse the other parts of version TODO */
    /* ignore, - */
    s++;
    if (s) {
        switch (toupper(*s)) {
        case 'S':
            ver->entrel = FW_VER_IMG_TYPE_S_RETAIL;
            break;
        case 'E':
            ver->entrel = FW_VER_IMG_TYPE_E_ENTERPRISE;
            break;
        default:
            ver->entrel = FW_VER_IMG_TYPE_X_UNDEF;
            return -1; /* no proper version string*/
        }
    }

    while(s && !isdigit(*s))
        s++;

    if (s) {
         while (s && isdigit(*s)) {
             vv[i++] = *s;
             s++;
         }
         ver->entrelv = atoi(vv);
         memset(vv, 0, sizeof(vv));
         i=0;
    }
    return 0;
}
/*
 * fill firmware header with appropriate values from command line args.
*/
int
fw_pack_fill_header(struct firmware_head *h, struct fw_pack_cmd_args *pargs)
{
    int i = 0, err=0;
    char found=0;
    int maj_ver, min_ver;
    int ent_rel, ent_rel_ver, build_id;
    struct _fw_ver ver;

    if (!h || !pargs) return -FW_PACK_ERR_INTERNAL;

    /* go through list of known chipsets and findout the image magic to use
     */
    if ((i = fw_check_chip_id(pargs->prd_dev_id)) >= 0) {
        if (fw_auth_supp_devs[i].dev_id == pargs->prd_dev_id) {
            h->fw_img_magic_number = fw_auth_supp_devs[i].img_magic;
            h->fw_chip_identification = fw_auth_supp_devs[i].dev_id;
        found = 1;
        }
    }

    if (!found) {
        return -FW_PACK_ERR_UNSUPP_CHIPSET;
    }


    /* fill the image singing version*/
    h->fw_img_sign_ver = THIS_FW_IMAGE_VERSION;

    h->fw_img_spare1 = 0xa5;
    h->fw_img_spare2 = 0xa5;
    h->fw_ver_rel_type = pargs->fw_release;

    /* parse and encode the version string into major and minor*/
    if (0 != fw_get_ver_str2vals(pargs->fw_ver_string, &ver))
        return -1;
    FW_VER_SET_MAJOR(h, ver.majv);
    FW_VER_SET_IMG_TYPE(h, ver.entrel);
    FW_VER_SET_IMG_TYPE_VER(h, ver.entrelv);
    FW_VER_IMG_SET_MINOR_VER(h, ver.minv);
    FW_VER_IMG_SET_MINOR_SUBVER(h, ver.minsubv);
    FW_VER_IMG_SET_MINOR_RELNBR(h, ver.bldid);

    /* fill all the spares*/
    h->fw_img_spare1 = h->fw_img_spare2 = 0xa5;
    h->fw_img_spare2 = h->fw_ver_spare3 = 0xa5;

    /* last three pointers are not meant to be in file header*/
    h->fw_hdr_length = sizeof(struct firmware_head) ;

    /* get the image type from the file name */
    if(0== (err = fw_get_img_type(pargs))) {
        /* check the file type is good */
        if (fw_pack_check_file_magic(pargs->img_type) == -1) {
            fprintf(stderr, "File type not supported");
            return err;
        }
        h->fw_img_file_magic = pargs->img_type;
    }

    switch (pargs->img_type) {
    case FW_IMG_FILE_MAGIC_TARGET_BOARD_DATA:
       FW_IMG_FLAGS_SET_TARGRES(h->fw_hdr_flags);
       break;
    case FW_IMG_FILE_MAGIC_TARGET_CODE_SWAP:
        /* do nothing, it is clear already*/
        ;
        break;
    case FW_IMG_FILE_MAGIC_TARGET_OTP:
        FW_IMG_FLAGS_SET_TARGRES(h->fw_hdr_flags);
        break;
    case FW_IMG_FILE_MAGIC_TARGET_WLAN:
        FW_IMG_FLAGS_SET_TARGRES(h->fw_hdr_flags);
        break;
    default:
        /* ignore */
        break;
    }

    h->fw_hdr_flags |= FW_IMG_FLAGS_FILE_FORMAT_BIN;
    h->fw_sig_algo = RSA_PSS1_SHA256;
    h->fw_sig_len = 512; /*2048 bits are 256 bytes*/
    h->fw_img_size = pargs->in_file_size;
    h->fw_img_length = h->fw_img_size + sizeof(*h) + h->fw_sig_len;
    h->fw_oem_id = 0;
    return 0;
}
/*
 * fw_header_encap - Encapsulate the firmware file with the header and sign. 
 * 
 * This includes creating a binary file, copy the bytes of the header first, 
 * then the actual binary and signature at end. Make sure any word level padding 
 * is required so that loading into memory gives word boundary always. 
 * by logic of this module, this founction would be called only after
 * completing sanity checks on the arguments parsed
 */
int
fw_header_encap(struct fw_pack_cmd_args *pargs)
{
    unsigned char *ualign_h;
    struct firmware_head *h = NULL;
    int err=0;
    int rsfd = -1;                                 /* readonly, signature file*/
    int wxfd = -1;                                    /* write only xsig file */
    int rffd = -1;                                 /* readonly, firmware file */
    int align_bytes = 0;

#define ALIGN 4
    if (pargs->header_only) {
        ualign_h = malloc(sizeof(struct firmware_head) + ALIGN);
        if (!ualign_h) return -FW_PACK_ERR_INTERNAL;

        if (!IS_ALIGNED(ualign_h, ALIGN)) {
            h = (struct firmware_head*)ALIGNTO(ualign_h, ALIGN);
        } else {
            h = (struct firmware_head*)ualign_h;
        }
        memset(h, 0, sizeof(struct firmware_head));

        if ( 0 && pargs->in_file_size % ALIGN) {
            align_bytes = ALIGN - (pargs->in_file_size % ALIGN);
            pargs->in_file_size += align_bytes;
            printf("%s requires alignment %d\n", __func__, align_bytes);
        }
        fw_pack_fill_header(h, pargs);

        /* get file for writing*/
        if ((rffd = open(pargs->in_file, O_RDONLY)) > 0) {
            if ((wxfd = open(pargs->ofile, O_CREAT | O_WRONLY, 00766)) > 0) {
                int len = h->fw_hdr_length;
                /* now compelte the three writes to the disk, simply
                 * by calling write routines for the size of files
                 */
                /* multiple systems to continue to store the bytes in different
                 * byte order. While writing the buffer make sure that all 
                 * bytes are converted to network order. Also make sure the 
                 * header is always aligned to word boundry, and keep the 
                 * memory aligned. 
                 * While reading load them to memory and convert them into 
                 * host byte order again. 
                 * We should use htonl and ntohl functions to do this
                 */
                fw_hex_dump((unsigned char*)h, len);
                htonlm((void*)h, len);
                /* CAUTION: do not access header post this line, this is 
                 * is converted to network byte order
                 */
                /* check if file is aligned to size, if not fill zeros */
                if( (err = fcopy_bfr2fd(wxfd, h, len)) >= 0) {
                    if ((err = fcopy_fd2fd(wxfd, rffd,
                                    pargs->in_file_size)) >= 0) {
                        while (align_bytes) {
                            unsigned char c=0;
                            fcopy_bfr2fd(wxfd, &c, 1);
                            align_bytes -= 1;
                        }
                    }
                }
            }
        }
        if (ualign_h) {
            free(ualign_h);
            ualign_h = NULL;
            h = NULL;
        }
    }

    /* do cleanup*/
    if (rffd) close(rffd);
    if (rsfd) close(rsfd);
    if (wxfd) close(wxfd); 
    return err;
}

int
fw_sign_encap(struct fw_pack_cmd_args *pargs)
{
    unsigned char *ualign_h;
    struct firmware_head *h = NULL;
    struct stat fs;
    int err=0;
    int rsfd = -1;                                 /* readonly, signature file*/
    int wxfd = -1;                                    /* write only xsig file */
    int rffd = -1;                                 /* readonly, firmware file */

    if (pargs->sign_only) {
        memset(&fs, 0, sizeof(fs));
        if (pargs->in_file && stat(pargs->in_file, &fs) < 0 ) {
                return -FW_PACK_ERR_INFILE_READ;
        } else {
            /* copy the file size to pargs*/
            pargs->in_file_size = fs.st_size;
        }
        rffd = open(pargs->in_file, O_RDONLY);

        memset(&fs, 0, sizeof(fs));
        if (pargs->sig_file && stat(pargs->sig_file, &fs) < 0 ) {
                return -FW_PACK_ERR_INFILE_READ;
        } else {
            /* copy the file size to pargs*/
            pargs->sig_file_size = fs.st_size;
        }
        rsfd = open(pargs->sig_file, O_RDONLY);

        wxfd = open(pargs->ofile, O_CREAT | O_WRONLY, 00766);

        if ((rffd <= 0) || (wxfd <= 0) || (rsfd <= 0)) {
            /* some thing wrong, close open files, and return */
            if (rffd > 0) close(rffd);
            if (wxfd > 0) close(wxfd);
            if (rsfd > 0) close(rsfd);
            return -1;
        }
        /* copy the original file to write file */
        if ( (err = fcopy_fd2fd(wxfd, rffd, pargs->in_file_size)) < 0) {
            printf("File creation Failed\n");
        }
        if ( (err = fcopy_fd2fd(wxfd, rsfd, pargs->sig_file_size)) < 0) {
            printf("File creattion complete\n");
        }
    }
    /* do cleanup*/
    if (rffd) close(rffd);
    if (rsfd) close(rsfd);
    if (wxfd) close(wxfd); 
    return err;
}
/*
 * fcopy_bfr2fd 
 *  
 * Copy from buffer to a file, file should open already for write/create. 
 * Input is in buffer, and length should be provided 
 * both files need to be closed by the caller 
 * return -1 on error 
 * return 0 on success 
*/
int
fcopy_bfr2fd(int wfd, unsigned char *bfr, unsigned int len)
{
    int m = 0, n = len, b=1;

    if (wfd < 0 ) return -FW_PACK_ERR_FILE_WRITE;
    if (!bfr) return -FW_PACK_ERR_INTERNAL;
    if (len <= 0) return -FW_PACK_ERR_INTERNAL;

    while (n) {
        if (n < b)
            b = n;
        if ( (m=write(wfd, bfr, b)) >= 0) {
            bfr += m;
            n -= m;
        } else {
            return -FW_PACK_ERR_FILE_WRITE;
        }
    }
    return 0;
}
int
fcopy_fd2fd(int wfd, int rfd, unsigned int len)
{
    unsigned char scratch; 
    int err=0;
    int m = 0, n = len, b = 1;
    
    /* FIXME Initially b is set to 512, while writing last chunk there seems 
       some issue. Rest of the code is left to work with this block size.
       
     * At this point block size is set to '1'. While any block size should
     * work, in some occasions extra 3 byte addition to files is observed. 
     * to avoid this, byte copy is chosen. Being this is only application
     * that would not add any overhead, yet this is sub-optimal, needs further
     * debug to understand why extra 3 bytes are added. 
     */
    if (wfd <= 0 || rfd <= 0) return -FW_PACK_ERR_FILE_WRITE;
    if (len <= 0) return -FW_PACK_ERR_FILE_WRITE;

    
    m = read(rfd, &scratch, b);
    while ( n > 0 && m > 0) {
        if ((fcopy_bfr2fd(wfd, &scratch, m)) >= 0) {
            n -= m;
        } else {
            return -FW_PACK_ERR_FILE_WRITE;
        }
        scratch=0;
        if (n < b) b = n;
        m = read(rfd, &scratch, b);
    }
    return 0;
}

/*
 * fcopy_fd2bfr`
 * 
*/
int
fcopy_fd2bfr(int rfd, unsigned char *bfr, unsigned int len)
{
    unsigned char *scratch=NULL; 
    int err=0;
    int m = 0, n = len, b = 1;
    unsigned int tread=0;

    if (rfd < 0 ) return -FW_PACK_ERR_FILE_WRITE;
    if (len <= 0) return -FW_PACK_ERR_FILE_WRITE;
    if (!bfr) return -FW_PACK_ERR_INTERNAL;

    if (len < b) b = len;
    
    memset(bfr, 0, b);

    m = read(rfd, bfr, b);
    while ( n > 0 && m > 0) {
        tread += m;
        if (n < b) b = n;
        bfr += m;
        if ((m = read(rfd, bfr, b)) >= 0) {
            n -= m;
        }
        if(m < 0) {
            /* probably read error */
            return -FW_PACK_ERR_FILE_ACCESS;
        }
    }
    return tread;
}
void
fw_pack_error(int err)
{
    if (-err < FW_PACK_ERR_MAX) {
        fprintf(stderr, "error %d:%s\n", err, fw_pack_errs[-err]);
        exit(-1);
    }
}

/*
 * Load the firmware into memory 
 *
 * Memory is allocated here, and freed outside, that is
 * inline with request_firmware approach
 */
unsigned char *
load_firmware(const char *file)
{
    int rfd = -1, err  = 0;
    unsigned char *bfr = NULL;
    struct stat fs;

    if (!file) return NULL;
    if (stat(file, &fs) >= 0) {
        bfr = malloc(fs.st_size);
        memset(bfr, 0, fs.st_size);
        /* open the file, and load it to memory */
        if ((rfd = open(file, O_RDONLY)) > 0) {
            err = fcopy_fd2bfr(rfd, bfr, fs.st_size);
        }
    }
    if (rfd < 0 || err < 0)  {
        free(bfr);
        bfr = NULL;
    }
    if (rfd) close(rfd);
    return bfr;
}

struct firmware_head *
fw_unpack(unsigned char *fw, int chip_id, int *err)
{
    struct firmware_head *th, *h;
    unsigned char *data, *signature;
    int i = 0;

    if (!fw) return FW_PACK_ERR_INTERNAL;

    /* at this point, the firmware header is in network byte order
     * load that into memort and convert to host byte order 
     */
    th = (struct firmware_head*)fw; 

    if (th) {
        ntohlm(fw, sizeof(struct firmware_head));
    }
    /* do not access header before this line */
    h = th;
    fw_hex_dump((unsigned char*)h, sizeof(struct firmware_head));
    signature = (unsigned char*)h + h->fw_hdr_length + h->fw_img_size;
    data = (unsigned char *)h + sizeof(struct firmware_head);
    if (fw_debug) {
        printf("Firmware header base        :0x%pK\n", h);
        printf("                length      :0x%8x\n", h->fw_hdr_length);
        printf("                img size    :0x%8x\n", h->fw_img_size);
        printf("                img magic   :0x%8x\n", h->fw_img_magic_number);
        printf("                chip id     :0x%8x\n", h->fw_chip_identification);
        printf("                file magic  :0x%8x\n", h->fw_img_file_magic);
        printf("                signing ver :0x%8x\n", h->fw_img_sign_ver);
        printf("                ver M/S/E   :%x/%x/%x\n",
                                                    FW_VER_GET_MAJOR(h),
                                                    FW_VER_GET_IMG_TYPE(h),
                                                    FW_VER_GET_IMG_TYPE_VER(h));
        printf("                flags       :%x\n",h->fw_hdr_flags);
        printf("                sign length :%8x\n", h->fw_sig_len);
        printf(" ==== HEX DUMPS OF REGIONS ====\n");
        printf("SIGNATURE:\n");
        fw_hex_dump((unsigned char*)signature, h->fw_sig_len);
        printf("\nDATA\n");
        fw_hex_dump((unsigned char*)data, h->fw_img_size);
        printf("=== HEX DUMP END ===\n");
    }
    if ( (i=fw_check_img_magic(h->fw_img_magic_number)) >= 0) {
        printf("Chip Identification:%x Device Magic %x: %s found\n",
                fw_auth_supp_devs[i].dev_id,
                fw_auth_supp_devs[i].img_magic, 
                fw_auth_supp_devs[i].dev_name);
    } else {
        *err = -FW_PACK_ERR_UNSUPP_CHIPSET;
        return NULL;
    }
    if ( (chip_id == h->fw_chip_identification) &&
            (fw_check_chip_id(h->fw_chip_identification) >= 0)) {
        printf("Chip Identification:%x Device Magic %x: %s found\n",
                fw_auth_supp_devs[i].dev_id,
                fw_auth_supp_devs[i].img_magic, 
                fw_auth_supp_devs[i].dev_name);
    } else {
        *err =  -FW_PACK_ERR_INV_DEV_ID;
        return NULL;
    }

    if (fw_pack_check_file_magic(h->fw_img_file_magic) >= 0) {
        printf("Found good image magic %x\n", h->fw_img_file_magic);
    } else {
        *err = -FW_PACK_ERR_INVALID_FILE_MAGIC;
        return NULL;
    }
    /* dump various versions */
    if (h->fw_img_sign_ver && (h->fw_img_sign_ver  != THIS_FW_IMAGE_VERSION)) {
        printf("Firmware is not signed with same version, as the tool\n");
        *err = -FW_PACK_ERR_IMAGE_VER;
        return NULL;
    }
    /* check and dump the versions that are available in the file for now
     * TODO roll back check would be added in future
     */
    printf("Major: %d Enter Prise: %d Enterprise Release %d\n",
            FW_VER_GET_MAJOR(h), FW_VER_GET_IMG_TYPE(h), FW_VER_GET_IMG_TYPE_VER(h));

    printf("Minor: %d sub version:%d build_id %d\n", 
            FW_VER_IMG_GET_MINOR_VER(h),
            FW_VER_IMG_GET_MINOR_SUBVER(h),
            FW_VER_IMG_GET_MINOR_RELNBR(h));

    printf("Header Flags %x\n", h->fw_hdr_flags);
    /* sanity can be added, but ignored for now, if this goes
     * wrong, file authentication goes wrong 
     */
    printf("image size %d \n", h->fw_img_size);
    printf("image length %d \n", h->fw_img_length);
    /* check if signature algorithm is supported or not */
    if (!fw_check_sig_algorithm(h->fw_sig_algo)) {
        printf("image signing algorithm mismatch %d \n", h->fw_sig_algo);
        *err = -FW_PACK_ERR_SIGN_ALGO;
        return NULL;
    }

    /* do not check oem for now */
    /* TODO dump the sinagure now here, and passit down to kernel
     * crypto module 
     */
    return h;
}

void free_secure_firmware(unsigned char *fw)
{
#if __KERNEL__
    kfree(fw );
#else
    free(fw);
#endif
}
int 
fw_pack_get_hash_algo (struct firmware_head *h)
{
    if(!h) return -1;

    unused(h);
    /* right now do not check any thing but return one known
     * algorithm, fix this by looking at versions of the 
     * signing
     */
#define HASH_ALGO_SHA256 4
    return HASH_ALGO_SHA256;
}

/*
 * At this time, do not use any thing, but return the same known algorithm type. Ideally we should
 * add this by knowing the file type and signature version
 */
int 
fw_pack_get_pk_algo (struct firmware_head *h)
{
    if(!h) return -1;

    unused(h);
    /* FIXME based on signing algorithm, we should choose
     * different keys, right now return only one
     */
#define PKEY_ALGO_RSA 1
    return PKEY_ALGO_RSA;
}

/* 
 * get certficate buffer based on file type and signing version
 */
const unsigned char  *
fw_sign_get_cert_buffer(struct firmware_head *h, int *cert_type, int *len)
{
    int idx=0;

    if (!h) return NULL;

    /* based on signing version, we should be filling these numbers, right now now checks */
#define PKEY_ID_X509 1
    *cert_type = PKEY_ID_X509;
    switch (h->fw_img_file_magic) 
    {
        case FW_IMG_FILE_MAGIC_TARGET_WLAN:
            idx = 0;
            break;
        case FW_IMG_FILE_MAGIC_TARGET_OTP:
            idx = 1;
            break;
        case FW_IMG_FILE_MAGIC_TARGET_BOARD_DATA:
            idx = 2;
            break;
        case FW_IMG_FILE_MAGIC_TARGET_CODE_SWAP:
            idx = 3;
            break;
        default:
            return NULL;
    }
    if (h->fw_ver_rel_type == FW_IMG_VER_REL_TEST) {
        *len = fw_test_certs[idx].cert_len;
        return &fw_test_certs[idx].cert[0];
    } else if (h->fw_ver_rel_type == FW_IMG_VER_REL_PROD) {
        *len = fw_prod_certs[idx].cert_len;
        return &fw_prod_certs[idx].cert[0];
    } else {
        return NULL;
    }
}
inline int
fw_check_sig_algorithm(int s)
{
    switch (s) {
        case RSA_PSS1_SHA256:
            return 1;
    }
    return 0;
}

int test_authenticate_fw(struct auth_input *ai)
{
    return 0;
}
/* interface function to return the firmware file pointers
 * Description: Load the firmware, check security attributes and then see if this requires
 *              firmware authentication or not. If no firmware authentication required 
 *              return firmware pointer, otherwise, returns the firmware pointer iff signature
 *              checks are good.
 *  In 
 */
unsigned char *
request_secure_firmware(const char *file, int auth, int dev_id, unsigned char **free_ptr)
{
    unsigned char *fw;
    unsigned int len=0;
    struct auth_input fw_check_buffer;
    int cert_type, err=0;
    struct firmware_head *h; 

    if(!file) return NULL;
#if __KERNEL__
    fw = request_firmware(file);
#else
    fw = load_firmware(file);
#endif
    h = fw_unpack(fw, dev_id, &err);
    if(!h) {
        fprintf(stderr, "something wrong in reading the firmware header \n");
        free_secure_firmware(fw);
        *free_ptr = NULL;
        return NULL;
    }
    *free_ptr = fw;
    /* we assume post the integration of the code, all files continue to have this header
     * if not, there is some thing wrong
     * Also in case of signed files, firmware signing version would be present and 
     * if that is zero, that is not signed 
     */
    if (h && !h->fw_img_sign_ver) return fw + h->fw_hdr_length;

    /* now the fimrware is unpacked, build and pass it to kernel
     */
    memset(&fw_check_buffer, 0, sizeof(fw_check_buffer));
    fw_check_buffer.certBuffer = (unsigned char *)fw_sign_get_cert_buffer(h, &cert_type, &len);
    if (fw_check_buffer.certBuffer) {
        fw_check_buffer.signature = fw + h->fw_hdr_length + h->fw_img_size;
        fw_check_buffer.data = fw + h->fw_hdr_length;
        fw_check_buffer.cert_len = len;
        fw_check_buffer.sig_len = h->fw_sig_len;
        fw_check_buffer.data_len = h->fw_img_size;
        fw_check_buffer.sig_hash_algo = fw_pack_get_hash_algo(h);
        fw_check_buffer.pk_algo = fw_pack_get_pk_algo(h);
        fw_check_buffer.cert_type = cert_type;
        /* we are done with all, now call into linux and see what it returns, 
         * if authentication is good, return the pointer to fw, otherwise, 
         * free the buffer and return NULL
         */
        if (test_authenticate_fw(&fw_check_buffer) < 0)  {
            free_secure_firmware(fw);
            *free_ptr = NULL;
            return NULL;
        }
    } else {
        free_secure_firmware(fw);
        *free_ptr = NULL;
        return NULL;
    }

    return fw;
}
/* test routines here */
void
fw_unpack_test(char *f, struct fw_pack_cmd_args *pargs)
{
    unsigned char *fw;
    struct firmware_head *h;
    int err = 0;

    if (!f) return;
    fw = request_secure_firmware(f, 1, 0x40, &fw);
    if (!fw) return;
}
#if !defined( __KERNEL__)
/* main is not to be defined in kernel mode, even though this is not
 * used in kernel drivers now
  */
int 
main(int argc, char *argv[])
{
    struct fw_pack_cmd_args pargs;
    int err=0;

    cmd_args = argv;

    memset(&pargs, 0, sizeof(pargs));
    if (argc == 1) {
        usage();
    }
    /* initialize the global defaults */
    if ((err=fw_pack_init_globals(&pargs)) < 0) {
        printf("error1");
        fw_pack_error(err);
    }
    if((err = fw_pack_parse_args(argc, argv, &pargs)) <0) {
        printf("error2");
        fw_pack_error(err);
    }
    if(pargs.header_only && (err = fw_pack_cmd_args_check(&pargs)) < 0) {
        printf("error3");
        fw_pack_error(err);
    }
    if(pargs.header_only) fw_header_encap(&pargs);
    if(pargs.sign_only) fw_sign_encap(&pargs);

    if (pargs.in_file && pargs.test_file)
        fw_unpack_test(pargs.in_file, &pargs);
    return 0;
}
#endif
