#ifndef IR_GENERATOR_H
#define IR_GENERATOR_H

#include "ast.h"

// Main IR generation function
char* generate_ir(TreeNode* node);

// Helper structure for control flow
typedef struct {
    char* continue_label;
    char* break_label;
} LoopContext;

typedef struct {
    char* end_label;
} SwitchContext;

#endif // IR_GENERATOR_H