#include <stdio.h>
#include "StringUtils.h"
#include "StandardConstants.h"

int compareString(const char *oneStr, const char *otherStr)
{
    // initialize function/variables
    int diff, index = 0;

    // loop to end of shortest string with protection
    while (oneStr[index] !=
               NULL_CHAR &&
           otherStr[index] !=
               NULL_CHAR &&
           index < MAX_STR_LEN)
    {
        // get difference in characters
        diff = oneStr[index] - otherStr[index];

        // check for difference between characters
        if (diff != 0)
        {
            // return difference
            return diff;
        }

        // increment index
        index++;
    }
    // end loop
    return getStringLength(oneStr) - getStringLength(otherStr);
}

void concatenateString(char *destStr, const char *sourceStr)
{
    // set destination index
    int destIndex = getStringLength(destStr);

    // get source string length
    int sourceStrLen = getStringLength(sourceStr);

    // create temporary string pointer
    char *tempStr;

    // create other variables
    int sourceIndex = 0;

    // copy source string in case of aliasing
    tempStr = (char *)malloc(sizeof(sourceStrLen + 1));

    // loop to end of source string
    while (tempStr[sourceIndex] != NULL_CHAR && destIndex < MAX_STR_LEN)
    {
        // assign characters to end of destination string
        destStr[destIndex] = tempStr[sourceIndex];

        // update indices
        destIndex++;
        sourceIndex++;

        // set temporary end of destination string
        destStr[destIndex] = NULL_CHAR;
    }

    free(tempStr);
}

void copyString(char *destStr, const char *sourceStr)
{
    // initialize function/variables
    int index = 0;

    // check for source/destination not the same (aliasing)
    if (destStr != sourceStr)
    {
        // loop to end of source string
        while (sourceStr[index] != NULL_CHAR && index < MAX_STR_LEN)
        {
            // assign characters to end of destination string
            destStr[index] = sourceStr[index];

            // update index
            index++;

            // set temporary end of destination string
            destStr[index] = NULL_CHAR;
        }
    }
    // end loop
}

int findSubString(const char *testStr, const char *searchSubStr)
{
    // initialize function/variables

    // initialize test string length
    // function: getStringLength
    int testStrLen = getStringLength(testStr);

    // initialize master index - location of sub string start point
    int masterIndex = 0;

    // initialize other variables
    int searchIndex, internalIndex;

    // loop across test string
    while (masterIndex <= testStrLen)
    {
        // set internal loop index to current test string index
        internalIndex = masterIndex;

        // set internal search index to zero
        searchIndex = 0;

        // loop to end of test string
        // while test string/sub string characters are the same
        while (internalIndex <= testStrLen &&
               testStr[internalIndex] == searchSubStr[searchIndex])
        {
            // increment test string, substring indices
            internalIndex++;
            searchIndex++;

            // check for end of substring (search completed)
            if (searchSubStr[searchIndex] == NULL_CHAR)
            {
                // return beginning location of sub string
                return masterIndex;
            }
        }
        // end internal comparison loop

        // increment current beginning location index
        masterIndex++;
    }
    // end loop across test string

    // assume test have failed at this point, return SUBSTRING_NOT_FOUND
    return SUBSTRING_NOT_FOUND;
}

bool getStringConstrained(
    FILE *inStream,
    bool clearLeadingNonPrintable,
    bool clearLeadingSpace,
    bool stopAtNonPrintable,
    char delimiter,
    char *capturedString)
{
    // initialize variables
    int intChar = EOF, index = 0;

    // initialize output string
    capturedString[index] = NULL_CHAR;

    // capture first value in stream
    // function: fgetc
    intChar = fgetc(inStream);

    // loop to clear non printable or space, if indicated
    while ((intChar != EOF) && ((clearLeadingNonPrintable && intChar < (int)SPACE) ||
                                (clearLeadingSpace && intChar == (int)SPACE)))
    {
        // get next character
        // function: fgetc
        intChar = fgetc(inStream);
    }
    // end clear non printable/space loop

    // check for end of file found
    if (intChar == EOF)
    {
        // return failed operation
        return false;
    }

    // loop to capture input
    while (
        // continues if not at end of file and max string length not reached
        (intChar != EOF && index < MAX_STR_LEN - 1)

        // AND
        // continues if not printable flag set and characters are printable
        // OR continues if not printable flag not set
        && ((stopAtNonPrintable && intChar >= (int)SPACE) || (!stopAtNonPrintable))

        // AND
        // continues if specified delimiter is not found
        && (intChar != (char)delimiter))
    {
        // place character in array element
        capturedString[index] = (char)intChar;

        // increment array index
        index++;

        // set next element to null character / end of c-string
        capturedString[index] = NULL_CHAR;

        // get next character as integer
        // function: fgetc
        intChar = fgetc(inStream);
    }
    // end loop

    // return successful operation
    return true;
}

int getStringLength(const char *testStr)
{
    // initialize function/variables
    int index = 0;

    // loop to end of string, protect from overflow
    while (index < STD_STR_LEN && testStr[index] != NULL_CHAR)
    {
        // update index
        index++;
    }
    // end loop

    // return index/length
    return index;
}

bool getStringToDelimiter(
    FILE *inStream,
    char delimiter,
    char *capturedString)
{
    // call engine function with delimiter
    // function: getStringConstrained
    return getStringConstrained(
        inStream,      // file stream pointer
        true,          // clears leading non printable character
        true,          // bool clearLeadingSpace,
        true,          // stops at non printable
        delimiter,     // stops at delimiter
        capturedString // returns string
    );
}

bool getStringToLineEnd(
    FILE *inStream,
    char *capturedString)
{
    // call engine function with delimiter
    // function: getStringConstrained
    return getStringConstrained(
        inStream,           // file stream pointer
        true,               // clears leading non printable character
        true,               // bool clearLeadingSpace,
        true,               // stops at non printable
        NON_PRINTABLE_CHAR, // non printable delimiter
        capturedString      // returns string
    );
}

void getSubstring(char *destStr, const char *sourceStr,
                  int startIndex, int endIndex)
{
    // initialize function/variables

    // set length of source string
    // function: getStringLength
    int sourceStrLen = getStringLength(sourceStr);

    // initialize the destination index to zero
    int destIndex = 0;

    // initialize source index to start index (parameter)
    int sourceIndex = startIndex;

    // create pointer for temp string
    char *tempStr;

    // check for indices within limits
    if (startIndex >= 0 && startIndex <= endIndex && endIndex < sourceStrLen)
    {
        // create temporary string
        // function: malloc, copyString
        tempStr = (char *)malloc(sourceStrLen + 1);
        copyString(tempStr, sourceStr);

        // loop across requested substring (indices)
        while (sourceIndex <= endIndex)
        {
            // assign source character to destination element
            destStr[destIndex] = tempStr[sourceIndex];

            // increment indices
            destIndex++;
            sourceIndex++;

            // set temporary end of destination string
            destStr[destIndex] = NULL_CHAR;
        }
        // end loop

        // return memory for temporary string
        // function: free
        free(tempStr);
    }
}

void setStrToLowerCase(char *destStr, const char *sourceStr)
{
    // initialize function/variables

    // get source string length
    // function: getStringLength
    int sourceStrLen = getStringLength(sourceStr);

    // create temporary string pointer
    char *tempStr;

    // create other variables
    int index = 0;

    // copy source string in case of aliasing
    // function: malloc, copyString
    tempStr = (char *)malloc(sourceStrLen + 1);
    copyString(tempStr, sourceStr);

    // loop across source string
    while (tempStr[index] != NULL_CHAR && index < MAX_STR_LEN)
    {
        // set individual character to lower case as needed,
        // assign to destination string
        destStr[index] = toLowerCase(tempStr[index]);

        // update index
        index++;

        // set temporary end of destination string
        destStr[index] = NULL_CHAR;
    }
    // end loop

    // release memory used for temp string
    // function: free
    free(tempStr);
}

char toLowerCase(char testChar)
{
    // check for upper case letter
    if (testChar >= 'A' && testChar <= 'Z')
    {
        // return lower case letter
        return testChar - 'A' + 'a';
    }

    // otherwise, assume no upper case letter,
    // return character unchanged
    return testChar;
}
