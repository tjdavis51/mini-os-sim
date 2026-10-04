#include "OS_SimDriver.h"
#include "configops.h"
#include "StringUtils.h"

int main(int argc, char **argv)
{
    // Initialize program

    // initialize variables
    ConfigDataType *configDataPtr = NULL;
    OpCodeType *metaDataPtr = NULL;
    char errorMessage[MAX_STR_LEN];
    CmdLineData cmdLineData;
    bool configUploadSuccess = false;

    // show title
    // function: printf
    printf("Simulator Program\n");
    printf("=================\n\n");

    // process command line, check for program run
    // at least one correct switch and config file name (at end) verified
    // function: processCmdLine
    if (processCmdLine(argc, argv, &cmdLineData))
    {
        // upload config file, check for success
        // function: getConfigData
        if (getConfigData(cmdLineData.fileName, &configDataPtr, errorMessage))
        {
            // check config display flag
            if (cmdLineData.configDisplayFlag)
            {
                // display config data
                // function: displayConfigData
                displayConfigData(configDataPtr);
            }

            // set config upload success flag
            configUploadSuccess = true;
        }

        // otherwise, assume config file upload failure
        else
        {
            // show error message, end program
            // function: printf
            printf("\nConfig Upload Error: %s, program aborted\n\n",
                   errorMessage);
        }

        // check for config success and need for metadata
        if (configUploadSuccess && (cmdLineData.mdDisplayFlag || cmdLineData.runSimFlag))
        {
            // upload meta data file, check for success
            // function: getMetaData
            if (getMetaData(configDataPtr->metaDataFileName, &metaDataPtr, errorMessage))
            {
                // check meta data display flag
                if (cmdLineData.mdDisplayFlag)
                {
                    // display meta data
                    // function: displayMetaData
                    displayMetaData(metaDataPtr);
                }

                // check run simulator flag
                if (cmdLineData.runSimFlag)
                {
                    // run simulator
                    // function: runSim
                    runSim(configDataPtr, metaDataPtr);
                }
            }

            // otherwise, assume meta data upload failure
            else
            {
                // show error message, end program
                // function: printf
                printf("\nMetadata Upload Error: %s, program aborted\n",
                       errorMessage);
            }
        }
        // end check need for metadata upload

        // clean up config data as needed
        // function: clearConfigData
        configDataPtr = clearConfigData(configDataPtr);

        // clean up metadata as needed
        // function: clearMetaData
        metaDataPtr = clearMetaDataList(metaDataPtr);
    }
    // end check for good command line

    // otherwise, assume command line failure
    else
    {
        // show command argument requirements
        // function: showCommandLineFormat
        showCommandLineFormat();
    }

    // show program end
    // function: printf
    printf("\nSimulator Program End.\n");

    // return success
    return 0;
}

void clearCmdLineStruct(CmdLineData *clDataPtr)
{
    // set all struct members to default
    clDataPtr->programRunFlag = false;
    clDataPtr->configDisplayFlag = false;
    clDataPtr->mdDisplayFlag = false;
    clDataPtr->runSimFlag = false;
    clDataPtr->fileName[0] = NULL_CHAR;

    // void function, no return
}

bool processCmdLine(int numArgs, char **strVector, CmdLineData *clDataPtr)
{
    // initialize function/variables

    // initialize structure to defaults
    // function: clearStruct;
    clearCmdLineStruct(clDataPtr);

    // initialize success flags to false
    bool atLeastOneSwitchFlag = false;
    bool correctConfigFileFlag = false;

    // initialize first arg index to one
    int argIndex = 1;

    // declare other variables
    int fileStrLen, fileStrSubLoc;

    // must have prog name, at least one switch, and config file name, minimum
    if (numArgs >= MIN_NUM_ARGS)
    {
        // Loop across args (starting at 1) and program run flag
        while (argIndex < numArgs)
        {
            // check for -dc (display config flag)
            if (compareString(strVector[argIndex], "-dc") == STR_EQ)
            {
                // set config display flag
                clDataPtr->configDisplayFlag = true;

                // set at least one switch flag
                atLeastOneSwitchFlag = true;
            }

            // otherwise, check for -dm (display metadata flag)
            else if (compareString(strVector[argIndex], "-dm") == STR_EQ)
            {
                // set meta data display flag
                clDataPtr->mdDisplayFlag = true;

                // set at least one switch flag
                atLeastOneSwitchFlag = true;
            }

            // otherwise, check for -rs (run simulator)
            else if (compareString(strVector[argIndex], "-rs") == STR_EQ)
            {
                // set run simulator flag
                clDataPtr->runSimFlag = true;

                // set at least one switch flag
                atLeastOneSwitchFlag = true;
            }

            // otherwise, check for file name, ending in .cfg
            // must be last four

            // find lengths to verify file name
            // file name must be last argument, and have ".cnf" extension

            // verify string found and correct format for config file name
            else
            {
                // find lengths to verify file name
                // file name must be last argument, and have ".cnf" extension
                fileStrLen = getStringLength(strVector[numArgs - 1]);
                fileStrSubLoc = findSubString(strVector[numArgs - 1], ".cnf");

                // verify string found and correct format for config file name
                if (fileStrSubLoc != SUBSTRING_NOT_FOUND &&
                    fileStrSubLoc == fileStrLen - LAST_FOUR_LETTERS)
                {
                    // set file name to variable
                    copyString(clDataPtr->fileName, strVector[numArgs - 1]);

                    // set success flag to true
                    correctConfigFileFlag = true;
                }

                // otherwise, assume bad config file name
                else
                {
                    // reset struct, correct config file flag stays false
                    // function: clearStruct
                    clearCmdLineStruct(clDataPtr);
                }
            }

            // update arg index
            argIndex++;
        }
        // end arg loop
    }
    // end test for minimum number of command line arguments

    // return verification of at least one switch and correct file name
    return atLeastOneSwitchFlag && correctConfigFileFlag;
}

void showCommandLineFormat(void)
{
    // display command line format
    // function: printf
    printf("Command Line Format:\n");
    printf("    sim_0x [-dc] [-dm] [-rs] <config file name>\n");
    printf("    -dc [optional] displays configuration data\n");
    printf("    -dm [optional] displays meta data\n");
    printf("    -rs [optional] runs simulator\n");
    printf("    required config file name\n");
}
