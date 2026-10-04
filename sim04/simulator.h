#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>

#include "datatypes.h"
#include "OS_SimDriver.h"
#include "StandardConstants.h"
#include "StringUtils.h"
#include "SimTimer.h"  // accessTimer, runTimer, timer control codes
#include "configops.h" // LOGTO_* codes

/*
Sim03 notes:
- FCFS-N and SJF-N
- Monitor output occurs in real time
- File output happens once at the end
- I/O operations must be timed using a POSIX thread
- Memory operations are handled in software only, no real memory allocation
*/

typedef struct PCB
{
    int pid;
    ProcessState state;

    // points into the global meta-data list
    // this is basically the program counter for a normal OS
    OpCodeType *currentOp;

    // total time remaining in ms for this process which is computed once at PCB build time
    int timeleft;

    // used by preemptive scheduling; FCFS/SRTF keep natural ordering, RR updates this
    int readyOrder;

    struct PCB *next;
} PCB;

typedef struct LogNode
{
    char message[STD_STR_LEN];
    struct LogNode *next;
} LogNode;

typedef struct LogData
{
    bool logToMonitor;
    bool logToFile;

    LogNode *head;
    LogNode *tail;
} LogData;

typedef struct MemoryNode
{
    int pid;
    int physBase;
    int base;
    int offset;
    struct MemoryNode *next;
} MemoryNode;

typedef struct MemoryData
{
    MemoryNode *head;
    int totalMemory;
} MemoryData;

// Required master simulator entry point
void runSim(ConfigDataType *configPtr, OpCodeType *metaDataMstPtr);

// PCB and scheduling
PCB *createPCBList(OpCodeType *mPtr, ConfigDataType *cPtr);
PCB *sched(PCB *headPtr, char tString[STD_STR_LEN], LogData *logData, int schedCode);
void allProcessStateReady(PCB *headPCB, char tString[STD_STR_LEN], LogData *logData);

// Logging
void logEvent(LogData *logData, const char *message);
void flushLogsToFile(LogData *logData, const char *fileName);

// Threaded I/O timing
void *simulateIO(void *cycleTime);
void runTimerThread(int simTimeMs);

// Memory management
bool accessMemory(MemoryData *memData, int pid, int base, int offset);
bool allocateMemory(MemoryData *memData, int pid, int base, int offset);
void clearMemoryData(MemoryData *memData);
void displayMemory(LogData *logData, MemoryData *memData, char tString[STD_STR_LEN]);
void displayMemoryStatus(LogData *logData, MemoryData *memData,
                         char tString[STD_STR_LEN], const char *statusMessage);

// Cleanup
void cleanup(PCB *head, LogData *logData);

#endif
