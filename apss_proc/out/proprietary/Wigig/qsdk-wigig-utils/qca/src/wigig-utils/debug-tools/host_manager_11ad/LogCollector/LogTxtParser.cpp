/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/
/* Start of vfprintf implementation taken from musl libc [1]
* distributed under standard MIT license [2].
*
* [1] http://git.musl-libc.org/cgit/musl/tree/src/stdio/vfprintf.c
* [2] http://git.musl-libc.org/cgit/musl/tree/COPYRIGHT
*/

#include "LogTxtParser.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <climits>
#include <cfloat>
#include "DebugLogger.h"
#include "LogCollectorDefinitions.h"

using namespace std;
namespace log_collector
{


#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

    //typedef unsigned int uint;

    OperationStatus LogTxtParser::LogParserInit(const char* stringConversionPath, CpuType cpuType,
        struct log_collector::module_level_info* modulesInfo)
    {
        SetStates();
        m_newLineRequired = true;
        m_prevModule = -1;
        size_t extra_space;

        /* reserve extra space at the end of string buffer for a special
        * string that we write to the log when we encounter corrupted
        * offsets for string parameters. This can happen when log is
        * wrapped in the middle of a log message
        */
        extra_space = strlen(CORRUPTED_PARAM_MARK) + 1;
        std::stringstream converionFileName;
        converionFileName << stringConversionPath;
        if (cpuType == CPU_TYPE_FW)
        {
            LOG_DEBUG << "cputype is : FW " << std::endl;
            converionFileName << "fw_image_trace_string_load.bin";
        }
        else if (cpuType == CPU_TYPE_UCODE)
        {
            LOG_DEBUG << "cputype is : uCode " << std::endl;
            converionFileName << "ucode_image_trace_string_load.bin";
        }

        if (m_stringsConversionBuffer)
        {
            delete(m_stringsConversionBuffer);
            m_stringsConversionBuffer = nullptr;
        }
        m_stringsConversionBuffer = ReadStringsConversionFile(converionFileName.str().c_str(), str_sz, extra_space);
        if (!m_stringsConversionBuffer)
        {
            return OperationStatus(false, "Could not open strings file at: " + converionFileName.str() + " Please make sure that the file exist and you have the right permissions. Or record Raw logs instead.");
        }
        //mod = m_stringsConversionBuffer;
        snprintf(m_stringsConversionBuffer + str_sz, extra_space, "%s", CORRUPTED_PARAM_MARK);
        ReadModuleNames();
        m_modulesInfo = modulesInfo;
        return OperationStatus();
    }


    char *LogTxtParser::ReadStringsConversionFile(const char *name, size_t & outRealSize, size_t extra)
    {
        outRealSize = 0;
        std::ifstream strings_file(name);
        if (!strings_file.is_open())
        {
            //could not open file
            LOG_ERROR << "Could not open strings file! " << name << std::endl;
            return nullptr;
        }
        strings_file.seekg(0, strings_file.end);
        std::streampos size = strings_file.tellg();
        outRealSize = (size_t)size;
        LOG_DEBUG << "outRealSize is: " << outRealSize << std::endl;

        char *buf;
        buf = new char[outRealSize + extra];
        strings_file.seekg(0, strings_file.beg);
        strings_file.read(buf, size);
        LOG_DEBUG << "buf is: " << buf << std::endl;
        strings_file.close();
        return buf;
    }


    std::string LogTxtParser::ParseLogMessage(unsigned stringOffset, const std::vector<int> &params,
        unsigned module, unsigned verbosityLevel, const std::string &timeStamp)
    {
        if (stringOffset >= str_sz || !m_stringsConversionBuffer)
        {
            std::stringstream invalidSs;
            invalidSs << timeStamp << " >>>> Invalid string offset: " << stringOffset << std::endl;
            return invalidSs.str();
        }
        const char *fmt = m_stringsConversionBuffer + stringOffset;
        LOG_VERBOSE << "DBG_MESSAGE: \nstringOffset: " << stringOffset << "\nparams length: " << params.size() <<
            "\ntimestamp: " << timeStamp << "\nverbosityLevel: " << verbosityLevel << std::endl;

        std::stringstream ssPrefix;
        ssPrefix << timeStamp << GetVerbosityLevelByIndex(verbosityLevel) <<
            GetModuleNameByIndex(module) << " ";
        return ParseMessageFmt(ssPrefix.str(), module, fmt, params);
    }

    std::string LogTxtParser::ParseMessageFmt(const std::string &prefix, int module, const char* fmt, const std::vector<int> &params)
    {
        std::stringstream ss;
        if (m_newLineRequired)
        {
            if (strncmp(NNL, fmt, strlen(NNL)) == 0)
            {
                if (m_prevModule != module)
                {
                    ss << (!m_modulesInfo[m_prevModule].third_party_mode ? "\n" : ""); // add a new line only if the previous module was not third party.
                    ss << prefix << " Message prefix missing, suffix: ";
                }
                else
                {
                    fmt += strlen(NNL);
                }
            }
            else
            {
                // If we got here we there are 2 options: either the prev module was thirdPartymodule and we already added \n
                ss << (((m_prevModule != -1) && !m_modulesInfo[m_prevModule].third_party_mode) ? "\n" : "") << prefix;
            }

        }
        else
        {
            if (m_prevModule != module && m_modulesInfo[m_prevModule].third_party_mode)
            {
                LOG_VERBOSE << "Module mismatch starting new line" << std::endl;
                ss << " >>>> Message suffix truncated missing \\n";
                ss << "\n";
                ss << prefix;
            }
            if (strncmp(NNL, fmt, strlen(NNL)) == 0 && m_modulesInfo[m_prevModule].third_party_mode)
            {

                LOG_VERBOSE << "Module mismatch starting new line" << std::endl;
                ss << " >>>> Message prefix missing, suffix: ";
            }
            else if (!m_modulesInfo[m_prevModule].third_party_mode)
            {
                LOG_VERBOSE << "just sanity check for third party mode" << std::endl;
                ss << "\n";
                ss << prefix;
            }
        }
        m_newLineRequired = !m_modulesInfo[module].third_party_mode;
        LOG_VERBOSE << "DBG_MESSAGE: " <<
            "\nline prefix: " << ss.str() << "\nfmt: " << fmt << std::endl;
        print_trace_string(ss, fmt, params, prefix);
        m_prevModule = module;
        return ss.str();
    }
    const char *LogTxtParser::next_mod(const char *mod) const
    {
        mod = strchr(mod, '\0') + 1;
        while (*mod == '\0')
        {
            mod++;
        }
        return mod;
    }


    void LogTxtParser::ReadModuleNames()
    {
        const char* mod = m_stringsConversionBuffer;
        for (size_t i = 0; i < MaxModules; i++, mod = next_mod(mod))
        {
            std::stringstream ssModuleName;
            ssModuleName << std::left;
            ssModuleName << "[" << std::setw(7) << mod << "]";
            m_moduleNames[i] = ssModuleName.str();
        }
        m_moduleNames[MaxModules] = "[INVALID MODULE INDEX]";
    }

    const char * LogTxtParser::GetVerbosityLevelByIndex(unsigned int index) const
    {
        if (index >= MaxVerboseLevel)
        {
            return "[INVALID VERBOSITY INDEX]";
        }
        return m_levels[index];
    }

    const std::string& LogTxtParser::GetModuleNameByIndex(unsigned int index)
    {
        if (index >= MaxModules)
        {
            return m_moduleNames[MaxModules];
        }
        return m_moduleNames[index];
    }

    namespace
    {
#define ALT_FORM (1U << ('#' - ' '))
#define ZERO_PAD (1U << ('0' - ' '))
#define LEFT_ADJ (1U << ('-' - ' '))
#define PAD_POS (1U << (' ' - ' '))
#define MARK_POS (1U << ('+' - ' '))
#define GROUPED (1U << ('\'' - ' '))

#define FLAGMASK (ALT_FORM | ZERO_PAD | LEFT_ADJ | PAD_POS | MARK_POS | GROUPED)
        void outString(std::stringstream &ss, const char *s, size_t l)
        {
            LOG_VERBOSE << "got *s: " << s << " printing *s: " << *s << std::endl;
            for (; l > 0; l--)
            {
                ss << s[0];
                s++;
            }
        }

        void padString(std::stringstream &ss, char c, int w, int l, int fl)
        {
            char pad[256];

            if (fl & (LEFT_ADJ | ZERO_PAD) || l >= w)
                return;
            l = w - l;
            memset(pad, c, (size_t)l > sizeof(pad) ? sizeof(pad) : (size_t)l);
            for (; (size_t)l >= sizeof(pad); l -= sizeof(pad))
                outString(ss, pad, sizeof(pad));
            outString(ss, pad, l);
        }

        const char xdigits[16] = { '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F' };

        char *fmt_x(uintmax_t x, char *s, int lower)
        {
            for (; x; x >>= 4)
                *--s = static_cast<char>(xdigits[(x & 15)] | lower);
            return s;
        }

        char *fmt_u(uintmax_t x, char *s)
        {
            unsigned long y;
            for (; x > ULONG_MAX; x /= 10)
            {
                *--s = '0' + x % 10;
            }

            for (y = static_cast<unsigned long>(x); y; y /= 10)
                *--s = '0' + y % 10;
            return s;
        }


        int getint(char **s)
        {
            int i;

            for (i = 0; isdigit(**s); (*s)++) {
                if ((unsigned int)i > INT_MAX / 10U || **s - '0' > INT_MAX - 10 * i)
                    i = -1;
                else
                    i = 10 * i + (**s - '0');
            }
            return i;
        }

        size_t string_length(const char *s, size_t n)
        {
            size_t len = 0;

            if (!s)
                return 0;

            while (n-- > 0 && *s++)
                len++;
            return len;
        }
    }

    /* State machine to accept length modifiers + conversion specifiers.
    * Result is 0 on failure, or an argument type to pop on success.
    */
    enum TYPE_STATE_ENUM {
        BARE, LPRE, LLPRE,
        STOP,
        PTR, INT_S, UINT_S, ULLONG_S, LONG_S, ULONG_S, SHORT_S, USHORT_S,
        CHAR_S, UCHAR_S, LLONG, SIZET, IMAX, UMAX, PDIFF, UIPTR,
        DBL, LDBL, MAXSTATE //NOARG, MAXSTATE
    };

#define OOB(x) ((unsigned int)(x) - 'A' > 'z' - 'A')
#define S(x)((x) - 'A')

    void LogTxtParser::SetStates()
    {
        /* 0: bare types */
        states[0][S('d')] = INT_S;
        states[0][S('x')] = UINT_S;
        states[0][S('g')] = DBL;
        states[0][S('G')] = DBL;
        states[0][S('s')] = PTR;
        states[0][S('i')] = INT_S;
        states[0][S('X')] = UINT_S;
        states[0][S('a')] = DBL;
        states[0][S('A')] = DBL;
        states[0][S('S')] = PTR;
        states[0][S('l')] = LPRE;
        states[0][S('o')] = UINT_S;
        states[0][S('e')] = DBL;
        states[0][S('E')] = DBL;
        states[0][S('c')] = CHAR_S;
        states[0][S('p')] = UIPTR;
        states[0][S('u')] = UINT_S;
        states[0][S('f')] = DBL;
        states[0][S('F')] = DBL;
        states[0][S('C')] = INT_S;
        states[0][S('n')] = PTR;
        /* 1: l-prefixed */
        states[1][S('d')] = LONG_S;
        states[1][S('x')] = ULONG_S;
        states[1][S('g')] = DBL;
        states[1][S('G')] = DBL;
        states[1][S('n')] = PTR;
        states[1][S('i')] = LONG_S;
        states[1][S('X')] = ULONG_S;
        states[1][S('a')] = DBL;
        states[1][S('A')] = DBL;
        states[1][S('l')] = LLPRE;
        states[1][S('o')] = ULONG_S;
        states[1][S('e')] = DBL;
        states[1][S('E')] = DBL;
        states[1][S('c')] = INT_S;
        states[1][S('u')] = ULONG_S;
        states[1][S('f')] = DBL;
        states[1][S('F')] = DBL;
        states[1][S('s')] = PTR;
        /* 2: ll-prefixed */
        states[2][S('d')] = LLONG;
        states[2][S('u')] = ULLONG_S;
        states[2][S('n')] = PTR;
        states[2][S('i')] = LLONG;
        states[2][S('x')] = ULLONG_S;
        states[2][S('o')] = ULLONG_S;
        states[2][S('X')] = ULLONG_S;
    }


    bool LogTxtParser::pop_arg(int type, const std::vector<int> & params, unsigned int &lastParamIndex, arg_t &newArg)
    {
        newArg = { 0 };
        intmax_t tmpRes = 0;
        LOG_VERBOSE << "In pop_arg, type is: " << type << " last param index is: " << lastParamIndex << std::endl;
        switch (type) {
        case PTR:
            if (lastParamIndex >= params.size() ||
                static_cast<size_t>(params[lastParamIndex] & str_mask) >=
                str_sz) {
                newArg.p = CORRUPTED_PARAM_MARK;
                lastParamIndex++;
                return false;
            }
            newArg.p = m_stringsConversionBuffer +
                (params[lastParamIndex++] & str_mask);
            break;
        case CHAR_S:
        case SHORT_S:
        case INT_S:
        case LONG_S:
        case PDIFF:
            if (lastParamIndex >= params.size()) {
                newArg.i = static_cast<uintmax_t>(-1);
                return false;
            }
            newArg.i = static_cast<int>(params[lastParamIndex++]);
            break;

        case UCHAR_S:
        case USHORT_S:
        case UINT_S:
        case ULONG_S:
        case UIPTR:
        case SIZET:
            if (lastParamIndex >= params.size()) {
                newArg.i = static_cast<uintmax_t>(-1);
                return false;
            }

            newArg.i = static_cast<unsigned int>(params[lastParamIndex++]);
            break;
        case LLONG:
        case IMAX:
            if (lastParamIndex + 1 >= params.size()) {
                newArg.i = static_cast<uintmax_t>(-1);
                return false;
            }

            newArg.i = static_cast<intmax_t>(params[lastParamIndex++]) & static_cast<intmax_t>(0xFFFFFFFF);
            newArg.i |= static_cast<intmax_t>(params[lastParamIndex++]) << 32;
            break;
        case ULLONG_S:
        case UMAX:
            if (lastParamIndex + 1 >= params.size()) {
                newArg.i = static_cast<uintmax_t>(-1);
                return false;
            }

            tmpRes = static_cast<intmax_t>(params[lastParamIndex++]) & static_cast<intmax_t>(0xFFFFFFFF);
            tmpRes |= static_cast<intmax_t>(params[lastParamIndex++]) << 32;
            newArg.i = static_cast<uintmax_t>(tmpRes);
            break;
            /* floating point types are not supported at the moment */
        case DBL:
        case LDBL:
            if (lastParamIndex + 1 >= params.size()) {
                newArg.f = static_cast<double>(-1);
                return false;
            }
            newArg.f = -1.0;
            lastParamIndex += 2;
            break;
        default:
            LOG_ERROR << "in pop_arg invalid type specifier" << endl;
        }

        return true;
    }


    int LogTxtParser::printf_core_ss(std::stringstream &ss, const char *fmt, arg_t *nl_arg,
        int *nl_type, const std::string &module_string, const std::vector<int> &params)
    {
        unsigned int lastParamIndex = 0;
        char *a, *z, *s = const_cast<char *>(fmt);
        unsigned int fl;
        int w, p, xp;
        union arg_t arg = {};
        int argpos;
        unsigned int stateMachineCurrentState, ps;
        int totalPrintCount = 0, l = 0;
        size_t i;
        char buf[sizeof(uintmax_t) * 3 + 3 + LDBL_MANT_DIG / 4];
        //const char *prefix;
        int t, pl;
        wchar_t wc[2];
        const wchar_t *ws;
        char mb[4];

        for (;;) {
            /* This error is only specified for snprintf, but since it's
            * unspecified for other forms, do the same. Stop immediately
            * on overflow; otherwise %n could produce wrong results.
            */
            if (l > INT_MAX - totalPrintCount)
            {
                LOG_VERBOSE << "Invalid string format (overflow occurred)" << std::endl;
                return -1;
            }

            /* Consume newlines */
            for (; *s && *s == '\n'; s++) {
                ss << '\n';
                if (s[1])
                {
                    ss << module_string;
                }
                else
                {
                    // the line finished wit \n so we want to start a new line
                    m_newLineRequired = true;
                }
                l++;
            }

            /* Update output count, end loop when fmt is exhausted */
            totalPrintCount += l;
            if (!*s)
            {
                break;
            }

            /* Handle literal text and %% format specifiers */
            for (a = s; *s && *s != '%' && *s != '\n'; s++) // read fmt before %
                ;
            for (z = s; s[0] == '%' && s[1] == '%'; z++, s += 2) // check if there are %%
                ;
            if (z - a > INT_MAX - totalPrintCount) //maybe change to z-a < 0 ??
            {
                LOG_VERBOSE << "Invalid string format (overflow occurred)" << std::endl;
                return -1;
            }
            l = z - a;
            outString(ss, a, l);

            if (l)
            {
                continue;
            }

            if (isdigit(s[1]) && s[2] == '$') {
                argpos = s[1] - '0';
                s += 3;
            }
            else {
                argpos = -1;
                s++;
            }

            /* Read modifier flags */
            for (fl = 0;
                static_cast<unsigned int>(*s) - ' ' < 32 &&
                (FLAGMASK & (1U << (*s - ' ')));
                s++)
            {
                fl |= 1U << (*s - ' ');
            }

            w = getint(&s);
            if (w < 0)
            {
                LOG_VERBOSE << "Invalid string format (overflow occurred)" << std::endl;
                return -1;
            }
            p = -1;
            xp = 0;

            /* Format specifier state machine */
            stateMachineCurrentState = 0;
            do {
                if (OOB(*s)) // Checks that the current char is a letter.
                {
                    LOG_VERBOSE << "Got invalid type specifier (OOB): " << (*s) << " Formatted string is: " << fmt << std::endl;
                    return -1;
                }

                ps = stateMachineCurrentState;
                stateMachineCurrentState = states[stateMachineCurrentState][S(*s++)];
                LOG_VERBOSE << "stateMachineCurrentState: " << stateMachineCurrentState << "ps: " << ps << std::endl;
            }
            // once we got invalid state we stop.
            while (stateMachineCurrentState > 0 && stateMachineCurrentState < STOP);
            if (!stateMachineCurrentState)
            {
                LOG_VERBOSE << "Got invalid type specifier (BAD_STATE): " << (*s) << " Formatted string is: " << fmt << std::endl;
                return -1;
            }

            if (argpos >= 0)
            {
                nl_type[argpos] = stateMachineCurrentState, arg = nl_arg[argpos];
            }
            else
            {
                if (!pop_arg(stateMachineCurrentState, params, lastParamIndex, arg))
                {
                    if (g_LogConfig.ShouldPrint(LOG_SEV_VERBOSE))
                    {
                        stringstream ssParams;
                        for (auto pv : params)
                        {
                            ssParams << pv << ", ";
                        }
                        LOG_VERBOSE << "Error in popping args:\n fmt is:" << fmt << "\nNumber of params provided is: " << params.size() << " params are: " << ssParams.str() << std::endl;
                    }
                }
            }


            z = buf + sizeof(buf);
            const char *prefix = "-+   0X0x";
            pl = 0;
            t = s[-1];

            /* Transform ls,lc -> S,C */
            if (ps && (t & 15) == 3)
            {
                t &= ~32;
            }

            /* - and 0 flags are mutually exclusive */
            if (fl & LEFT_ADJ)
            {
                fl &= ~ZERO_PAD;
            }

            // Valid t values: d|x|X|s|c|i|u|p|lld|llx|llX|lli|llu
            switch (t) {
            case 'n':
                switch (ps) {
                case BARE:
                    *(int *)arg.p = totalPrintCount;
                    break;
                case LPRE:
                    *(long *)arg.p = totalPrintCount;
                    break;
                case LLPRE:
                    *(long long *)arg.p = totalPrintCount;
                    break;
                }
                continue;
            case 'p':
                p = MAX((size_t)p, 2 * sizeof(void *));
                t = 'x';
                fl |= ALT_FORM;
            case 'x':
            case 'X':
                a = fmt_x(arg.i, z, t & 32);
                if (arg.i && (fl & ALT_FORM))
                    prefix += (t >> 4), pl = 2;
                if (xp && p < 0)
                {
                    LOG_VERBOSE << "Invalid string format (overflow occurred)" << std::endl;
                    return -1;
                }
                if (xp)
                {
                    fl &= ~ZERO_PAD;
                }
                if (!arg.i && !p) {
                    a = z;
                    break;
                }
                p = MAX(p, z - a + !arg.i);
                break;
            case 'd':
            case 'i':
                LOG_VERBOSE << "got %d. arg.i is: " << arg.i << std::endl;
                pl = 1;
                if (arg.i > INTMAX_MAX)
                    arg.i = ~arg.i + 1;
                else if (fl & MARK_POS)
                    prefix++;
                else if (fl & PAD_POS)
                    prefix += 2;
                else
                    pl = 0;
            case 'u':
                a = fmt_u(arg.i, z);
                if (xp && p < 0)
                {
                    LOG_VERBOSE << "Invalid string format (overflow occurred)" << std::endl;
                    return -1;
                }
                if (xp)
                    fl &= ~ZERO_PAD;
                if (!arg.i && !p) {
                    a = z;
                    break;
                }
                p = MAX(p, z - a + !arg.i);
                break;
            case 'c':
                *(a = z - (p = 1)) = static_cast<char>(arg.i);
                fl &= ~ZERO_PAD;
                break;
            case 's':
                a = (char *)(arg.p ? arg.p : "(null)");
                z = a + string_length(a, p < 0 ? INT_MAX : p);
                if (p < 0 && *z)
                {
                    LOG_VERBOSE << "Invalid string format (overflow occurred)" << std::endl;
                    return -1;
                }
                p = z - a;
                fl &= ~ZERO_PAD;
                break;
            case 'C':
                wc[0] = static_cast<wchar_t>(arg.i);
                wc[1] = 0;
                arg.p = wc;
                p = -1;
            case 'S':
                ws = static_cast<const wchar_t*>(arg.p);
                for (i = l = 0;
                    i < static_cast<size_t>(p) &&
                    *ws &&
                    (l = wctomb(mb, *ws++)) >= 0 &&
                    l <= p - static_cast<int>(i);
                    i += l)
                    ;
                if (l < 0)
                {
                    LOG_VERBOSE << "Invalid string format (overflow occurred)" << std::endl;
                    return -1;
                }

                if (i > INT_MAX)
                {
                    LOG_VERBOSE << "Invalid string format (overflow occurred)" << std::endl;
                    return -1;
                }
                p = i;
                padString(ss, ' ', w, p, fl);
                ws = static_cast<const wchar_t*>(arg.p);
                for (i = 0;
                    i < 0U + p &&
                    *ws &&
                    i + (l = wctomb(mb, *ws++)) <= static_cast<size_t>(p);
                    i += l)
                    outString(ss, mb, l);
                padString(ss, ' ', w, p, fl ^ LEFT_ADJ);
                l = w > p ? w : p;
                continue;
                //case 'e':
                //case 'f':
                //case 'g':
                //case 'a':
                //case 'E':
                //case 'F':
                //case 'G':
                //case 'A':
                //    // moved here - not supported:
                //case 'o':
                //case 'm':
            default:
                LOG_WARNING << "Invalid format specifier: " << t << std::endl;
                continue;
            }

            if (p < z - a)
            {
                p = z - a;
            }
            if (p > INT_MAX - pl)
            {
                LOG_VERBOSE << "Invalid string format (overflow occurred)" << std::endl;
                return -1;
            }
            if (w < pl + p)
            {
                w = pl + p;
            }
            if (w > INT_MAX - totalPrintCount)
            {
                LOG_VERBOSE << "Invalid string format (overflow occurred)" << std::endl;
                return -1;
            }

            padString(ss, ' ', w, pl + p, fl);
            outString(ss, prefix, pl);
            padString(ss, '0', w, pl + p, fl ^ ZERO_PAD);
            padString(ss, '0', p, z - a, 0);
            outString(ss, a, z - a);
            padString(ss, ' ', w, pl + p, fl ^ LEFT_ADJ);

            l = w;
        }

        return totalPrintCount;
    }


    int LogTxtParser::print_trace_string(std::stringstream &ss, const char *fmt,
        const std::vector<int> &params, const std::string & prefix)
    {
        int ret;
        int nl_type[MaxParams + 1] = { 0 };
        arg_t nl_arg[MaxParams + 1];
        ret = printf_core_ss(ss, fmt, nl_arg, nl_type, prefix, params);
        return ret;
    }

    LogTxtParser::~LogTxtParser()
    {
        if (m_stringsConversionBuffer)
        {
            delete(m_stringsConversionBuffer);
        }
    }
}
