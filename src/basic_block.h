/**
 * Basic Block Analysis and Next-Use Information
 * Following Lecture 34: Basic Blocks & Flow Graphs
 */

#ifndef BASIC_BLOCK_H
#define BASIC_BLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "ir_context.h"

#define MAX_BASIC_BLOCKS 1000
#define MAX_SUCCESSORS 10
#define MAX_PREDECESSORS 10
#define MAX_VARS_PER_INSTRUCTION 20

/**
 * Basic Block Structure (Lecture 34)
 * Represents a sequence of instructions with single entry and exit
 */
typedef struct BasicBlock {
    int id;                              // Block identifier
    int startIndex;                      // First IR instruction in block
    int endIndex;                        // Last IR instruction in block
    
    // Control Flow Graph edges
    int successors[MAX_SUCCESSORS];      // Outgoing edges
    int successorCount;
    int predecessors[MAX_PREDECESSORS];  // Incoming edges
    int predecessorCount;
} BasicBlock;

/**
 * Next-Use Information (Lecture 34)
 * For each variable at each instruction, tracks:
 * - Whether the variable is live (will be used again)
 * - The next instruction that uses this variable (-1 if no next use)
 */
typedef struct NextUseInfo {
    char varNames[MAX_VARS_PER_INSTRUCTION][128];  // Variable names
    bool isLive[MAX_VARS_PER_INSTRUCTION];         // Live/dead status
    int nextUse[MAX_VARS_PER_INSTRUCTION];         // Index of next use (-1 if none)
    int varCount;                                   // Number of variables tracked
} NextUseInfo;

// Global data structures
extern BasicBlock blocks[MAX_BASIC_BLOCKS];
extern int blockCount;
extern NextUseInfo nextUseTable[];  // One entry per IR instruction

// Main analysis functions
void analyzeIR();                    // Main entry point: analyze entire IR
void printBasicBlocks();             // Print basic block information
void printNextUseInfo();             // Print next-use information

// Helper functions
bool isLabel(const Quadruple* quad);
bool isJump(const Quadruple* quad);
bool isConstant(const char* str);
bool getNextUseInfo(int irIndex, const char* varName, bool* isLive, int* nextUse);

// Internal analysis functions
void findLeaders(bool leaders[], int start, int end);
int buildBasicBlocksForFunction(int funcStart, int funcEnd);
void buildFlowGraph(int startBlock, int numBlocks);
void computeNextUseInformation(int startBlock, int numBlocks);
void computeNextUseForBlock(int blockId);

#ifdef __cplusplus
}
#endif

#endif // BASIC_BLOCK_H
