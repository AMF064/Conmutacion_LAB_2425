#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdarg.h>


/********************************************************************
 * Constant definitions
 ********************************************************************/
#define OUTPUT_NAME ".out"
#define OK 0
#define ROUTING_TABLE_NOT_FOUND -3000
#define INPUT_FILE_NOT_FOUND -3001
#define BAD_ROUTING_TABLE -3002
#define REACHED_EOF -3003
#define BAD_INPUT_FILE -3004
#define PARSE_ERROR -3005
#define CANNOT_CREATE_OUTPUT -3006

/***********************************************************************
 * Write the input to the specified file (f) and the standard output
 *
 * Use as fprintf(FILE *stream, const char *format, ...)
 *
 ***********************************************************************/
void tee(FILE *f, char const *fmt, ...);


/***********************************************************************
 * Initalize file descriptors
 *
 * routingTableName contains FIB info (argv[1] of main function)
 * inputFileName contains IP addresses (argv[2] of main function)
 *
 ***********************************************************************/
int initializeIO(char *routingTableName, char *inputFileName);


/***********************************************************************
 * Close the input/output files
 ***********************************************************************/
void freeIO();


/***********************************************************************
 * Write explanation for error identifier (verbose mode)
 ***********************************************************************/
void printIOExplanationError(int result);


/***********************************************************************
 * Read one entry in the FIB
 *
 * It should be noted that prefix, prefixLength and outInterface are
 * pointers since they are used as output parameters
 *
 ***********************************************************************/
int readFIBLine(uint32_t *prefix, int *prefixLength, int *outInterface);


/***********************************************************************
 * Read one entry in the input packet file
 *
 * Again, it should be noted that IPAddress is a pointer since it is used
 * as output parameter
 *
 ***********************************************************************/
int readInputPacketFileLine(uint32_t *IPAddress);


/***********************************************************************
 * Print a line to the output file
 *
 * clock_gettime(CLOCK_MONOTONIC_RAW, &initialTime) must be called right before the lookup function
 *
 * clock_gettime(CLOCK_MONOTONIC_RAW, &finalTime) must be called right after the lookup function
 *
 * The lookup function must return (either as output parameter or as return value)
 * the number of hash tables that have been accessed for every IP address
 *
 ***********************************************************************/
 void printOutputLine(uint32_t IPAddress, int outInterface, struct timespec *initialTime, struct timespec *finalTime,
                        double *searchingTime, int numberOfTableAccesses);


/***********************************************************************
 * Print execution summary to the output file
 *
 * It should be noted that:
 *
 * 		averageNodeAccesses = totalNodeAccesses/processedPackets
 *
 *		averagePacketProcessingTime = totalPacketProcessingTime/processedPackets
 *
 ***********************************************************************/
void printSummary(int NumberOfNodesInTrie, int processedPackets, double averageNodeAccesses, double averagePacketProcessingTime);


/***********************************************************************
 * Print memory and CPU time
 *
 * For more info: man getrusage
 *
 ***********************************************************************/
void printMemoryTimeUsage();


