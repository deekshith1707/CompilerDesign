#include "ir_generator.h"
#include "ir_context.h"
#include "symbol_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Control flow stacks
#define MAX_LOOP_DEPTH 100
static LoopContext loopStack[MAX_LOOP_DEPTH];
static int loopDepth = 0;
static SwitchContext switchStack[MAX_LOOP_DEPTH];
static int switchDepth = 0;

// Helper functions for control flow
static void pushLoopLabels(char* continue_label, char* break_label) {
    if (loopDepth >= MAX_LOOP_DEPTH) {
        fprintf(stderr, "Error: Loop nesting too deep\n");
        return;
    }
    loopStack[loopDepth].continue_label = continue_label;
    loopStack[loopDepth].break_label = break_label;
    loopDepth++;
}

static void popLoopLabels() {
    if (loopDepth > 0) loopDepth--;
}

static char* getCurrentLoopContinue() {
    return (loopDepth > 0) ? loopStack[loopDepth - 1].continue_label : NULL;
}

static char* getCurrentLoopBreak() {
    return (loopDepth > 0) ? loopStack[loopDepth - 1].break_label : NULL;
}

static void pushSwitchLabel(char* end_label) {
    if (switchDepth >= MAX_LOOP_DEPTH) {
        fprintf(stderr, "Error: Switch nesting too deep\n");
        return;
    }
    switchStack[switchDepth].end_label = end_label;
    switchDepth++;
}

static void popSwitchLabel() {
    if (switchDepth > 0) switchDepth--;
}

static char* getCurrentSwitchEnd() {
    return (switchDepth > 0) ? switchStack[switchDepth - 1].end_label : NULL;
}

// Type conversion helper
static char* convertType(char* place, const char* from_type, const char* to_type) {
    if (strcmp(from_type, to_type) == 0) return place;
    char* temp = newTemp();
    char cast_op[64];
    sprintf(cast_op, "CAST_%s_to_%s", from_type, to_type);
    emit(cast_op, place, "", temp);
    return temp;
}

// Main IR generation function
char* generate_ir(TreeNode* node) {
    if (!node) return NULL;

    switch (node->type) {
        
        // ===== LEAF NODES =====
        
        case NODE_CONSTANT:
        case NODE_INTEGER_CONSTANT:
        case NODE_HEX_CONSTANT:
        case NODE_OCTAL_CONSTANT:
        case NODE_BINARY_CONSTANT:
        case NODE_FLOAT_CONSTANT:
        case NODE_CHAR_CONSTANT:
        case NODE_STRING_LITERAL:
        {
            char* temp = newTemp();
            emit("ASSIGN", node->value, "", temp);
            return temp;
        }
        
        case NODE_IDENTIFIER:
        {
            // Return the identifier name itself
            return strdup(node->value);
        }

        // ===== PROGRAM AND DECLARATIONS =====
        
        case NODE_PROGRAM:
        case NODE_DECLARATION_SPECIFIERS:
        case NODE_TYPE_SPECIFIER:
        case NODE_STORAGE_CLASS_SPECIFIER:
        case NODE_TYPE_QUALIFIER:
        {
            // Just process children
            for (int i = 0; i < node->childCount; i++) {
                generate_ir(node->children[i]);
            }
            return NULL;
        }

        case NODE_FUNCTION_DEFINITION:
        {
            // Child 0: declaration_specifiers
            // Child 1: declarator (has function name)
            // Child 2: compound_statement
            
            TreeNode* declarator = node->children[1];
            if (declarator && declarator->value) {
                char func_label[140];
                sprintf(func_label, "FUNC_%s", declarator->value);
                emit("LABEL", func_label, "", "");
                emit("FUNC_BEGIN", declarator->value, "", "");
            }
            
            // Generate code for function body
            if (node->childCount > 2) {
                generate_ir(node->children[2]);
            }
            
            if (declarator && declarator->value) {
                emit("FUNC_END", declarator->value, "", "");
            }
            
            return NULL;
        }

        case NODE_DECLARATION:
        {
            // Process initializers if any
            for (int i = 0; i < node->childCount; i++) {
                generate_ir(node->children[i]);
            }
            return NULL;
        }

        // ===== STATEMENTS =====
        
        case NODE_COMPOUND_STATEMENT:
        {
            for (int i = 0; i < node->childCount; i++) {
                generate_ir(node->children[i]);
            }
            return NULL;
        }

        case NODE_EXPRESSION_STATEMENT:
        {
            if (node->childCount > 0) {
                generate_ir(node->children[0]);
            }
            return NULL;
        }

        case NODE_SELECTION_STATEMENT: // if, if-else, switch
        {
            if (strcmp(node->value, "if") == 0) {
                // Child 0: marker (has condition)
                // Child 1: then statement
                TreeNode* marker = node->children[0];
                
                // Get condition from marker's children
                char* cond_result = NULL;
                if (marker->childCount > 0) {
                    cond_result = generate_ir(marker->children[0]);
                }
                
                char* else_label = newLabel();
                if (cond_result) {
                    emit("IF_FALSE_GOTO", cond_result, else_label, "");
                }
                
                // Then block
                if (node->childCount > 1) {
                    generate_ir(node->children[1]);
                }
                
                emit("LABEL", else_label, "", "");
            }
            else if (strcmp(node->value, "if_else") == 0) {
                // Child 0: marker (has condition)
                // Child 1: then statement
                // Child 2: else statement
                TreeNode* marker = node->children[0];
                
                char* cond_result = NULL;
                if (marker->childCount > 0) {
                    cond_result = generate_ir(marker->children[0]);
                }
                
                char* else_label = newLabel();
                char* end_label = newLabel();
                
                if (cond_result) {
                    emit("IF_FALSE_GOTO", cond_result, else_label, "");
                }
                
                // Then block
                if (node->childCount > 1) {
                    generate_ir(node->children[1]);
                }
                
                emit("GOTO", end_label, "", "");
                emit("LABEL", else_label, "", "");
                
                // Else block
                if (node->childCount > 2) {
                    generate_ir(node->children[2]);
                }
                
                emit("LABEL", end_label, "", "");
            }
            else if (strcmp(node->value, "switch") == 0) {
                // Child 0: expression
                // Child 1: statement
                char* switch_expr = generate_ir(node->children[0]);
                char* switch_end = newLabel();
                
                pushSwitchLabel(switch_end);
                
                if (switch_expr) {
                    emit("SWITCH", switch_expr, "", "");
                }
                
                // Generate switch body
                if (node->childCount > 1) {
                    generate_ir(node->children[1]);
                }
                
                emit("LABEL", switch_end, "", "");
                popSwitchLabel();
            }
            return NULL;
        }

        case NODE_ITERATION_STATEMENT:
        {
            if (strcmp(node->value, "while") == 0) {
                // Child 0: expression (condition)
                // Child 1: statement (body)
                
                char* start_label = newLabel();
                char* end_label = newLabel();
                
                pushLoopLabels(start_label, end_label);
                
                emit("LABEL", start_label, "", "");
                
                char* cond_result = generate_ir(node->children[0]);
                if (cond_result) {
                    emit("IF_FALSE_GOTO", cond_result, end_label, "");
                }
                
                // Loop body
                if (node->childCount > 1) {
                    generate_ir(node->children[1]);
                }
                
                emit("GOTO", start_label, "", "");
                emit("LABEL", end_label, "", "");
                
                popLoopLabels();
            }
            else if (strcmp(node->value, "do_while") == 0 || strcmp(node->value, "do_until") == 0) {
                // Child 0: marker
                // Child 1: statement (body)
                // Child 2: expression (condition)
                
                char* start_label = newLabel();
                char* end_label = newLabel();
                
                pushLoopLabels(start_label, end_label);
                
                emit("LABEL", start_label, "", "");
                
                // Loop body
                if (node->childCount > 1) {
                    generate_ir(node->children[1]);
                }
                
                // Condition
                char* cond_result = NULL;
                if (node->childCount > 2) {
                    cond_result = generate_ir(node->children[2]);
                }
                
                if (cond_result) {
                    if (strcmp(node->value, "do_while") == 0) {
                        emit("IF_TRUE_GOTO", cond_result, start_label, "");
                    } else {
                        emit("IF_FALSE_GOTO", cond_result, start_label, "");
                    }
                }
                
                emit("LABEL", end_label, "", "");
                popLoopLabels();
            }
            else if (strcmp(node->value, "for") == 0) {
                // Child 0: init
                // Child 1: condition
                // Child 2: increment
                // Child 3: body
                
                char* cond_label = newLabel();
                char* incr_label = newLabel();
                char* end_label = newLabel();
                
                // Init
                if (node->childCount > 0 && node->children[0]->type != NODE_EXPRESSION_STATEMENT) {
                    generate_ir(node->children[0]);
                }
                
                emit("LABEL", cond_label, "", "");
                
                pushLoopLabels(incr_label, end_label);
                
                // Condition
                if (node->childCount > 1 && node->children[1]) {
                    char* cond_result = generate_ir(node->children[1]);
                    if (cond_result) {
                        emit("IF_FALSE_GOTO", cond_result, end_label, "");
                    }
                }
                
                // Body
                if (node->childCount > 3) {
                    generate_ir(node->children[3]);
                }
                
                emit("LABEL", incr_label, "", "");
                
                // Increment
                if (node->childCount > 2 && node->children[2]) {
                    generate_ir(node->children[2]);
                }
                
                emit("GOTO", cond_label, "", "");
                emit("LABEL", end_label, "", "");
                
                popLoopLabels();
            }
            return NULL;
        }

        case NODE_JUMP_STATEMENT:
        {
            if (strcmp(node->value, "goto") == 0) {
                if (node->childCount > 0 && node->children[0]->value) {
                    emit("GOTO", node->children[0]->value, "", "");
                }
            }
            else if (strcmp(node->value, "continue") == 0) {
                char* continue_label = getCurrentLoopContinue();
                if (continue_label) {
                    emit("GOTO", continue_label, "", "");
                }
            }
            else if (strcmp(node->value, "break") == 0) {
                char* break_label = getCurrentLoopBreak();
                if (!break_label) {
                    break_label = getCurrentSwitchEnd();
                }
                if (break_label) {
                    emit("GOTO", break_label, "", "");
                }
            }
            else if (strcmp(node->value, "return") == 0) {
                if (node->childCount > 0) {
                    char* ret_val = generate_ir(node->children[0]);
                    emit("RETURN", ret_val, "", "");
                } else {
                    emit("RETURN", "", "", "");
                }
            }
            return NULL;
        }

        case NODE_LABELED_STATEMENT:
        {
            if (strcmp(node->value, "label") == 0) {
                if (node->childCount > 0 && node->children[0]->value) {
                    emit("LABEL", node->children[0]->value, "", "");
                }
                if (node->childCount > 1) {
                    generate_ir(node->children[1]);
                }
            }
            else if (strcmp(node->value, "case") == 0) {
                if (node->childCount > 0) {
                    char* case_val = generate_ir(node->children[0]);
                    if (case_val) {
                        char case_label[128];
                        sprintf(case_label, "CASE_%s", case_val);
                        emit("LABEL", case_label, "", "");
                    }
                }
                if (node->childCount > 1) {
                    generate_ir(node->children[1]);
                }
            }
            else if (strcmp(node->value, "default") == 0) {
                emit("LABEL", "DEFAULT", "", "");
                if (node->childCount > 0) {
                    generate_ir(node->children[0]);
                    }
            }
            return NULL;
        }

        // ===== EXPRESSIONS =====
        
        case NODE_EXPRESSION:
        {
            // Comma operator - evaluate all, return last
            char* result = NULL;
            for (int i = 0; i < node->childCount; i++) {
                result = generate_ir(node->children[i]);
            }
            return result;
        }

        case NODE_ASSIGNMENT_EXPRESSION:
        {
            if (strcmp(node->value, "=") == 0) {
                // Child 0: lhs (identifier)
                // Child 1: rhs (expression)
                
                char* lhs_name = node->children[0]->value;
                char* rhs_result = generate_ir(node->children[1]);
                
                // Handle type conversion if needed
                if (node->children[0]->dataType && node->children[1]->dataType) {
                    rhs_result = convertType(rhs_result, node->children[1]->dataType, node->children[0]->dataType);
                }
                
                emit("ASSIGN", rhs_result, "", lhs_name);
                return lhs_name;
            }
            else if (strcmp(node->value, "+=") == 0) {
                char* lhs = node->children[0]->value;
                char* rhs = generate_ir(node->children[1]);
                emit("ADD", lhs, rhs, lhs);
                return lhs;
            }
            else if (strcmp(node->value, "-=") == 0) {
                char* lhs = node->children[0]->value;
                char* rhs = generate_ir(node->children[1]);
                emit("SUB", lhs, rhs, lhs);
                return lhs;
            }
            else if (strcmp(node->value, "*=") == 0) {
                char* lhs = node->children[0]->value;
                char* rhs = generate_ir(node->children[1]);
                emit("MUL", lhs, rhs, lhs);
                return lhs;
            }
            else if (strcmp(node->value, "/=") == 0) {
                char* lhs = node->children[0]->value;
                char* rhs = generate_ir(node->children[1]);
                emit("DIV", lhs, rhs, lhs);
                return lhs;
            }
            else if (strcmp(node->value, "%=") == 0) {
                char* lhs = node->children[0]->value;
                char* rhs = generate_ir(node->children[1]);
                emit("MOD", lhs, rhs, lhs);
                return lhs;
            }
            else if (strcmp(node->value, "&=") == 0) {
                char* lhs = node->children[0]->value;
                char* rhs = generate_ir(node->children[1]);
                emit("BITAND", lhs, rhs, lhs);
                return lhs;
            }
            else if (strcmp(node->value, "|=") == 0) {
                char* lhs = node->children[0]->value;
                char* rhs = generate_ir(node->children[1]);
                emit("BITOR", lhs, rhs, lhs);
                return lhs;
            }
            else if (strcmp(node->value, "^=") == 0) {
                char* lhs = node->children[0]->value;
                char* rhs = generate_ir(node->children[1]);
                emit("BITXOR", lhs, rhs, lhs);
                return lhs;
            }
            else if (strcmp(node->value, "<<=") == 0) {
                char* lhs = node->children[0]->value;
                char* rhs = generate_ir(node->children[1]);
                emit("LSHIFT", lhs, rhs, lhs);
                return lhs;
            }
            else if (strcmp(node->value, ">>=") == 0) {
                char* lhs = node->children[0]->value;
                char* rhs = generate_ir(node->children[1]);
                emit("RSHIFT", lhs, rhs, lhs);
                return lhs;
            }
            return NULL;
        }

        case NODE_CONDITIONAL_EXPRESSION:
        {
            // Ternary: condition ? true_expr : false_expr
            if (node->childCount >= 3) {
                char* cond = generate_ir(node->children[0]);
                char* true_val = generate_ir(node->children[1]);
                char* false_val = generate_ir(node->children[2]);
                
                char* temp = newTemp();
                emit("TERNARY", cond, true_val, temp);
                // Note: Full ternary implementation would need labels
                return temp;
            }
            return NULL;
        }

        case NODE_LOGICAL_OR_EXPRESSION:
        {
            char* left = generate_ir(node->children[0]);
            char* right = generate_ir(node->children[1]);
            char* temp = newTemp();
            emit("OR", left, right, temp);
            return temp;
        }

        case NODE_LOGICAL_AND_EXPRESSION:
        {
            char* left = generate_ir(node->children[0]);
            char* right = generate_ir(node->children[1]);
            char* temp = newTemp();
            emit("AND", left, right, temp);
            return temp;
        }

        case NODE_INCLUSIVE_OR_EXPRESSION:
        {
            char* left = generate_ir(node->children[0]);
            char* right = generate_ir(node->children[1]);
            char* temp = newTemp();
            emit("BITOR", left, right, temp);
            return temp;
        }

        case NODE_EXCLUSIVE_OR_EXPRESSION:
        {
            char* left = generate_ir(node->children[0]);
            char* right = generate_ir(node->children[1]);
            char* temp = newTemp();
            emit("BITXOR", left, right, temp);
            return temp;
        }

        case NODE_AND_EXPRESSION:
        {
            char* left = generate_ir(node->children[0]);
            char* right = generate_ir(node->children[1]);
            char* temp = newTemp();
            emit("BITAND", left, right, temp);
            return temp;
        }

        case NODE_EQUALITY_EXPRESSION:
        {
            char* left = generate_ir(node->children[0]);
            char* right = generate_ir(node->children[1]);
            char* temp = newTemp();
            
            if (strcmp(node->value, "==") == 0) {
                emit("EQ", left, right, temp);
            } else if (strcmp(node->value, "!=") == 0) {
                emit("NE", left, right, temp);
            }
            return temp;
        }

        case NODE_RELATIONAL_EXPRESSION:
        {
            char* left = generate_ir(node->children[0]);
            char* right = generate_ir(node->children[1]);
            char* temp = newTemp();
            
            if (strcmp(node->value, "<") == 0) {
                emit("LT", left, right, temp);
            } else if (strcmp(node->value, ">") == 0) {
                emit("GT", left, right, temp);
            } else if (strcmp(node->value, "<=") == 0) {
                emit("LE", left, right, temp);
            } else if (strcmp(node->value, ">=") == 0) {
                emit("GE", left, right, temp);
            }
            return temp;
        }

        case NODE_SHIFT_EXPRESSION:
        {
            char* left = generate_ir(node->children[0]);
            char* right = generate_ir(node->children[1]);
            char* temp = newTemp();
            
            if (strcmp(node->value, "<<") == 0) {
                emit("LSHIFT", left, right, temp);
            } else if (strcmp(node->value, ">>") == 0) {
                emit("RSHIFT", left, right, temp);
            }
            return temp;
        }

        case NODE_ADDITIVE_EXPRESSION:
        {
            char* left = generate_ir(node->children[0]);
            char* right = generate_ir(node->children[1]);
            
            // Handle type conversions if needed
            if (node->children[0]->dataType && node->children[1]->dataType) {
                char* result_type = usualArithConv(node->children[0]->dataType, node->children[1]->dataType);
                left = convertType(left, node->children[0]->dataType, result_type);
                right = convertType(right, node->children[1]->dataType, result_type);
            }
            
            char* temp = newTemp();
            
            if (strcmp(node->value, "+") == 0) {
                emit("ADD", left, right, temp);
            } else if (strcmp(node->value, "-") == 0) {
                emit("SUB", left, right, temp);
            }
            return temp;
        }

        case NODE_MULTIPLICATIVE_EXPRESSION:
        {
            char* left = generate_ir(node->children[0]);
            char* right = generate_ir(node->children[1]);
            
            // Handle type conversions if needed
            if (node->children[0]->dataType && node->children[1]->dataType) {
                char* result_type = usualArithConv(node->children[0]->dataType, node->children[1]->dataType);
                left = convertType(left, node->children[0]->dataType, result_type);
                right = convertType(right, node->children[1]->dataType, result_type);
            }
            
            char* temp = newTemp();
            
            if (strcmp(node->value, "*") == 0) {
                emit("MUL", left, right, temp);
            } else if (strcmp(node->value, "/") == 0) {
                emit("DIV", left, right, temp);
            } else if (strcmp(node->value, "%") == 0) {
                emit("MOD", left, right, temp);
            }
            return temp;
        }

        case NODE_CAST_EXPRESSION:
        {
            // Just process the expression being cast
            if (node->childCount > 0) {
                return generate_ir(node->children[0]);
            }
            return NULL;
        }

        case NODE_UNARY_EXPRESSION:
        {
            if (strcmp(node->value, "++") == 0) {
                // Pre-increment
                char* operand = node->children[0]->value;
                char* temp = newTemp();
                emit("ADD", operand, "1", temp);
                emit("ASSIGN", temp, "", operand);
                return operand;
            }
            else if (strcmp(node->value, "--") == 0) {
                // Pre-decrement
                char* operand = node->children[0]->value;
                char* temp = newTemp();
                emit("SUB", operand, "1", temp);
                emit("ASSIGN", temp, "", operand);
                return operand;
            }
            else if (strcmp(node->value, "&") == 0) {
                // Address-of
                char* operand = generate_ir(node->children[0]);
                char* temp = newTemp();
                emit("ADDR", operand, "", temp);
                return temp;
            }
            else if (strcmp(node->value, "*") == 0) {
                // Dereference
                char* operand = generate_ir(node->children[0]);
                char* temp = newTemp();
                emit("DEREF", operand, "", temp);
                return temp;
            }
            else if (strcmp(node->value, "+") == 0) {
                // Unary plus - just return operand
                return generate_ir(node->children[0]);
            }
            else if (strcmp(node->value, "-") == 0) {
                // Unary minus
                char* operand = generate_ir(node->children[0]);
                char* temp = newTemp();
                emit("NEG", operand, "", temp);
                return temp;
            }
            else if (strcmp(node->value, "~") == 0) {
                // Bitwise NOT
                char* operand = generate_ir(node->children[0]);
                char* temp = newTemp();
                emit("BITNOT", operand, "", temp);
                return temp;
            }
            else if (strcmp(node->value, "!") == 0) {
                // Logical NOT
                char* operand = generate_ir(node->children[0]);
                char* temp = newTemp();
                emit("NOT", operand, "", temp);
                return temp;
            }
            else if (strcmp(node->value, "sizeof") == 0) {
                // Sizeof
                char* temp = newTemp();
                char size_str[32];
                
                if (node->children[0]->dataType) {
                    sprintf(size_str, "%d", getTypeSize(node->children[0]->dataType));
                } else {
                    sprintf(size_str, "4");
                }
                
                emit("ASSIGN", size_str, "", temp);
                return temp;
            }
            return NULL;
        }

        case NODE_POSTFIX_EXPRESSION:
        {
            if (strcmp(node->value, "[]") == 0) {
                // Array access
                char* array = generate_ir(node->children[0]);
                char* index = generate_ir(node->children[1]);
                char* temp = newTemp();
                emit("ARRAY_ACCESS", array, index, temp);
                return temp;
            }
            else if (strcmp(node->value, "()") == 0) {
                // Function call
                char* func_name = node->children[0]->value;
                
                // Process arguments
                int arg_count = 0;
                if (node->childCount > 1) {
                    TreeNode* args = node->children[1];
                    for (int i = 0; i < args->childCount; i++) {
                        char* arg = generate_ir(args->children[i]);
                        emit("ARG", arg, "", "");
                        arg_count++;
                    }
                }
                
                char* temp = newTemp();
                char arg_count_str[32];
                sprintf(arg_count_str, "%d", arg_count);
                emit("CALL", func_name, arg_count > 0 ? "" : "0", temp);
                return temp;
            }
            else if (strcmp(node->value, ".") == 0) {
                // Member access
                char* struct_var = generate_ir(node->children[0]);
                char* member = node->children[1]->value;
                char* temp = newTemp();
                emit("MEMBER_ACCESS", struct_var, member, temp);
                return temp;
            }
            else if (strcmp(node->value, "->") == 0) {
                // Arrow access
                char* struct_ptr = generate_ir(node->children[0]);
                char* member = node->children[1]->value;
                char* temp = newTemp();
                emit("ARROW_ACCESS", struct_ptr, member, temp);
                return temp;
            }
            else if (strcmp(node->value, "++") == 0) {
                // Post-increment
                char* operand = node->children[0]->value;
                char* temp = newTemp();
                emit("ASSIGN", operand, "", temp);
                char* temp2 = newTemp();
                emit("ADD", operand, "1", temp2);
                emit("ASSIGN", temp2, "", operand);
                return temp;
            }
            else if (strcmp(node->value, "--") == 0) {
                // Post-decrement
                char* operand = node->children[0]->value;
                char* temp = newTemp();
                emit("ASSIGN", operand, "", temp);
                char* temp2 = newTemp();
                emit("SUB", operand, "1", temp2);
                emit("ASSIGN", temp2, "", operand);
                return temp;
            }
            return NULL;
        }

        case NODE_PRIMARY_EXPRESSION:
        {
            // This should have been handled by specific cases above
            if (node->childCount > 0) {
                return generate_ir(node->children[0]);
            }
            return node->value ? strdup(node->value) : NULL;
        }

        case NODE_INITIALIZER:
        {
            // Process initializer
            if (node->childCount > 0) {
                return generate_ir(node->children[0]);
            }
            return NULL;
        }

        case NODE_MARKER:
        {
            // Markers are used during parsing, just process children
            for (int i = 0; i < node->childCount; i++) {
                generate_ir(node->children[i]);
            }
            return NULL;
        }

        // ===== DEFAULT =====
        
        default:
        {
            // For any other node type, just process children
            for (int i = 0; i < node->childCount; i++) {
                generate_ir(node->children[i]);
            }
            return NULL;
        }
    }
}