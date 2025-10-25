#include "ir_context.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>

using namespace std;

Quadruple IR[MAX_IR_SIZE];
int irCount = 0;
int tempCount = 0;
int labelCount = 0;

// Static variable tracking
StaticVarInfo staticVars[MAX_STATIC_VARS];
int staticVarCount = 0;

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

void registerStaticVar(const char* name, const char* init_value) {
    if (staticVarCount >= MAX_STATIC_VARS) {
        cerr << "Error: Too many static variables" << endl;
        return;
    }
    
    strcpy(staticVars[staticVarCount].name, name);
    if (init_value && strlen(init_value) > 0) {
        strcpy(staticVars[staticVarCount].init_value, init_value);
        staticVars[staticVarCount].is_initialized = 1;
    } else {
        strcpy(staticVars[staticVarCount].init_value, "0");
        staticVars[staticVarCount].is_initialized = 0;
    }
    staticVarCount++;
}

string convertToThreeAddress(const Quadruple& quad) {
    string op = quad.op;
    string arg1 = quad.arg1;
    string arg2 = quad.arg2;
    string result = quad.result;
    
    // Assignment operations: x = y op z, x = op y, x = y
    if (op == "ASSIGN") {
        return result + " = " + arg1;
    }
    else if (op == "ADD") {
        return result + " = " + arg1 + " + " + arg2;
    }
    else if (op == "SUB") {
        return result + " = " + arg1 + " - " + arg2;
    }
    else if (op == "MUL") {
        return result + " = " + arg1 + " * " + arg2;
    }
    else if (op == "DIV") {
        return result + " = " + arg1 + " / " + arg2;
    }
    else if (op == "MOD") {
        return result + " = " + arg1 + " % " + arg2;
    }
    else if (op == "NEG") {
        return result + " = -" + arg1;
    }
    else if (op == "NOT") {
        return result + " = !" + arg1;
    }
    else if (op == "BITNOT") {
        return result + " = ~" + arg1;
    }
    
    // Bitwise operations
    else if (op == "BITAND") {
        return result + " = " + arg1 + " & " + arg2;
    }
    else if (op == "BITOR") {
        return result + " = " + arg1 + " | " + arg2;
    }
    else if (op == "BITXOR") {
        return result + " = " + arg1 + " ^ " + arg2;
    }
    else if (op == "LSHIFT") {
        return result + " = " + arg1 + " << " + arg2;
    }
    else if (op == "RSHIFT") {
        return result + " = " + arg1 + " >> " + arg2;
    }
    
    // Relational operations
    else if (op == "LT") {
        return result + " = " + arg1 + " < " + arg2;
    }
    else if (op == "GT") {
        return result + " = " + arg1 + " > " + arg2;
    }
    else if (op == "LE") {
        return result + " = " + arg1 + " <= " + arg2;
    }
    else if (op == "GE") {
        return result + " = " + arg1 + " >= " + arg2;
    }
    else if (op == "EQ") {
        return result + " = " + arg1 + " == " + arg2;
    }
    else if (op == "NE") {
        return result + " = " + arg1 + " != " + arg2;
    }
    
    // Jump operations: goto L, if x relop y goto L
    else if (op == "GOTO") {
        return "goto " + arg1;
    }
    else if (op == "IF_TRUE_GOTO") {
        return "if " + arg1 + " != 0 goto " + arg2;
    }
    else if (op == "IF_FALSE_GOTO") {
        return "if " + arg1 + " == 0 goto " + arg2;
    }
    else if (op == "IF_FALSE_GOTO_FLOAT") {
        return "if " + arg1 + " == 0.0 goto " + arg2;
    }
    else if (op == "IF_TRUE_GOTO_FLOAT") {
        return "if " + arg1 + " != 0.0 goto " + arg2;
    }
    
    // Indexed assignment: x = y[i], x[i] = y
    else if (op == "ARRAY_ACCESS") {
        return result + " = " + arg1 + "[" + arg2 + "]";
    }
    else if (op == "ASSIGN_ARRAY") {
        return arg2 + "[" + arg1 + "] = " + result; // Note: reordered for correct assignment
    }
    // Array element address: x = &arr[i] (computed as arr + i)
    else if (op == "ARRAY_ADDR") {
        return result + " = " + arg1 + " + " + arg2;
    }
    
    // Function operations: param x, call p,n, return y
    else if (op == "ARG" || op == "PARAM") {
        return "param " + arg1;
    }
    else if (op == "CALL") {
        if (strlen(quad.result) > 0) {
            return result + " = call " + arg1 + ", " + (strlen(quad.arg2) > 0 ? arg2 : "0");
        } else {
            return "call " + arg1 + ", " + (strlen(quad.arg2) > 0 ? arg2 : "0");
        }
    }
    else if (op == "RETURN") {
        if (strlen(quad.arg1) > 0) {
            return "return " + arg1;
        } else {
            return "return";
        }
    }
    
    // Pointer operations: x = &y, x = *y, *x = y
    else if (op == "ADDR") {
        return result + " = &" + arg1;
    }
    else if (op == "DEREF") {
        return result + " = *" + arg1;
    }
    else if (op == "ASSIGN_DEREF") {
        return "*" + arg2 + " = " + arg1;
    }
    
    // Member access operations (convert to appropriate format)
    else if (op == "LOAD_MEMBER") {
        return result + " = " + arg1 + "." + arg2;
    }
    else if (op == "ASSIGN_MEMBER") {
        return arg2 + "." + arg1 + " = " + result;
    }
    else if (op == "LOAD_ARROW") {
        return result + " = " + arg1 + "->" + arg2;
    }
    else if (op == "ASSIGN_ARROW") {
        return arg2 + "->" + arg1 + " = " + result;
    }
    
    // Type casting operations: result = cast_op(arg1)
    else if (op.substr(0, 5) == "CAST_") {
        return result + " = " + op + "(" + arg1 + ")";
    }
    
    // Increment/Decrement operations: result = arg1 + 1, result = arg1 - 1
    else if (op == "INC") {
        return result + " = " + arg1 + " + 1";
    }
    else if (op == "DEC") {
        return result + " = " + arg1 + " - 1";
    }
    
    // Pointer arithmetic: result = arg1 + arg2 (pointer arithmetic)
    else if (op == "PTR_ADD") {
        return result + " = " + arg1 + " + " + arg2;
    }
    else if (op == "PTR_SUB") {
        return result + " = " + arg1 + " - " + arg2;
    }
    
    // Type promotion: result = (double)arg1
    else if (op == "FLOAT_TO_DOUBLE") {
        return result + " = (double)" + arg1;
    }
    
    // Default case - return original format for unknown operations
    else {
        return string(quad.op) + " " + string(quad.arg1) + " " + string(quad.arg2) + " " + string(quad.result);
    }
}

void printIR(const char* filename) {
    ofstream fp(filename);
    if (!fp.is_open()) {
        cerr << "Error: Cannot open file " << filename << " for writing" << endl;
        return;
    }
    
    fp << "# Three-Address Code (Intermediate Representation)" << endl;
    fp << "# ================================================" << endl << endl;
    
    // First, emit DATA section for static/global variable initializations
    bool hasGlobalsOrStatics = false;
    
    // Check if we have any global variables or static variables
    for (int i = 0; i < irCount; i++) {
        if (strcmp(IR[i].op, "ASSIGN") == 0 && strcmp(IR[i].op, "FUNC_BEGIN") != 0) {
            // Check if this is a global assignment (before any function)
            bool beforeAnyFunction = true;
            for (int j = 0; j < i; j++) {
                if (strcmp(IR[j].op, "FUNC_BEGIN") == 0) {
                    beforeAnyFunction = false;
                    break;
                }
            }
            if (beforeAnyFunction) {
                if (!hasGlobalsOrStatics) {
                    fp << "DATA:" << endl;
                    hasGlobalsOrStatics = true;
                }
            }
        }
    }
    
    // Emit static variable initializations in DATA section
    if (staticVarCount > 0) {
        if (!hasGlobalsOrStatics) {
            fp << "DATA:" << endl;
            hasGlobalsOrStatics = true;
        }
        for (int i = 0; i < staticVarCount; i++) {
            fp << "    " << staticVars[i].name << " = " << staticVars[i].init_value << endl;
        }
    }
    
    // Emit global variable initializations in DATA section
    for (int i = 0; i < irCount; i++) {
        if (strcmp(IR[i].op, "ASSIGN") == 0) {
            // Check if this is a global assignment (before any function)
            bool beforeAnyFunction = true;
            for (int j = 0; j < i; j++) {
                if (strcmp(IR[j].op, "FUNC_BEGIN") == 0) {
                    beforeAnyFunction = false;
                    break;
                }
            }
            if (beforeAnyFunction) {
                string instruction = convertToThreeAddress(IR[i]);
                fp << "    " << instruction << endl;
            }
        }
    }
    
    if (hasGlobalsOrStatics) {
        fp << endl;
    }
    
    // Then emit the rest of the IR (functions)
    bool inFunction = false;
    for (int i = 0; i < irCount; i++) {
        if (strlen(IR[i].op) > 0) {
            if (strcmp(IR[i].op, "FUNC_BEGIN") == 0) {
                fp << "func_begin " << IR[i].arg1 << endl;
                inFunction = true;
            } else if (strcmp(IR[i].op, "FUNC_END") == 0) {
                fp << "func_end " << IR[i].arg1 << endl << endl;
                inFunction = false;
            } else if (strcmp(IR[i].op, "LABEL") == 0) {
                fp << IR[i].arg1 << ":" << endl;
            } else if (strcmp(IR[i].op, "ASSIGN") == 0) {
                // Skip global assignments (already handled in DATA section)
                bool beforeAnyFunction = true;
                for (int j = 0; j < i; j++) {
                    if (strcmp(IR[j].op, "FUNC_BEGIN") == 0) {
                        beforeAnyFunction = false;
                        break;
                    }
                }
                if (!beforeAnyFunction || inFunction) {
                    // This is inside a function
                    string instruction = convertToThreeAddress(IR[i]);
                    fp << "    " << instruction << endl;
                }
            } else {
                // Convert to three-address instruction format
                string instruction = convertToThreeAddress(IR[i]);
                fp << "    " << instruction << endl;
            }
        }
    }
    
    fp.close();
    cout << "\nIntermediate code written to: " << filename << endl;
    cout << "Total IR instructions: " << irCount << endl;
    if (staticVarCount > 0) {
        cout << "Static variables: " << staticVarCount << endl;
    }
}