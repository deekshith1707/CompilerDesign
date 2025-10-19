#include "ir_context.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstdio>
#include <cstring>
#include <cstdlib>

using namespace std;

Quadruple IR[MAX_IR_SIZE];
int irCount = 0;
int tempCount = 0;
int labelCount = 0;

void emit(const char* op, const char* arg1, const char* arg2, const char* result) {
    if (irCount >= MAX_IR_SIZE) {
        cerr << "Error: IR size limit exceeded" << endl;
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
    ofstream fp(filename);
    if (!fp.is_open()) {
        cerr << "Error: Cannot open file " << filename << " for writing" << endl;
        return;
    }
    
    fp << "# Three-Address Code (Intermediate Representation)" << endl;
    fp << "# Format: OPERATION ARG1 ARG2 RESULT" << endl;
    fp << "# ================================================" << endl << endl;
    
    for (int i = 0; i < irCount; i++) {
        if (strlen(IR[i].op) > 0) {
            if (strcmp(IR[i].op, "LABEL") == 0) {
                fp << endl << IR[i].arg1 << ":" << endl;
            } else if (strcmp(IR[i].op, "FUNC_BEGIN") == 0) {
                fp << endl << "# Function Start: " << IR[i].arg1 << endl;
            } else if (strcmp(IR[i].op, "FUNC_END") == 0) {
                fp << "# Function End: " << IR[i].arg1 << endl;
            } else {
                fp << "    " << left << setw(15) << IR[i].op
                   << setw(25) << (strlen(IR[i].arg1) > 0 ? IR[i].arg1 : "")
                   << setw(25) << (strlen(IR[i].arg2) > 0 ? IR[i].arg2 : "")
                   << setw(25) << (strlen(IR[i].result) > 0 ? IR[i].result : "")
                   << endl;
            }
        }
    }
    
    fp.close();
    cout << "\nIntermediate code written to: " << filename << endl;
    cout << "Total IR instructions: " << irCount << endl;
}