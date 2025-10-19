#ifndef IR_GENERATOR_H
#define IR_GENERATOR_H

#include "ast.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif // IR_GENERATOR_H