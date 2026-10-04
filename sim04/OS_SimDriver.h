#ifndef OS_SIMDRIVER_H
#define OS_SIMDRIVER_H

// header files
#include <stdio.h>
#include <stdlib.h>
#include "datatypes.h"
#include "StringUtils.h"
#include "configops.h"
#include "metadataops.h"
#include "simulator.h"
#include "StandardConstants.h"

// Program constants
typedef enum
{
    MIN_NUM_ARGS = 3,
    LAST_FOUR_LETTERS = 4
} PRGRM_CONSTANTS;

// Command line struct for storing command line switch settings
typedef struct CmdLineDataStruct
{
    bool programRunFlag;
    bool configDisplayFlag;
    bool mdDisplayFlag;
    bool runSimFlag;

    char fileName[STD_STR_LEN];
} CmdLineData;

void clearCmdLineStruct(CmdLineData *clDataPtr);

bool processCmdLine(int numArgs, char **strVector, CmdLineData *clDataPtr);

void showCommandLineFormat(void);

#endif
