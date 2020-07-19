
--------------------------------------------------------------
To add a command to TLV2 using the "cmdRspDictGenSrc" utility
--------------------------------------------------------------

1. Make sure:
	- to get the latest halphy_tools component from P4 //components/src/wlanfw_halphy_tools.cnss/1.0
	- to have perl installed in your machine. Any version of Perl should work
	
2. Goto halphy_tools/common/tlv2p0/util directory

3. Create your command cmd*Handler.s file (where * is the command name) under util/input folder
See the format of the file in "To create an input .s file" below

4. Make sure to build the cmdRspDictGenSrc.exe on (Windows) or cmdRspDictGenSrc.out on Linux

5. On Windows, open util.sln using VS2012, and build in either Debug or Release configuration. 
The .exe file will be copied to bin folder.

6. On Linux, run "make -f makefile.Linux", the .out file will be in Linux folder

7. From the "util" folder, run addTlv2Cmd2Dict.pl to add command.

	Usage: perl addTlv2Cmd2Dict.pl input/cmd*Handler.s

This will update the following files:
	- cmdParser/cmdHandlerTbl.c
	- common/cmdRspParmsDict.c
	- common/src/*.s file (if applicable)
	- include/cmdRspParmsDict.h
	- include/cmdHandlers.h
	- include/tlv2Api.h
and add the new command handler files
	- cmdParser/cmd*Handler.c
	- include/cmd*Handler.h

Make sure the files to be updated are checked out (writeable).
NOTE: Please pay attention to the messages of addTlv2Cmd2Dict.pl, especially the errors and the "ALREADY EXISTS" warning.
If your parameter "ALREADY EXISTS", check the one already exists in the cmdRspPArmsDict.c for the type and default value.
If they are the same, you can ignore the WARNING; otherwise, consider if you can use the existing one; if not, change your parameter to a different name.

--------------------------------------------------------------
To add a system (internal) command to TLV2 using the "cmdRspDictGenSrc" utility
--------------------------------------------------------------
Use the same steps in "To add a command to TLV2 using the "cmdRspDictGenSrc" utility" with the following differences:

Step 3: The .s file should use SYSCMD for command and SYSRSP for response
Step 7: Same as step 7 above

This will update the following files (see *):
	*- cmdParser/sysCmdHandlerTbl.c
	*- common/sysCmdRspParmsDict.c
	- common/src/*.s file (if applicable)
	- include/cmdRspParmsDict.h
	- include/cmdHandlers.h
	*- include/tlv2SysApi.h
and add the new command handler files
	- cmdParser/cmd*Handler.c
	- include/cmd*Handler.h

--------------------------------------------------------------
To create an input .s file
--------------------------------------------------------------

Example: See the samples of .s files in "input" folder.
	 See cmdTestArray.s for defining different types of parameters, especially the array.

NOTE: Make sure the parameters are correctly aligned; otherwise UTF will crashes

The formats of the parameter entry:

Format 1: simple
--------
	type:name:numOfElements:specifier[:element1:element2:.]

Valid type: void, char, UINT8, UINT16, UINT32, INT8, INT16, INT32, DATA64, DATA128, DATA256, DATA512, DATA1024
Valid specifier: d(ecimal), u(nsigned decimal) , (he)x
element1..elementn: the default values; if not specified, they are set to 0

Note:
- Number of elements specified should be <= numOfElements
- DO NOT specify [:element1:element2:...] for arrays of 0's to avoid generating default arrays in *Array.s and *Data.s files


03/26/2015
Format 2: Add the ability to generate array parameter with number of elements as pre-defined macro (option2)
---------
	type:name:numOfElements_MACRO:numOfElements:specifier[:element1:element2:.]
	
Example: (see cmdTPCCALPWRHandler.s for more detail)

    cmd*Handler.s                                       cmd*Handler.h
    -------------                                       -------------
Option1:
INT16:tgtPwr2G:13:d                      --->         A_INT16	tgtPwr2G[13];

Option2:
#define NUM_CAL_GAINS_2G    13
INT16:tgtPwr2G:NUM_CAL_GAINS_2G:13:d     --->         A_INT16	tgtPwr2G[NUM_CAL_GAINS_2G];

11/11/2015
Format 3: repeat the default values 
--------
	type:name:numOfElements:specifier:nonRepeatedElems:repeat:repeatTime:repeatPatternSize:repeatPattern
	type:name:numOfElements:specifier:repeat:repeatTime:repeatPatternSize:repeatPattern[:nonRepeatedElems]
	type:name:numOfElements:specifier:repeat:repeatTime1:repeatPatternSize1:repeatPattern1:repeat:repeatTime2:repeatPatternSize2:repeatPattern2
	
where:	repeat: the repeat key word
		repeatTime: number of times the following pattern is repeated
		repeatPatternSize: number of elements in the repeated pattern
		repeatPatern: repeated pattern; each element separeted by :
		nonRepeatedElems: the non-repeated elements separated by :
	Note: If the repeatPatternSize > 1, then only 1 element is allowed in nonRepeatedElems.
		  If number of elements in nonRepeatedElems > 1, then only 1 element is allowed in repeatPatern (repeatPatternSize = 1)
		  In the 3rd case, only 1 repeated pattern is allowed to have more than 1 element.
		  
Example in cmdTxHandler.s
-----
INT32:pwrGainStepAC160:8:x:0x01010101:0x01010101:0x01010101:0x01010101:0x01010101:0x01010101:0x01010101:0x01010101
becomes
INT32:pwrGainStepAC160:8:x:repeat:8:1:0x01010101

Same default values for pwrGainStepAC160 but:
	- With Format 1, a default array of size 8 will be generated in s32Array.s file:
static A_INT32 PARM_PWRGAINSTEPAC160Array[8] = {0x1010101,0x1010101,0x1010101,0x1010101,0x1010101,0x1010101,0x1010101,0x1010101};

	- With Format 3, no default array will be generated
-----
INT32:pwrGainStartAC160:8:x:0x1c1c1c1c:0x1c1c1c1c:0x1c1c1c1c:0x1a1a1c1c:0x20202020:0x181c1e20:0x1e1e1e1e:0x181a1c1e
becomes
INT32:pwrGainStartAC160:8:x:repeat:3:1:0x1c1c1c1c:0x1a1a1c1c:0x20202020:0x181c1e20:0x1e1e1e1e:0x181a1c1e

Same default values for pwrGainStepAC160 but:
	- With Format 1, a default array of size 8 will be generated in s32Array.s file:
static A_INT32 PARM_PWRGAINSTARTAC160Array[8] = {0x1c1c1c1c,0x1c1c1c1c,0x1c1c1c1c,0x1a1a1c1c,0x20202020,0x181c1e20,0x1e1e1e1e,0x181a1c1e};

	- With Format 3, a default array of size 5 will be generated in s32Array.s file:
static A_INT32 PARM_PWRGAINSTARTAC160Array[8] = {0x1a1a1c1c,0x20202020,0x181c1e20,0x1e1e1e1e,0x181a1c1e};

Example to use repeat keyword
# cmd
CMD= repeat

# cmd parm
PARM_START:
UINT8:rep1:30:x:repeat:24:1:0xff:1:2:3
UINT8:rep2:30:x:repeat:3:3:0x1f:0x2f:0x3f:0xff
UINT8:rep3:30:x:1:2:3:repeat:25:1:0x55
UINT8:rep4:30:x:1:repeat:5:5:0x5f:0x6f:0x7f:0x8f:0x9f
UINT8:rep5:30:x:repeat:20:1:0xaa:repeat:4:3:0xa1:0xa2:0xa3
UINT8:rep6:30:x:repeat:5:3:0xa1:0xa2:0xa3:repeat:20:1:0xbb
PARM_END:

The default values of the parameters will be:
rep1[0..23]=0xff; rep1[24..26]={1,2,3}; rep[27..29]={0}
rep2[0..2]={0x1f,0x2f,0x3f}; rep2[3..5]={0x1f,0x2f,0x3f}; rep2[6..8]={0x1f,0x2f,0x3f}; rep2[9]=0xff; rep2[10..29]={0}
rep3[0]=1; rep3[1]=2; rep3[2]=3; rep3[3..27]={0x55}; rep33[28..29]={0} 
rep4[0]=1; rep4[1..5][6..10][11..15][16..20][21..25]={0x5f,0x6f,0x7f,0x8f,0x9f}; rep4[26..29]={0};
rep5[0..19]={0xaa}; rep5[20..22][23..25][26..28]={0xa1,0xa2,0xa3}; rep5[29]=0xa1
rep6[0..2][3..5][6..8][9..11][12..14]={0xa1,0xa2,0xa3}; rep[15..29]={0xbb}

--------------------------------------------------------------
To build tlv2 library with the new added command
--------------------------------------------------------------

Add the new cmd*Handler.c file to the TLV2p0.vcxproj/filters (Windows) or the cmdParser/makefile.Linux (Linux), and build the tlv2p0.


