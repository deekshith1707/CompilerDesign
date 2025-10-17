#include "ir_context.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

Quadruple IR[MAX_IR_SIZE];
int irCount = 0;
int tempCount = 0;
int labelCount = 0;

void emit(const char* op, const char* arg1, const char* arg2, const char* result) {
    if (irCount >= MAX_IR_SIZE) {
        fprintf(stderr, "Error: IR size limit exceeded\n");
        return;
    }
    strcpy(IR[irCount].op, op);
    strcpy(IR[irCount].arg1, arg1 ? arg1 : "");
    strcpy(IR[irCount].arg2, arg2 ? arg2 : "");
    strcpy(IR[irCount].result, result ? result : "");
    irCount++;
}

char* newTemp() {
    static char temp[16];
    sprintf(temp, "t%d", tempCount++);
    return strdup(temp);
}

char* newLabel() {
    static char label[32];
    sprintf(label, "L%d", labelCount++);
    return strdup(label);
}

void printIR(const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file %s for writing\n", filename);
        return;
    }
    
    fprintf(fp, "# Three-Address Code (Intermediate Representation)\n");
    fprintf(fp, "# Format: OPERATION ARG1 ARG2 RESULT\n");
    fprintf(fp, "# ================================================\n\n");
    
    for (int i = 0; i < irCount; i++) {
        if (strlen(IR[i].op) > 0) {
            if (strcmp(IR[i].op, "LABEL") == 0) {
                fprintf(fp, "\n%s:\n", IR[i].arg1);
            } else if (strcmp(IR[i].op, "FUNC_BEGIN") == 0) {
                fprintf(fp, "\n# Function Start: %s\n", IR[i].arg1);
            } else if (strcmp(IR[i].op, "FUNC_END") == 0) {
                fprintf(fp, "# Function End: %s\n", IR[i].arg1);
            } else {
                fprintf(fp, "    %-15s", IR[i].op);
                if (strlen(IR[i].arg1) > 0) fprintf(fp, " %-25s", IR[i].arg1);
                else fprintf(fp, " %-25s", "");
                if (strlen(IR[i].arg2) > 0) fprintf(fp, " %-25s", IR[i].arg2);
                else fprintf(fp, " %-25s", "");
                if (strlen(IR[i].result) > 0) fprintf(fp, " %-25s", IR[i].result);
                fprintf(fp, "\n");
            }
        }
    }
    
    fclose(fp);
    printf("\nIntermediate code written to: %s\n", filename);
    printf("Total IR instructions: %d\n", irCount);
}