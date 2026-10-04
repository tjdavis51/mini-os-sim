#include "simulator.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct InterruptNode
{
    PCB *pcb;
    OpCodeType *op;
    char timeString[STD_STR_LEN];
    struct InterruptNode *next;
} InterruptNode;

typedef struct InterruptData
{
    pthread_mutex_t mutex;
    InterruptNode *head;
    InterruptNode *tail;
    int activeCount;
    int readyCounter;
} InterruptData;

typedef struct IOThreadData
{
    int simTimeMs;
    PCB *pcb;
    OpCodeType *op;
    InterruptData *interruptData;
    InterruptNode *interruptNode;
} IOThreadData;

/*
Name: allProcessStateReady
Process: goes through and changes each PCB state to READY
Function I/Parameters: pointer to head of PCB list, time string, logData pointer
Function O/Returned: none
Dependencies: accessTimer, snprintf, logEvent
*/
void allProcessStateReady(PCB *headPCB, char tString[STD_STR_LEN], LogData *logData)
{
    // initialization
    PCB *pcbCursor;
    char message[STD_STR_LEN];
    pcbCursor = headPCB;

    // get one time stamp for all NEW -> READY transitions
    accessTimer(LAP_TIMER, tString);

    // loop through the PCBs
    while (pcbCursor != NULL)
    {
        // set to ready
        pcbCursor->state = READY_STATE;
        pcbCursor->readyOrder = pcbCursor->pid;

        // log the event
        snprintf(message, STD_STR_LEN,
                 " %s, OS: Process %d set to READY state from NEW state",
                 tString, pcbCursor->pid);

        logEvent(logData, message);

        // move to the next PCB
        pcbCursor = pcbCursor->next;
    }
}

/*
Name: cleanup
Process: frees allocated memory used for linked lists (PCB & log data nodes)
Function I/Parameters: head of PCB list, pointer to LogData
Dependencies: free
*/
void cleanup(PCB *head, LogData *logData)
{
    // initialize
    PCB *pcbCurrent = head;
    PCB *pcbTemp = NULL;
    LogNode *logCurrent = NULL;

    // set logCurrent if logData is not null
    if (logData != NULL)
    {
        logCurrent = logData->head;
    }

    LogNode *logTemp = NULL;

    // free PCBs
    while (pcbCurrent != NULL)
    {
        pcbTemp = pcbCurrent;
        pcbCurrent = pcbCurrent->next;
        free(pcbTemp);
    }

    // free log nodes
    while (logCurrent != NULL)
    {
        logTemp = logCurrent;
        logCurrent = logCurrent->next;
        free(logTemp);
    }

    // reset logData pointers if logData is not null
    if (logData != NULL)
    {
        logData->head = NULL;
        logData->tail = NULL;
    }
}

/*
Helper: isAppEndOp
Process: checks if the given opcode is an "app end" opcode
*/
static bool isAppEndOp(const OpCodeType *op)
{
    return op != NULL &&
           compareString(op->command, "app") == STR_EQ &&
           compareString(op->strArg1, "end") == STR_EQ;
}

/*
Helper: isAppStartOp
Process: checks if the given opcode is an "app start" opcode
*/
static bool isAppStartOp(const OpCodeType *op)
{
    return op != NULL &&
           compareString(op->command, "app") == STR_EQ &&
           compareString(op->strArg1, "start") == STR_EQ;
}

/*
Helper: clearMemoryForProcess
Process: removes all memory nodes owned by a given process
*/
static void clearMemoryForProcess(MemoryData *memData, int pid)
{
    MemoryNode *current;
    MemoryNode *previous;
    MemoryNode *tempNode;

    if (memData == NULL)
    {
        return;
    }

    current = memData->head;
    previous = NULL;

    while (current != NULL)
    {
        if (current->pid == pid)
        {
            tempNode = current;

            if (previous == NULL)
            {
                memData->head = current->next;
            }
            else
            {
                previous->next = current->next;
            }

            current = current->next;
            free(tempNode);
        }
        else
        {
            previous = current;
            current = current->next;
        }
    }
}

/*
Name: createPCBList
Process: creates a linked list of PCBs from meta-data
Function I/Parameters: pointer to meta-data head, pointer to config
Function O/Returned: head of PCB list
Dependencies: compareString, malloc
*/
PCB *createPCBList(OpCodeType *mPtr, ConfigDataType *cPtr)
{
    // initialization
    PCB *head = NULL;
    OpCodeType *currentOp = mPtr;
    int pid = 0;

    // check for null pointers
    if (mPtr == NULL || cPtr == NULL)
    {
        return NULL;
    }

    // loop through meta-data until we find app starts, then build PCBs from those
    while (currentOp != NULL)
    {
        // find "app start"
        if (isAppStartOp(currentOp))
        {
            PCB *newPCB = (PCB *)malloc(sizeof(PCB));
            if (newPCB == NULL)
            {
                // out of memory so return what we have
                return head;
            }

            newPCB->pid = pid;
            newPCB->state = NEW_STATE;

            // start executing after app start
            newPCB->currentOp = currentOp->nextNode;

            newPCB->timeleft = 0;
            newPCB->readyOrder = pid;
            newPCB->next = NULL;

            // append to PCB list
            if (head == NULL)
            {
                head = newPCB;
            }
            else
            {
                PCB *tail = head;
                while (tail->next != NULL)
                {
                    tail = tail->next;
                }
                tail->next = newPCB;
            }

            // compute timeleft by scanning until matching app end
            OpCodeType *scan = newPCB->currentOp;
            while (scan != NULL && !isAppEndOp(scan))
            {
                // only CPU and DEV ops contribute to timeleft in this sim
                if (compareString(scan->command, "dev") == STR_EQ)
                {
                    newPCB->timeleft += scan->intArg2 * cPtr->ioCycleRate;
                }
                else if (compareString(scan->command, "cpu") == STR_EQ)
                {
                    newPCB->timeleft += scan->intArg2 * cPtr->procCycleRate;
                }

                // advance scan
                scan = scan->nextNode;
            }

            pid++;

            // advance currentOp until after this app end until the next app start is found
            while (currentOp != NULL && !isAppEndOp(currentOp))
            {
                currentOp = currentOp->nextNode;
            }
            if (currentOp != NULL)
            {
                currentOp = currentOp->nextNode;
            }
        }
        else
        {
            currentOp = currentOp->nextNode;
        }
    }

    return head;
}

/*
Name: simulateIO
Process: thread entry for simulating IO operation
Dependencies: runTimer
*/
void *simulateIO(void *cycleTime)
{
    // run the timer for the given time to simulate the IO operation
    int *cTime = (int *)cycleTime;
    if (cTime != NULL)
    {
        runTimer(*cTime);
    }
    return NULL;
}

/*
Name: runTimerThread
Process: uses a POSIX thread to simulate a device operation time
Dependencies: pthread_create, pthread_join
*/
void runTimerThread(int simTimeMs)
{
    // make a thread
    pthread_t thread;
    int localTime = simTimeMs;

    // create the thread to run the timer
    int createRC = pthread_create(&thread, NULL, simulateIO, &localTime);
    if (createRC != 0)
    {
        // if thread creation fails run synchronously
        runTimer(localTime);
        return;
    }

    // wait for the thread to finish before returning
    pthread_join(thread, NULL);
}

/*
Helper: appendInterrupt
Process: adds a completed I/O operation to the interrupt queue
*/
static void appendInterrupt(InterruptData *interruptData, InterruptNode *node)
{
    if (interruptData == NULL || node == NULL)
    {
        return;
    }

    node->next = NULL;

    pthread_mutex_lock(&interruptData->mutex);

    if (interruptData->head == NULL)
    {
        interruptData->head = node;
        interruptData->tail = node;
    }
    else
    {
        interruptData->tail->next = node;
        interruptData->tail = node;
    }

    interruptData->activeCount--;

    pthread_mutex_unlock(&interruptData->mutex);
}

/*
Name: simulateAsyncIO
Process: threaded I/O timer for preemptive scheduling
*/
static void *simulateAsyncIO(void *arg)
{
    IOThreadData *threadData = (IOThreadData *)arg;

    if (threadData != NULL)
    {
        /*
        This detached thread only simulates elapsed device time.  When the
        timer completes, it records a small interrupt request under mutex
        control; the simulator loop owns all logging and PCB state changes.
        */
        runTimer(threadData->simTimeMs);

        appendInterrupt(threadData->interruptData, threadData->interruptNode);

        free(threadData);
    }

    return NULL;
}

/*
Helper: startAsyncIO
Process: starts a detached I/O timer thread and records active I/O count
*/
static bool startAsyncIO(PCB *pcb, InterruptData *interruptData, int simTimeMs)
{
    pthread_t thread;
    IOThreadData *threadData;
    InterruptNode *interruptNode;

    if (pcb == NULL || interruptData == NULL || pcb->currentOp == NULL)
    {
        return false;
    }

    threadData = (IOThreadData *)malloc(sizeof(IOThreadData));
    interruptNode = (InterruptNode *)malloc(sizeof(InterruptNode));

    if (threadData == NULL || interruptNode == NULL)
    {
        free(threadData);
        free(interruptNode);
        return false;
    }

    interruptNode->pcb = pcb;
    interruptNode->op = pcb->currentOp;
    interruptNode->timeString[0] = NULL_CHAR;
    interruptNode->next = NULL;

    threadData->simTimeMs = simTimeMs;
    threadData->pcb = pcb;
    threadData->op = pcb->currentOp;
    threadData->interruptData = interruptData;
    threadData->interruptNode = interruptNode;

    /*
    Preemptive I/O is launched as a detached POSIX thread.  There is no join;
    completion is reported back through the interrupt queue so CPU processing
    can continue while the device timer runs.
    */
    pthread_mutex_lock(&interruptData->mutex);
    interruptData->activeCount++;
    pthread_mutex_unlock(&interruptData->mutex);

    if (pthread_create(&thread, NULL, simulateAsyncIO, threadData) != 0)
    {
        runTimer(simTimeMs);
        appendInterrupt(interruptData, interruptNode);
        free(threadData);
        return true;
    }

    pthread_detach(thread);

    return true;
}

/*
Helper: popInterrupt
Process: removes one interrupt request from the queue
*/
static InterruptNode *popInterrupt(InterruptData *interruptData)
{
    InterruptNode *node;

    if (interruptData == NULL)
    {
        return NULL;
    }

    pthread_mutex_lock(&interruptData->mutex);

    node = interruptData->head;

    if (node != NULL)
    {
        interruptData->head = node->next;

        if (interruptData->head == NULL)
        {
            interruptData->tail = NULL;
        }
    }

    pthread_mutex_unlock(&interruptData->mutex);

    return node;
}

/*
Helper: hasPendingInterrupt
Process: checks for any completed I/O request
*/
static bool hasPendingInterrupt(InterruptData *interruptData)
{
    bool result;

    if (interruptData == NULL)
    {
        return false;
    }

    pthread_mutex_lock(&interruptData->mutex);
    result = interruptData->head != NULL;
    pthread_mutex_unlock(&interruptData->mutex);

    return result;
}

/*
Helper: hasActiveIO
Process: checks if any I/O timers are still running
*/
static bool hasActiveIO(InterruptData *interruptData)
{
    bool result;

    if (interruptData == NULL)
    {
        return false;
    }

    pthread_mutex_lock(&interruptData->mutex);
    result = interruptData->activeCount > 0;
    pthread_mutex_unlock(&interruptData->mutex);

    return result;
}

/*
Name: logEvent
Process: logs to monitor (if enabled) and stores the line for file flush
Dependencies: printf, malloc, snprintf
*/
void logEvent(LogData *logData, const char *message)
{
    // check for null pointers
    if (logData == NULL || message == NULL)
    {
        return;
    }

    // print to monitor in real time if enabled
    if (logData->logToMonitor)
    {
        printf("%s\n", message);
    }

    // store line for later file flush if file logging is enabled
    if (!logData->logToFile)
    {
        return;
    }

    // create new log node
    LogNode *newNode = (LogNode *)malloc(sizeof(LogNode));
    if (newNode == NULL)
    {
        return;
    }

    // copy message into node and append to log list
    snprintf(newNode->message, STD_STR_LEN, "%s", message);
    newNode->next = NULL;

    // append to log list
    if (logData->head == NULL)
    {
        logData->head = newNode;
        logData->tail = newNode;
    }
    // append to end
    else
    {
        logData->tail->next = newNode;
        logData->tail = newNode;
    }
}

/*
Name: flushLogsToFile
Process: writes all stored log lines to the configured log file at the end
Dependencies: fopen, fprintf, fclose
*/
void flushLogsToFile(LogData *logData, const char *fileName)
{
    // check for null pointers and file logging enabled
    if (logData == NULL || !logData->logToFile || fileName == NULL)
    {
        return;
    }

    // open the file for writing
    FILE *fp = fopen(fileName, "w");
    if (fp == NULL)
    {
        // cannot open log file so nothing else to do here
        return;
    }

    // write each log line to the file
    LogNode *current = logData->head;
    while (current != NULL)
    {
        fprintf(fp, "%s\n", current->message);
        current = current->next;
    }

    fclose(fp);
}

/*
Name: allocateMemory
Process: attempts to allocate a software memory block for a process
Function O/Returned: true if successful, false if failed
*/
bool allocateMemory(MemoryData *memData, int pid, int base, int offset)
{
    MemoryNode *current;
    MemoryNode *previous;
    int requestEnd;
    int candidateBase;
    int currentEnd;

    if (memData == NULL)
    {
        return false;
    }

    if (base < 0 || offset <= 0)
    {
        return false;
    }

    requestEnd = base + offset - 1;

    if (requestEnd < base)
    {
        return false;
    }

    candidateBase = 0;
    current = memData->head;
    previous = NULL;

    while (current != NULL)
    {
        // Logical memory ranges may repeat across processes, but not within one process.
        currentEnd = current->base + current->offset - 1;

        if (current->pid == pid &&
            !(requestEnd < current->base || base > currentEnd))
        {
            return false;
        }

        if (candidateBase + offset <= current->physBase)
        {
            break;
        }

        candidateBase = current->physBase + current->offset;
        previous = current;
        current = current->next;
    }

    if (candidateBase + offset > memData->totalMemory)
    {
        return false;
    }

    MemoryNode *newNode = (MemoryNode *)malloc(sizeof(MemoryNode));
    if (newNode == NULL)
    {
        return false;
    }

    newNode->pid = pid;
    newNode->physBase = candidateBase;
    newNode->base = base;
    newNode->offset = offset;
    newNode->next = current;

    if (previous == NULL)
    {
        memData->head = newNode;
    }
    else
    {
        previous->next = newNode;
    }

    return true;
}

/*
Name: accessMemory
Process: verifies that an access request is valid for a process
Function O/Returned: true if successful, false if failed
*/
bool accessMemory(MemoryData *memData, int pid, int base, int offset)
{
    MemoryNode *current;
    int requestEnd;

    if (memData == NULL)
    {
        return false;
    }

    if (base < 0 || offset <= 0)
    {
        return false;
    }

    requestEnd = base + offset - 1;

    current = memData->head;
    while (current != NULL)
    {
        int currentEnd = current->base + current->offset - 1;

        if (current->pid == pid &&
            base >= current->base &&
            requestEnd <= currentEnd)
        {
            return true;
        }

        current = current->next;
    }

    return false;
}

/*
Helper: memoryRequestOverlapsProcess
Process: identifies duplicate logical memory use inside one process
*/
static bool memoryRequestOverlapsProcess(MemoryData *memData, int pid,
                                         int base, int offset)
{
    MemoryNode *current;
    int requestEnd;
    int currentEnd;

    if (memData == NULL || offset <= 0)
    {
        return false;
    }

    requestEnd = base + offset - 1;
    current = memData->head;

    while (current != NULL)
    {
        currentEnd = current->base + current->offset - 1;

        if (current->pid == pid &&
            !(requestEnd < current->base || base > currentEnd))
        {
            return true;
        }

        current = current->next;
    }

    return false;
}

/*
Name: clearMemoryData
Process: frees all configured memory nodes
*/
void clearMemoryData(MemoryData *memData)
{
    MemoryNode *current;
    MemoryNode *tempNode;

    if (memData == NULL)
    {
        return;
    }

    current = memData->head;
    while (current != NULL)
    {
        tempNode = current;
        current = current->next;
        free(tempNode);
    }

    memData->head = NULL;
}

/*
Name: displayMemory
Process: logs a simple software memory display
Dependencies: snprintf, logEvent
*/
void displayMemory(LogData *logData, MemoryData *memData, char tString[STD_STR_LEN])
{
    MemoryNode *current;
    char message[MAX_STR_LEN];
    int displayBase = 0;

    // tString not needed directly but preserved for header consistency
    (void)tString;

    if (logData == NULL || memData == NULL)
    {
        return;
    }

    if (memData->head == NULL)
    {
        snprintf(message, MAX_STR_LEN, "0 [ Open, P#: x, 0-0 ] %d",
                 memData->totalMemory - 1);
        logEvent(logData, message);
        logEvent(logData, "--------------------------------------------------");
        return;
    }

    current = memData->head;
    while (current != NULL)
    {
        if (displayBase < current->physBase)
        {
            snprintf(message, MAX_STR_LEN, "%d [ Open, P#: x, 0-0 ] %d",
                     displayBase, current->physBase - 1);
            logEvent(logData, message);
        }

        snprintf(message, MAX_STR_LEN,
                 "%d [ Used, P#: %d, %d-%d ] %d",
                 current->physBase,
                 current->pid,
                 current->base,
                 current->base + current->offset - 1,
                 current->physBase + current->offset - 1);
        logEvent(logData, message);

        displayBase = current->physBase + current->offset;
        current = current->next;
    }

    if (displayBase < memData->totalMemory)
    {
        snprintf(message, MAX_STR_LEN, "%d [ Open, P#: x, 0-0 ] %d",
                 displayBase, memData->totalMemory - 1);
        logEvent(logData, message);
    }

    logEvent(logData, "--------------------------------------------------");
}

/*
Name: displayMemoryStatus
Process: shows memory state with sample-required separator/header order
Dependencies: logEvent, displayMemory
*/
void displayMemoryStatus(LogData *logData, MemoryData *memData,
                         char tString[STD_STR_LEN], const char *statusMessage)
{
    if (logData == NULL || memData == NULL || statusMessage == NULL)
    {
        return;
    }

    logEvent(logData, "--------------------------------------------------");
    logEvent(logData, statusMessage);
    displayMemory(logData, memData, tString);
}

/*
Name: sched
Process: selects next READY PCB using FCFS-N or SJF-N
Shows time remaining and READY->RUNNING transition
Dependencies: accessTimer, snprintf, logEvent
*/
PCB *sched(PCB *headPtr, char tString[STD_STR_LEN], LogData *logData, int schedCode)
{
    PCB *current;
    PCB *selected;
    char message[STD_STR_LEN];

    selected = NULL;
    current = headPtr;

    // FCFS-N/FCFS-P by list order
    if (schedCode == CPU_SCHED_FCFS_N_CODE ||
        schedCode == CPU_SCHED_FCFS_P_CODE)
    {
        while (current != NULL && current->state != READY_STATE)
        {
            current = current->next;
        }

        selected = current;
    }

    // SJF-N/SRTF-P by smallest timeleft, ties resolved by FCFS/list order
    else if (schedCode == CPU_SCHED_SJF_N_CODE ||
             schedCode == CPU_SCHED_SRTF_P_CODE)
    {
        while (current != NULL)
        {
            if (current->state == READY_STATE)
            {
                if (selected == NULL || current->timeleft < selected->timeleft)
                {
                    selected = current;
                }
            }

            current = current->next;
        }
    }

    // RR-P by ready queue return order
    else if (schedCode == CPU_SCHED_RR_P_CODE)
    {
        while (current != NULL)
        {
            if (current->state == READY_STATE)
            {
                if (selected == NULL || current->readyOrder < selected->readyOrder)
                {
                    selected = current;
                }
            }

            current = current->next;
        }
    }

    // fallback to FCFS-N if anything else is configured
    else
    {
        current = headPtr;
        while (current != NULL && current->state != READY_STATE)
        {
            current = current->next;
        }

        selected = current;
    }

    if (selected != NULL)
    {
        // show time remaining and transition to running
        accessTimer(LAP_TIMER, tString);
        snprintf(message, STD_STR_LEN,
                 " %s, OS: Process %d selected with %d ms remaining",
                 tString, selected->pid, selected->timeleft);
        logEvent(logData, message);

        // state transition to RUNNING
        selected->state = RUNNING_STATE;

        snprintf(message, STD_STR_LEN,
                 " %s, OS: Process %d set from READY to RUNNING",
                 tString, selected->pid);
        logEvent(logData, message);
    }

    return selected;
}

/*
Helper: findScheduleCandidate
Process: finds the next READY PCB without logging or changing state
*/
static PCB *findScheduleCandidate(PCB *headPtr, int schedCode)
{
    PCB *current = headPtr;
    PCB *selected = NULL;

    if (schedCode == CPU_SCHED_FCFS_N_CODE ||
        schedCode == CPU_SCHED_FCFS_P_CODE)
    {
        while (current != NULL && current->state != READY_STATE)
        {
            current = current->next;
        }

        return current;
    }

    if (schedCode == CPU_SCHED_SJF_N_CODE ||
        schedCode == CPU_SCHED_SRTF_P_CODE)
    {
        while (current != NULL)
        {
            if (current->state == READY_STATE &&
                (selected == NULL || current->timeleft < selected->timeleft))
            {
                selected = current;
            }

            current = current->next;
        }

        return selected;
    }

    if (schedCode == CPU_SCHED_RR_P_CODE)
    {
        while (current != NULL)
        {
            if (current->state == READY_STATE &&
                (selected == NULL || current->readyOrder < selected->readyOrder))
            {
                selected = current;
            }

            current = current->next;
        }
    }

    return selected;
}

/*
Helper: allProcessesExited
Process: reports whether every PCB has reached EXIT
*/
static bool allProcessesExited(PCB *headPtr)
{
    while (headPtr != NULL)
    {
        if (headPtr->state != EXIT_STATE)
        {
            return false;
        }

        headPtr = headPtr->next;
    }

    return true;
}

/*
Helper: setProcessReadyOrder
Process: applies FCFS-P/RR-P ready placement policy
*/
static void setProcessReadyOrder(PCB *pcb, InterruptData *interruptData, int schedCode)
{
    if (pcb == NULL)
    {
        return;
    }

    if (schedCode == CPU_SCHED_RR_P_CODE && interruptData != NULL)
    {
        interruptData->readyCounter++;
        pcb->readyOrder = interruptData->readyCounter;
    }
    else
    {
        pcb->readyOrder = pcb->pid;
    }
}

/*
Helper: processInterrupts
Process: services all queued I/O completion interrupts
*/
static int processInterrupts(InterruptData *interruptData, LogData *logData,
                             int schedCode, char tString[STD_STR_LEN])
{
    InterruptNode *node;
    char message[MAX_STR_LEN];
    int interruptCount = 0;

    node = popInterrupt(interruptData);

    while (node != NULL)
    {
        accessTimer(LAP_TIMER, tString);

        snprintf(message, MAX_STR_LEN,
                 " %s, OS: Interrupted by process %d, %s %sput operation",
                 tString, node->pcb->pid,
                 node->op->strArg1, node->op->inOutArg);
        logEvent(logData, message);

        if (node->pcb->state == BLOCKED_STATE)
        {
            node->pcb->state = READY_STATE;
            node->pcb->currentOp = node->op->nextNode;
            setProcessReadyOrder(node->pcb, interruptData, schedCode);

            snprintf(message, MAX_STR_LEN,
                     " %s, OS: Process %d set from BLOCKED to READY",
                     tString, node->pcb->pid);
            logEvent(logData, message);
        }

        free(node);
        interruptCount++;
        node = popInterrupt(interruptData);
    }

    accessTimer(LAP_TIMER, tString);

    return interruptCount;
}

/*
Helper: setRunningToReady
Process: returns a preempted running process to READY
*/
static void setRunningToReady(PCB *pcb, InterruptData *interruptData, int schedCode)
{
    if (pcb != NULL && pcb->state == RUNNING_STATE)
    {
        pcb->state = READY_STATE;
        setProcessReadyOrder(pcb, interruptData, schedCode);
    }
}

/*
Helper: processEnd
Process: handles normal or faulted process completion for preemptive path
*/
static void processEnd(PCB *pcb, MemoryData *memData, ConfigDataType *configPtr,
                       LogData *logData, char tString[STD_STR_LEN],
                       bool segFaultFlag)
{
    char message[MAX_STR_LEN];

    if (pcb == NULL)
    {
        return;
    }

    logEvent(logData, "");

    accessTimer(LAP_TIMER, tString);

    if (segFaultFlag)
    {
        snprintf(message, MAX_STR_LEN,
                 " %s, OS: Segmentation fault, Process %d ended",
                 tString, pcb->pid);
    }
    else
    {
        snprintf(message, MAX_STR_LEN,
                 " %s, OS: Process %d ended",
                 tString, pcb->pid);
    }

    logEvent(logData, message);

    clearMemoryForProcess(memData, pcb->pid);

    if (configPtr->memDisplay)
    {
        snprintf(message, MAX_STR_LEN, "After clear process %d success", pcb->pid);
        displayMemoryStatus(logData, memData, tString, message);
    }

    pcb->state = EXIT_STATE;

    accessTimer(LAP_TIMER, tString);
    snprintf(message, MAX_STR_LEN,
             " %s, OS: Process %d set to EXIT", tString, pcb->pid);
    logEvent(logData, message);
}

/*
Helper: runPreemptiveSim
Process: Phase 4 multiprogramming simulation for FCFS-P, SRTF-P, and RR-P
*/
static void runPreemptiveSim(ConfigDataType *configPtr, OpCodeType *metaDataMstPtr)
{
    char tString[STD_STR_LEN];
    char message[MAX_STR_LEN];
    int opTime;
    int cyclesUsed;
    int quantumCycles;
    bool segFaultFlag;
    bool opComplete;
    bool interruptBreak;
    PCB *pcbHead;
    PCB *currentPCB;
    LogData logData;
    MemoryData memData;
    InterruptData interruptData;

    accessTimer(ZERO_TIMER, tString);

    logData.head = NULL;
    logData.tail = NULL;
    logData.logToMonitor =
        (configPtr->logToCode == LOGTO_MONITOR_CODE ||
         configPtr->logToCode == LOGTO_BOTH_CODE);
    logData.logToFile =
        (configPtr->logToCode == LOGTO_FILE_CODE ||
         configPtr->logToCode == LOGTO_BOTH_CODE);

    memData.head = NULL;
    memData.totalMemory = configPtr->memAvailable;

    interruptData.head = NULL;
    interruptData.tail = NULL;
    interruptData.activeCount = 0;
    interruptData.readyCounter = 0;
    pthread_mutex_init(&interruptData.mutex, NULL);

    if (configPtr->logToCode == LOGTO_FILE_CODE)
    {
        printf("Simulator running for output to file only\n");
    }

    logEvent(&logData, "Simulator Run");
    logEvent(&logData, "-------------");
    logEvent(&logData, "");

    snprintf(message, MAX_STR_LEN, " %s, OS: Simulator start", tString);
    logEvent(&logData, message);

    pcbHead = createPCBList(metaDataMstPtr, configPtr);
    allProcessStateReady(pcbHead, tString, &logData);

    if (configPtr->memDisplay)
    {
        displayMemoryStatus(&logData, &memData, tString,
                            "After memory initialization");
    }

    currentPCB = NULL;
    quantumCycles = configPtr->quantumCycles;

    while (!allProcessesExited(pcbHead))
    {
        processInterrupts(&interruptData, &logData,
                          configPtr->cpuSchedCode, tString);

        if (currentPCB == NULL || currentPCB->state != RUNNING_STATE)
        {
            currentPCB = sched(pcbHead, tString, &logData,
                               configPtr->cpuSchedCode);

            if (currentPCB != NULL)
            {
                logEvent(&logData, "");
            }
        }

        if (currentPCB == NULL)
        {
            if (hasActiveIO(&interruptData))
            {
                accessTimer(LAP_TIMER, tString);
                snprintf(message, MAX_STR_LEN,
                         " %s, OS: CPU idle, all active processes blocked",
                         tString);
                logEvent(&logData, message);

                while (!hasPendingInterrupt(&interruptData))
                {
                    runTimer(1);
                }

                accessTimer(LAP_TIMER, tString);
                snprintf(message, MAX_STR_LEN,
                         " %s, OS: CPU interrupt, end idle", tString);
                logEvent(&logData, message);
            }
            else
            {
                break;
            }

            continue;
        }

        segFaultFlag = false;

        if (currentPCB->currentOp == NULL || isAppEndOp(currentPCB->currentOp))
        {
            processEnd(currentPCB, &memData, configPtr, &logData,
                       tString, false);
            currentPCB = NULL;
        }
        else if (compareString(currentPCB->currentOp->command, "cpu") == STR_EQ)
        {
            accessTimer(LAP_TIMER, tString);
            snprintf(message, MAX_STR_LEN,
                     " %s, Process: %d, cpu process operation start",
                     tString, currentPCB->pid);
            logEvent(&logData, message);

            cyclesUsed = 0;
            opComplete = false;
            interruptBreak = false;

            while (!opComplete && cyclesUsed < quantumCycles)
            {
                runTimer(configPtr->procCycleRate);
                currentPCB->currentOp->intArg2--;
                cyclesUsed++;

                if (currentPCB->timeleft >= configPtr->procCycleRate)
                {
                    currentPCB->timeleft -= configPtr->procCycleRate;
                }
                else
                {
                    currentPCB->timeleft = 0;
                }

                opComplete = currentPCB->currentOp->intArg2 <= 0;

                if (!opComplete && hasPendingInterrupt(&interruptData))
                {
                    interruptBreak = true;
                    break;
                }
            }

            accessTimer(LAP_TIMER, tString);

            if (opComplete)
            {
                snprintf(message, MAX_STR_LEN,
                         " %s, Process: %d, cpu process operation end",
                         tString, currentPCB->pid);
                logEvent(&logData, message);

                currentPCB->currentOp = currentPCB->currentOp->nextNode;

                processInterrupts(&interruptData, &logData,
                                  configPtr->cpuSchedCode, tString);

                if (configPtr->cpuSchedCode == CPU_SCHED_RR_P_CODE)
                {
                    /*
                    RR-P only gives up the CPU on quantum timeout, blocking,
                    exit, or fault.  A completed CPU opcode can continue into
                    the next opcode without being requeued.
                    */
                }
                else
                {
                    setRunningToReady(currentPCB, &interruptData,
                                      configPtr->cpuSchedCode);

                    if (findScheduleCandidate(pcbHead, configPtr->cpuSchedCode) ==
                        currentPCB)
                    {
                        currentPCB->state = RUNNING_STATE;
                    }
                    else
                    {
                        currentPCB = NULL;
                    }
                }
            }
            else if (interruptBreak)
            {
                snprintf(message, MAX_STR_LEN,
                         " %s, Process: %d, cpu process operation end",
                         tString, currentPCB->pid);
                logEvent(&logData, message);

                setRunningToReady(currentPCB, &interruptData,
                                  configPtr->cpuSchedCode);

                processInterrupts(&interruptData, &logData,
                                  configPtr->cpuSchedCode, tString);

                if (findScheduleCandidate(pcbHead, configPtr->cpuSchedCode) ==
                    currentPCB)
                {
                    currentPCB->state = RUNNING_STATE;
                }
                else
                {
                    currentPCB = NULL;
                }
            }
            else
            {
                logEvent(&logData, "");
                snprintf(message, MAX_STR_LEN,
                         " %s, OS: Process %d quantum time out, cpu process operation end",
                         tString, currentPCB->pid);
                logEvent(&logData, message);

                setRunningToReady(currentPCB, &interruptData, configPtr->cpuSchedCode);

                processInterrupts(&interruptData, &logData,
                                  configPtr->cpuSchedCode, tString);

                if (findScheduleCandidate(pcbHead, configPtr->cpuSchedCode) == currentPCB)
                {
                    currentPCB->state = RUNNING_STATE;
                }
                else
                {
                    currentPCB = NULL;
                }
            }
        }
        else if (compareString(currentPCB->currentOp->command, "dev") == STR_EQ)
        {
            opTime = currentPCB->currentOp->intArg2 * configPtr->ioCycleRate;

            accessTimer(LAP_TIMER, tString);
            snprintf(message, MAX_STR_LEN,
                     " %s, Process: %d, %s %sput operation start",
                     tString, currentPCB->pid,
                     currentPCB->currentOp->strArg1,
                     currentPCB->currentOp->inOutArg);
            logEvent(&logData, message);

            logEvent(&logData, "");

            snprintf(message, MAX_STR_LEN,
                     " %s, OS: Process %d blocked for %sput operation",
                     tString, currentPCB->pid,
                     currentPCB->currentOp->inOutArg);
            logEvent(&logData, message);

            currentPCB->state = BLOCKED_STATE;

            snprintf(message, MAX_STR_LEN,
                     " %s, OS: Process %d set from RUNNING to BLOCKED",
                     tString, currentPCB->pid);
            logEvent(&logData, message);

            if (currentPCB->timeleft >= opTime)
            {
                currentPCB->timeleft -= opTime;
            }
            else
            {
                currentPCB->timeleft = 0;
            }

            startAsyncIO(currentPCB, &interruptData, opTime);
            currentPCB = NULL;
        }
        else if (compareString(currentPCB->currentOp->command, "mem") == STR_EQ)
        {
            accessTimer(LAP_TIMER, tString);
            snprintf(message, MAX_STR_LEN,
                     " %s, Process: %d, mem %s request (%d, %d)",
                     tString,
                     currentPCB->pid,
                     currentPCB->currentOp->strArg1,
                     currentPCB->currentOp->intArg2,
                     currentPCB->currentOp->intArg3);
            logEvent(&logData, message);

            if (compareString(currentPCB->currentOp->strArg1, "allocate") == STR_EQ)
            {
                bool overlapFailure = memoryRequestOverlapsProcess(&memData,
                                                                   currentPCB->pid,
                                                                   currentPCB->currentOp->intArg2,
                                                                   currentPCB->currentOp->intArg3);
                bool allocSuccess = allocateMemory(&memData,
                                                   currentPCB->pid,
                                                   currentPCB->currentOp->intArg2,
                                                   currentPCB->currentOp->intArg3);

                if (configPtr->memDisplay)
                {
                    if (allocSuccess)
                    {
                        displayMemoryStatus(&logData, &memData, tString,
                                            "After allocate success");
                    }
                    else if (overlapFailure)
                    {
                        displayMemoryStatus(&logData, &memData, tString,
                                            "After allocate overlap failure");
                    }
                    else
                    {
                        displayMemoryStatus(&logData, &memData, tString,
                                            "After allocate failure");
                    }
                }

                accessTimer(LAP_TIMER, tString);
                snprintf(message, MAX_STR_LEN,
                         " %s, Process: %d, %s mem allocate request",
                         tString, currentPCB->pid,
                         allocSuccess ? "successful" : "failed");
                logEvent(&logData, message);

                segFaultFlag = !allocSuccess;
            }
            else if (compareString(currentPCB->currentOp->strArg1, "access") == STR_EQ)
            {
                bool accessSuccess = accessMemory(&memData,
                                                  currentPCB->pid,
                                                  currentPCB->currentOp->intArg2,
                                                  currentPCB->currentOp->intArg3);

                if (configPtr->memDisplay)
                {
                    displayMemoryStatus(&logData, &memData, tString,
                                        accessSuccess ? "After access success"
                                                      : "After access failure");
                }

                accessTimer(LAP_TIMER, tString);
                snprintf(message, MAX_STR_LEN,
                         " %s, Process: %d, %s mem access request",
                         tString, currentPCB->pid,
                         accessSuccess ? "successful" : "failed");
                logEvent(&logData, message);

                segFaultFlag = !accessSuccess;
            }

            if (segFaultFlag)
            {
                processEnd(currentPCB, &memData, configPtr, &logData,
                           tString, true);
                currentPCB = NULL;
            }
            else
            {
                currentPCB->currentOp = currentPCB->currentOp->nextNode;
            }
        }
        else
        {
            currentPCB->currentOp = currentPCB->currentOp->nextNode;
        }
    }

    accessTimer(LAP_TIMER, tString);
    snprintf(message, MAX_STR_LEN, " %s, OS: System stop", tString);
    logEvent(&logData, message);

    clearMemoryData(&memData);

    if (configPtr->memDisplay)
    {
        logEvent(&logData, "--------------------------------------------------");
        logEvent(&logData, "After clear all process success");
        logEvent(&logData, "No memory configured");
        logEvent(&logData, "--------------------------------------------------");
    }

    accessTimer(LAP_TIMER, tString);
    snprintf(message, MAX_STR_LEN, " %s, OS: Simulation end", tString);
    logEvent(&logData, message);

    flushLogsToFile(&logData, configPtr->logToFileName);

    cleanup(pcbHead, &logData);
    pthread_mutex_destroy(&interruptData.mutex);
}

/*
Name: runSim
Process: master simulation loop (required to be here for Sim03)
Dependencies: createPCBList, allProcessStateReady, sched, runTimerThread,
              flushLogsToFile, logEvent, cleanup, accessTimer, snprintf, runTimer
*/
void runSim(ConfigDataType *configPtr, OpCodeType *metaDataMstPtr)
{
    // initialization
    char tString[STD_STR_LEN];
    char message[MAX_STR_LEN];
    bool segFaultFlag = false;
    int opTime = 0;

    // check for null pointers
    if (configPtr == NULL || metaDataMstPtr == NULL)
    {
        return;
    }

    if (configPtr->cpuSchedCode == CPU_SCHED_FCFS_P_CODE ||
        configPtr->cpuSchedCode == CPU_SCHED_SRTF_P_CODE ||
        configPtr->cpuSchedCode == CPU_SCHED_RR_P_CODE)
    {
        runPreemptiveSim(configPtr, metaDataMstPtr);
        return;
    }

    // start the timer
    accessTimer(ZERO_TIMER, tString);

    // setup logging behavior
    LogData logData;
    logData.head = NULL;
    logData.tail = NULL;

    logData.logToMonitor =
        (configPtr->logToCode == LOGTO_MONITOR_CODE ||
         configPtr->logToCode == LOGTO_BOTH_CODE);

    logData.logToFile =
        (configPtr->logToCode == LOGTO_FILE_CODE ||
         configPtr->logToCode == LOGTO_BOTH_CODE);

    // initialize memory data
    MemoryData memData;
    memData.head = NULL;
    memData.totalMemory = configPtr->memAvailable;

    // only show this under file-only condition
    if (configPtr->logToCode == LOGTO_FILE_CODE)
    {
        printf("Simulator running for output to file only\n");
    }

    // header lines
    logEvent(&logData, "Simulator Run");
    logEvent(&logData, "-------------");
    logEvent(&logData, "");

    // system start
    snprintf(message, MAX_STR_LEN, " %s, OS: Simulator start", tString);
    logEvent(&logData, message);

    // build PCBs
    PCB *pcbHead = createPCBList(metaDataMstPtr, configPtr);

    // set all processes to ready before starting scheduling loop
    allProcessStateReady(pcbHead, tString, &logData);

    // display initial memory if enabled
    if (configPtr->memDisplay)
    {
        displayMemoryStatus(&logData, &memData, tString,
                            "After memory initialization");
    }

    // select first
    PCB *currentPCB = sched(pcbHead, tString, &logData, configPtr->cpuSchedCode);
    logEvent(&logData, "");

    // one master loop only
    while (currentPCB != NULL)
    {
        segFaultFlag = false;

        // process opcodes until app end
        while (currentPCB->currentOp != NULL &&
               !isAppEndOp(currentPCB->currentOp) &&
               !segFaultFlag)
        {
            // CPU op
            if (compareString(currentPCB->currentOp->command, "cpu") == STR_EQ)
            {
                opTime = currentPCB->currentOp->intArg2 * configPtr->procCycleRate;

                accessTimer(LAP_TIMER, tString);
                snprintf(message, MAX_STR_LEN,
                         " %s, Process: %d, cpu process operation start",
                         tString, currentPCB->pid);
                logEvent(&logData, message);

                runTimer(opTime);

                // subtract remaining time
                if (currentPCB->timeleft >= opTime)
                {
                    currentPCB->timeleft -= opTime;
                }
                else
                {
                    currentPCB->timeleft = 0;
                }

                accessTimer(LAP_TIMER, tString);
                snprintf(message, MAX_STR_LEN,
                         " %s, Process: %d, cpu process operation end",
                         tString, currentPCB->pid);
                logEvent(&logData, message);
            }

            // DEV op
            else if (compareString(currentPCB->currentOp->command, "dev") == STR_EQ)
            {
                opTime = currentPCB->currentOp->intArg2 * configPtr->ioCycleRate;

                accessTimer(LAP_TIMER, tString);
                snprintf(message, MAX_STR_LEN,
                         " %s, Process: %d, %s %sput operation start",
                         tString, currentPCB->pid,
                         currentPCB->currentOp->strArg1,
                         currentPCB->currentOp->inOutArg);
                logEvent(&logData, message);

                runTimerThread(opTime);

                // subtract remaining time
                if (currentPCB->timeleft >= opTime)
                {
                    currentPCB->timeleft -= opTime;
                }
                else
                {
                    currentPCB->timeleft = 0;
                }

                accessTimer(LAP_TIMER, tString);
                snprintf(message, MAX_STR_LEN,
                         " %s, Process: %d, %s %sput operation end",
                         tString, currentPCB->pid,
                         currentPCB->currentOp->strArg1,
                         currentPCB->currentOp->inOutArg);
                logEvent(&logData, message);
            }

            // memory allocate or access
            else if (compareString(currentPCB->currentOp->command, "mem") == STR_EQ)
            {
                accessTimer(LAP_TIMER, tString);
                snprintf(message, MAX_STR_LEN,
                         " %s, Process: %d, mem %s request (%d, %d)",
                         tString,
                         currentPCB->pid,
                         currentPCB->currentOp->strArg1,
                         currentPCB->currentOp->intArg2,
                         currentPCB->currentOp->intArg3);
                logEvent(&logData, message);

                // allocate
                if (compareString(currentPCB->currentOp->strArg1, "allocate") == STR_EQ)
                {
                    bool allocSuccess = allocateMemory(&memData,
                                                       currentPCB->pid,
                                                       currentPCB->currentOp->intArg2,
                                                       currentPCB->currentOp->intArg3);

                    if (configPtr->memDisplay)
                    {
                        if (allocSuccess)
                        {
                            displayMemoryStatus(&logData, &memData, tString,
                                                "After allocate success");
                        }
                        else
                        {
                            displayMemoryStatus(&logData, &memData, tString,
                                                "After allocate overlap failure");
                        }
                    }

                    accessTimer(LAP_TIMER, tString);
                    if (allocSuccess)
                    {
                        snprintf(message, MAX_STR_LEN,
                                 " %s, Process: %d, successful mem allocate request",
                                 tString, currentPCB->pid);
                        logEvent(&logData, message);
                    }
                    else
                    {
                        snprintf(message, MAX_STR_LEN,
                                 " %s, Process: %d, failed mem allocate request",
                                 tString, currentPCB->pid);
                        logEvent(&logData, message);
                        segFaultFlag = true;
                    }
                }

                // access
                else if (compareString(currentPCB->currentOp->strArg1, "access") == STR_EQ)
                {
                    bool accessSuccess = accessMemory(&memData,
                                                      currentPCB->pid,
                                                      currentPCB->currentOp->intArg2,
                                                      currentPCB->currentOp->intArg3);

                    if (configPtr->memDisplay)
                    {
                        if (accessSuccess)
                        {
                            displayMemoryStatus(&logData, &memData, tString,
                                                "After access success");
                        }
                        else
                        {
                            displayMemoryStatus(&logData, &memData, tString,
                                                "After access failure");
                        }
                    }

                    accessTimer(LAP_TIMER, tString);
                    if (accessSuccess)
                    {
                        snprintf(message, MAX_STR_LEN,
                                 " %s, Process: %d, successful mem access request",
                                 tString, currentPCB->pid);
                        logEvent(&logData, message);
                    }
                    else
                    {
                        snprintf(message, MAX_STR_LEN,
                                 " %s, Process: %d, failed mem access request",
                                 tString, currentPCB->pid);
                        logEvent(&logData, message);
                        segFaultFlag = true;
                    }
                }
            }

            // advance op only if process has not faulted
            if (!segFaultFlag)
            {
                currentPCB->currentOp = currentPCB->currentOp->nextNode;
            }
        }

        // if seg fault occurred, report it
        if (segFaultFlag)
        {
            logEvent(&logData, "");

            accessTimer(LAP_TIMER, tString);
            snprintf(message, MAX_STR_LEN,
                     " %s, OS: Segmentation fault, Process %d ended",
                     tString, currentPCB->pid);
            logEvent(&logData, message);
        }
        else
        {
            // process ended normally
            logEvent(&logData, "");

            accessTimer(LAP_TIMER, tString);
            snprintf(message, MAX_STR_LEN,
                     " %s, OS: Process %d ended",
                     tString, currentPCB->pid);
            logEvent(&logData, message);
        }

        // clear process memory when process ends
        clearMemoryForProcess(&memData, currentPCB->pid);

        if (configPtr->memDisplay)
        {
            snprintf(message, MAX_STR_LEN,
                     "After clear process %d success",
                     currentPCB->pid);
            displayMemoryStatus(&logData, &memData, tString, message);
        }

        // running -> exit
        currentPCB->state = EXIT_STATE;

        accessTimer(LAP_TIMER, tString);
        snprintf(message, MAX_STR_LEN,
                 " %s, OS: Process %d set to EXIT",
                 tString, currentPCB->pid);
        logEvent(&logData, message);

        // next FCFS or SJF ready process
        currentPCB = sched(pcbHead, tString, &logData, configPtr->cpuSchedCode);

        if (currentPCB != NULL)
        {
            logEvent(&logData, "");
        }
    }

    // system stop
    accessTimer(LAP_TIMER, tString);
    snprintf(message, MAX_STR_LEN, " %s, OS: System stop", tString);
    logEvent(&logData, message);

    // clear all configured memory
    clearMemoryData(&memData);

    if (configPtr->memDisplay)
    {
        logEvent(&logData, "--------------------------------------------------");
        logEvent(&logData, "After clear all process success");
        logEvent(&logData, "No memory configured");
        logEvent(&logData, "--------------------------------------------------");
    }

    // simulation end
    accessTimer(LAP_TIMER, tString);
    snprintf(message, MAX_STR_LEN, " %s, OS: Simulation end", tString);
    logEvent(&logData, message);

    // file output must happen at one time after simulation is complete
    flushLogsToFile(&logData, configPtr->logToFileName);

    // cleanup memory
    cleanup(pcbHead, &logData);
}
