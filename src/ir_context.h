#ifndef IR_CONTEXT_H
#define IR_CONTEXT_H

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_IR_SIZE 10000
#define MAX_STATIC_VARS 500

// Backpatching data structures
typedef struct JumpListNode {
    int quad_index;
    struct JumpListNode* next;
} JumpListNode;

typedef struct {
    JumpListNode* head;
} JumpList;

// Structure to track static variables for initialization
typedef struct {
    char name[256];           // Full name (e.g., "static_function.local_static")
    char init_value[128];     // Initialization value
    int is_initialized;       // Whether it has an initializer
} StaticVarInfo;

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

// Static variable tracking
extern StaticVarInfo staticVars[MAX_STATIC_VARS];
extern int staticVarCount;

// Function prototypes
void emit(const char* op, const char* arg1, const char* arg2, const char* result);
int emitWithIndex(const char* op, const char* arg1, const char* arg2, const char* result);
char* newTemp();
char* newLabel();
int nextQuad();
void printIR(const char* filename);
void registerStaticVar(const char* name, const char* init_value);
void emitStaticVarInitializations();

// Backpatching functions
JumpList* makelist(int quad_index);
JumpList* merge(JumpList* list1, JumpList* list2);
void backpatch(JumpList* list, const char* target_label);
void freeJumpList(JumpList* list);

#ifdef __cplusplus
}
#endif

#endif // IR_CONTEXT_H
