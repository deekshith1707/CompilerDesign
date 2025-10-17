#ifndef IR_CONTEXT_H
#define IR_CONTEXT_H

#define MAX_IR_SIZE 10000

typedef struct {
    char op[32];
    char arg1[128];
    char arg2[128];
    char result[128];
} Quadruple;

// Global IR infrastructure
extern Quadruple IR[MAX_IR_SIZE];
extern int irCount;
extern int tempCount;
extern int labelCount;

// Function prototypes
void emit(const char* op, const char* arg1, const char* arg2, const char* result);
char* newTemp();
char* newLabel();
void printIR(const char* filename);

#endif // IR_CONTEXT_H