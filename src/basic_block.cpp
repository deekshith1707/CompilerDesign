/**
 * Basic Block Analysis and Next-Use Information
 * Following Lecture 34: Basic Blocks & Flow Graphs
 * 
 * This module implements:
 * 1. Basic block partitioning (Leader finding algorithm)
 * 2. Flow graph construction
 * 3. Next-use information computation (backward scan)
 */

#include "basic_block.h"
#include "ir_context.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Global data structures for analysis
BasicBlock blocks[MAX_BASIC_BLOCKS];
int blockCount = 0;

// Next-use and liveness information for each variable at each IR instruction
NextUseInfo nextUseTable[MAX_IR_SIZE];

/**
 * Check if an IR instruction is a label
 */
bool isLabel(const Quadruple* quad) {
    return (strlen(quad->op) > 0 && 
            quad->op[strlen(quad->op) - 1] == ':');
}

/**
 * Check if an IR instruction is a jump (goto, conditional branch, return)
 */
bool isJump(const Quadruple* quad) {
    return (strcmp(quad->op, "goto") == 0 ||
            strcmp(quad->op, "GOTO") == 0 ||
            strcmp(quad->op, "if_true_goto") == 0 ||
            strcmp(quad->op, "IF_TRUE_GOTO") == 0 ||
            strcmp(quad->op, "if_false_goto") == 0 ||
            strcmp(quad->op, "IF_FALSE_GOTO") == 0 ||
            strcmp(quad->op, "return") == 0 ||
            strcmp(quad->op, "RETURN") == 0 ||
            strcmp(quad->op, "func_end") == 0 ||
            strcmp(quad->op, "FUNC_END") == 0);
}

/**
 * Check if instruction is a function begin
 */
bool isFuncBegin(const Quadruple* quad) {
    return (strcmp(quad->op, "func_begin") == 0 || 
            strcmp(quad->op, "FUNC_BEGIN") == 0);
}

/**
 * Check if instruction is a function end
 */
bool isFuncEnd(const Quadruple* quad) {
    return (strcmp(quad->op, "func_end") == 0 || 
            strcmp(quad->op, "FUNC_END") == 0);
}

/**
 * Extract label name from a label instruction
 */
void extractLabelName(const Quadruple* quad, char* labelName) {
    if (isLabel(quad)) {
        // Label is stored in the 'op' field with ':' at the end
        strcpy(labelName, quad->op);
        // Remove the trailing ':'
        labelName[strlen(labelName) - 1] = '\0';
    } else {
        labelName[0] = '\0';
    }
}

/**
 * Find all leaders in the IR code
 * 
 * Leader Finding Algorithm (Lecture 34):
 * 1. First statement after func_begin is a leader
 * 2. Any statement that is a label target is a leader
 * 3. Any statement immediately following a jump is a leader
 */
void findLeaders(bool leaders[], int start, int end) {
    // Initialize all as non-leaders
    for (int i = start; i <= end; i++) {
        leaders[i] = false;
    }
    
    // Rule 1: First statement after func_begin is a leader
    if (start < end) {
        leaders[start + 1] = true;
    }
    
    // Scan all instructions
    for (int i = start; i <= end; i++) {
        Quadruple* quad = &IR[i];
        
        // Rule 2: Any label is a leader
        if (isLabel(quad)) {
            leaders[i] = true;
        }
        
        // Rule 3: Statement following a jump is a leader
        if (isJump(quad) && i + 1 <= end) {
            leaders[i + 1] = true;
        }
    }
}

/**
 * Build basic blocks for a function
 * 
 * Basic Block Construction (Lecture 34):
 * - Each leader starts a new basic block
 * - Block extends from leader to (but not including) next leader
 */
int buildBasicBlocksForFunction(int funcStart, int funcEnd) {
    bool leaders[MAX_IR_SIZE];
    int localBlockCount = 0;
    
    // Find all leaders in this function
    findLeaders(leaders, funcStart, funcEnd);
    
    // Create basic blocks
    int blockStart = -1;
    for (int i = funcStart; i <= funcEnd; i++) {
        if (leaders[i]) {
            // If we had a previous block, close it
            if (blockStart != -1) {
                blocks[blockCount].startIndex = blockStart;
                blocks[blockCount].endIndex = i - 1;
                blocks[blockCount].id = blockCount;
                blocks[blockCount].successorCount = 0;
                blocks[blockCount].predecessorCount = 0;
                blockCount++;
                localBlockCount++;
            }
            // Start new block
            blockStart = i;
        }
    }
    
    // Close the last block
    if (blockStart != -1) {
        blocks[blockCount].startIndex = blockStart;
        blocks[blockCount].endIndex = funcEnd;
        blocks[blockCount].id = blockCount;
        blocks[blockCount].successorCount = 0;
        blocks[blockCount].predecessorCount = 0;
        blockCount++;
        localBlockCount++;
    }
    
    return localBlockCount;
}

/**
 * Find basic block containing a specific label
 */
int findBlockByLabel(const char* label, int startBlock, int numBlocks) {
    for (int b = startBlock; b < startBlock + numBlocks; b++) {
        for (int i = blocks[b].startIndex; i <= blocks[b].endIndex; i++) {
            if (isLabel(&IR[i])) {
                char labelName[128];
                extractLabelName(&IR[i], labelName);
                if (strcmp(labelName, label) == 0) {
                    return b;
                }
            }
        }
    }
    return -1;
}

/**
 * Build control flow graph
 * 
 * Flow Graph Construction (Lecture 34):
 * - Add edge from B1 to B2 if:
 *   1. B2 immediately follows B1 (fall-through), OR
 *   2. B1 ends with jump to B2's label
 */
void buildFlowGraph(int startBlock, int numBlocks) {
    for (int b = startBlock; b < startBlock + numBlocks; b++) {
        BasicBlock* block = &blocks[b];
        Quadruple* lastQuad = &IR[block->endIndex];
        
        // Case 1: If last instruction is not an unconditional jump,
        // add fall-through edge to next block
        if (strcmp(lastQuad->op, "goto") != 0 && 
            strcmp(lastQuad->op, "GOTO") != 0 && 
            strcmp(lastQuad->op, "return") != 0 &&
            strcmp(lastQuad->op, "RETURN") != 0 &&
            strcmp(lastQuad->op, "func_end") != 0 &&
            strcmp(lastQuad->op, "FUNC_END") != 0 &&
            b + 1 < startBlock + numBlocks) {
            
            block->successors[block->successorCount++] = b + 1;
            blocks[b + 1].predecessors[blocks[b + 1].predecessorCount++] = b;
        }
        
        // Case 2: If last instruction is a jump, add edge to target
        if (strcmp(lastQuad->op, "goto") == 0 ||
            strcmp(lastQuad->op, "GOTO") == 0 ||
            strcmp(lastQuad->op, "if_true_goto") == 0 ||
            strcmp(lastQuad->op, "IF_TRUE_GOTO") == 0 ||
            strcmp(lastQuad->op, "if_false_goto") == 0 ||
            strcmp(lastQuad->op, "IF_FALSE_GOTO") == 0) {
            
            // Target label is in result field
            const char* targetLabel = lastQuad->result;
            int targetBlock = findBlockByLabel(targetLabel, startBlock, numBlocks);
            
            if (targetBlock != -1) {
                block->successors[block->successorCount++] = targetBlock;
                blocks[targetBlock].predecessors[blocks[targetBlock].predecessorCount++] = b;
            }
        }
    }
}

/**
 * Check if a string is a constant (number or character)
 */
bool isConstant(const char* str) {
    if (str == NULL || strlen(str) == 0) return true;
    
    // Check for numbers
    if (str[0] == '-' || (str[0] >= '0' && str[0] <= '9')) {
        return true;
    }
    
    // Check for character literals
    if (str[0] == '\'') {
        return true;
    }
    
    // Check for string literals
    if (str[0] == '"') {
        return true;
    }
    
    return false;
}

/**
 * Initialize next-use information for a variable
 */
void initNextUse(NextUseInfo* info) {
    for (int i = 0; i < MAX_VARS_PER_INSTRUCTION; i++) {
        info->varNames[i][0] = '\0';
        info->isLive[i] = false;
        info->nextUse[i] = -1;
    }
    info->varCount = 0;
}

/**
 * Find variable in next-use info table
 */
int findVarInNextUse(NextUseInfo* info, const char* varName) {
    for (int i = 0; i < info->varCount; i++) {
        if (strcmp(info->varNames[i], varName) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * Add or update variable in next-use info
 */
void updateNextUse(NextUseInfo* info, const char* varName, bool live, int nextUseIndex) {
    if (varName == NULL || strlen(varName) == 0 || isConstant(varName)) {
        return;
    }
    
    int idx = findVarInNextUse(info, varName);
    if (idx == -1) {
        // Add new variable
        if (info->varCount < MAX_VARS_PER_INSTRUCTION) {
            strcpy(info->varNames[info->varCount], varName);
            info->isLive[info->varCount] = live;
            info->nextUse[info->varCount] = nextUseIndex;
            info->varCount++;
        }
    } else {
        // Update existing variable
        info->isLive[idx] = live;
        info->nextUse[idx] = nextUseIndex;
    }
}

/**
 * Compute next-use information for a basic block
 * 
 * Next-Use Algorithm (Lecture 34 - Backward Scan):
 * For each statement S: result = arg1 op arg2 (from END to START):
 *   1. Attach current next-use info to S
 *   2. Mark result as "dead" (no next use)
 *   3. Mark arg1 and arg2 as "live" with next use = S
 */
void computeNextUseForBlock(int blockId) {
    BasicBlock* block = &blocks[blockId];
    
    // Symbol table for tracking next-use within this block
    // Maps variable name -> {isLive, nextUseIndex}
    NextUseInfo currentState;
    initNextUse(&currentState);
    
    // Scan block BACKWARDS from end to start
    for (int i = block->endIndex; i >= block->startIndex; i--) {
        Quadruple* quad = &IR[i];
        
        // Initialize next-use info for this instruction
        initNextUse(&nextUseTable[i]);
        
        // Step 1: Attach current next-use information to this statement
        // Copy current state to this instruction's next-use info
        for (int v = 0; v < currentState.varCount; v++) {
            updateNextUse(&nextUseTable[i], 
                         currentState.varNames[v],
                         currentState.isLive[v],
                         currentState.nextUse[v]);
        }
        
        // Skip if this is a label or function marker
        if (isLabel(quad) || isFuncBegin(quad) || isFuncEnd(quad)) {
            continue;
        }
        
        // Step 2: Mark result variable as "dead" (no next use)
        if (strlen(quad->result) > 0 && !isConstant(quad->result)) {
            updateNextUse(&currentState, quad->result, false, -1);
        }
        
        // Step 3: Mark operands as "live" with next use = current instruction
        if (strlen(quad->arg1) > 0 && !isConstant(quad->arg1)) {
            updateNextUse(&currentState, quad->arg1, true, i);
        }
        
        if (strlen(quad->arg2) > 0 && !isConstant(quad->arg2)) {
            updateNextUse(&currentState, quad->arg2, true, i);
        }
    }
}

/**
 * Compute next-use information for all basic blocks
 */
void computeNextUseInformation(int startBlock, int numBlocks) {
    for (int b = startBlock; b < startBlock + numBlocks; b++) {
        computeNextUseForBlock(b);
    }
}

/**
 * Main entry point: Partition IR into basic blocks and compute next-use info
 */
void analyzeIR() {
    blockCount = 0;
    
    // Initialize next-use table
    for (int i = 0; i < MAX_IR_SIZE; i++) {
        initNextUse(&nextUseTable[i]);
    }
    
    // Find all functions in the IR and analyze each one
    int i = 0;
    while (i < irCount) {
        if (isFuncBegin(&IR[i])) {
            int funcStart = i;
            int funcEnd = i;
            
            // Find the end of this function
            while (funcEnd < irCount && !isFuncEnd(&IR[funcEnd])) {
                funcEnd++;
            }
            
            int startBlock = blockCount;
            int numBlocks = buildBasicBlocksForFunction(funcStart, funcEnd);
            
            // Build control flow graph for this function
            buildFlowGraph(startBlock, numBlocks);
            
            // Compute next-use information
            computeNextUseInformation(startBlock, numBlocks);
            
            i = funcEnd + 1;
        } else {
            i++;
        }
    }
}

/**
 * Print basic block information (for debugging)
 */
void printBasicBlocks() {
    printf("\n========================================\n");
    printf("BASIC BLOCK ANALYSIS RESULTS\n");
    printf("========================================\n\n");
    
    for (int b = 0; b < blockCount; b++) {
        BasicBlock* block = &blocks[b];
        
        printf("Block B%d: [%d-%d]\n", block->id, block->startIndex, block->endIndex);
        
        // Print instructions in this block
        printf("  Instructions:\n");
        for (int i = block->startIndex; i <= block->endIndex; i++) {
            printf("    [%d] %s %s %s %s\n", i,
                   IR[i].op, IR[i].arg1, IR[i].arg2, IR[i].result);
        }
        
        // Print successors
        if (block->successorCount > 0) {
            printf("  Successors: ");
            for (int s = 0; s < block->successorCount; s++) {
                printf("B%d ", blocks[block->successors[s]].id);
            }
            printf("\n");
        }
        
        // Print predecessors
        if (block->predecessorCount > 0) {
            printf("  Predecessors: ");
            for (int p = 0; p < block->predecessorCount; p++) {
                printf("B%d ", blocks[block->predecessors[p]].id);
            }
            printf("\n");
        }
        
        printf("\n");
    }
}

/**
 * Print next-use information (for debugging)
 */
void printNextUseInfo() {
    printf("\n========================================\n");
    printf("NEXT-USE INFORMATION\n");
    printf("========================================\n\n");
    
    for (int i = 0; i < irCount; i++) {
        if (nextUseTable[i].varCount > 0) {
            printf("[%d] %s %s %s %s\n", i,
                   IR[i].op, IR[i].arg1, IR[i].arg2, IR[i].result);
            printf("  Next-use info:\n");
            
            for (int v = 0; v < nextUseTable[i].varCount; v++) {
                printf("    %s: %s, next-use=%d\n",
                       nextUseTable[i].varNames[v],
                       nextUseTable[i].isLive[v] ? "live" : "dead",
                       nextUseTable[i].nextUse[v]);
            }
        }
    }
}

/**
 * Get next-use information for a variable at a specific IR instruction
 */
bool getNextUseInfo(int irIndex, const char* varName, bool* isLive, int* nextUse) {
    if (irIndex < 0 || irIndex >= irCount) {
        return false;
    }
    
    NextUseInfo* info = &nextUseTable[irIndex];
    int idx = findVarInNextUse(info, varName);
    
    if (idx != -1) {
        *isLive = info->isLive[idx];
        *nextUse = info->nextUse[idx];
        return true;
    }
    
    return false;
}
