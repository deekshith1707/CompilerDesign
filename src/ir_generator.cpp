#include "ir_generator.h"
#include "ir_context.h"
#include "symbol_table.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>

using namespace std;

// Struct to hold case label info
struct CaseLabel {
    char* value;
    char* label;
};

// Helper function to emit appropriate conditional jump based on data type
static void emitConditionalJump(const char* op, const char* operand, const char* label, const char* dataType) {
    if (dataType && strcmp(dataType, "float") == 0) {
        if (strcmp(op, "IF_FALSE_GOTO") == 0) {
            emit("IF_FALSE_GOTO_FLOAT", operand, label, "");
        } else if (strcmp(op, "IF_TRUE_GOTO") == 0) {
            emit("IF_TRUE_GOTO_FLOAT", operand, label, "");
        } else {
            emit(op, operand, label, "");
        }
    } else {
        emit(op, operand, label, "");
    }
}

// Control flow stacks
#define MAX_LOOP_DEPTH 100
static LoopContext loopStack[MAX_LOOP_DEPTH];
static int loopDepth = 0;
static SwitchContext switchStack[MAX_LOOP_DEPTH];
static int switchDepth = 0;
static int switchCount = 0;

// Helper functions for control flow
static void pushLoopLabels(char* continue_label, char* break_label) {
    if (loopDepth >= MAX_LOOP_DEPTH) {
        cerr << "Error: Loop nesting too deep" << endl;
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

static void pushSwitchLabel(char* end_label, char* default_label) {
    if (switchDepth >= MAX_LOOP_DEPTH) {
        cerr << "Error: Switch nesting too deep" << endl;
        return;
    }
    switchStack[switchDepth].switch_id = switchCount++;
    switchStack[switchDepth].end_label = end_label;
    switchStack[switchDepth].default_label = default_label;
    switchDepth++;
}

static void popSwitchLabel() {
    if (switchDepth > 0) switchDepth--;
}

static char* getCurrentSwitchEnd() {
    return (switchDepth > 0) ? switchStack[switchDepth - 1].end_label : NULL;
}

static int getCurrentSwitchId() {
    return (switchDepth > 0) ? switchStack[switchDepth - 1].switch_id : -1;
}

static char* getCurrentSwitchDefault() {
    return (switchDepth > 0) ? switchStack[switchDepth - 1].default_label : NULL;
}

static char* get_constant_value_from_expression(TreeNode* node) {
    if (!node) return NULL;
    
    switch (node->type) {
        case NODE_CONSTANT:
        case NODE_INTEGER_CONSTANT:
        case NODE_HEX_CONSTANT:
        case NODE_OCTAL_CONSTANT:
        case NODE_BINARY_CONSTANT:
        case NODE_CHAR_CONSTANT:
            return node->value;
    }
    
    // Recurse down for wrapper nodes
    if (node->childCount == 1) {
        return get_constant_value_from_expression(node->children[0]);
    }
    
    return NULL; // Not a simple constant
}

static void find_case_labels(TreeNode* node, std::vector<CaseLabel>& cases, char** default_label, int switch_id) {
    if (!node) return;

    if (node->type == NODE_LABELED_STATEMENT) {
        if (strcmp(node->value, "case") == 0 && node->childCount > 0) {
            char* const_val = get_constant_value_from_expression(node->children[0]);
            if (const_val) {
                char label_name[128];
                sprintf(label_name, "SWITCH_%d_CASE_%s", switch_id, const_val);
                cases.push_back({strdup(const_val), strdup(label_name)});
            }
            // Continue searching in the statement *after* the label
            if (node->childCount > 1) {
                find_case_labels(node->children[1], cases, default_label, switch_id);
            }
            return; // Stop recursion down this branch, we've handled it.
        }
        else if (strcmp(node->value, "default") == 0) {
            char default_name[128];
            sprintf(default_name, "SWITCH_%d_DEFAULT", switch_id);
            *default_label = strdup(default_name);
            // Continue searching in the statement *after* the label
            if (node->childCount > 0) {
                find_case_labels(node->children[0], cases, default_label, switch_id);
            }
            return; // Stop recursion
        }
    }

    // Don't traverse into nested switches
    if (node->type == NODE_SELECTION_STATEMENT && strcmp(node->value, "switch") == 0) {
        return;
    }

    // Recurse
    for (int i = 0; i < node->childCount; i++) {
        find_case_labels(node->children[i], cases, default_label, switch_id);
    }
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
            // Return the constant value directly - don't create unnecessary temporaries
            return strdup(node->value);
        }
        
        case NODE_IDENTIFIER:
        {
            // Process children, if any (for declarator lists)
            for (int i = 0; i < node->childCount; i++) {
                generate_ir(node->children[i]);
            }
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
            // Process all children, handling multiple declarators properly
            for (int i = 0; i < node->childCount; i++) {
                TreeNode* child = node->children[i];
                
                // Skip declaration_specifiers (type info), process init_declarator_list
                if (child && i > 0) {  // Skip first child which is usually declaration_specifiers
                    // Check if this is an init_declarator_list or single init_declarator
                    if (child->childCount > 0) {
                        // Process all declarators in the list
                        for (int j = 0; j < child->childCount; j++) {
                            TreeNode* declarator = child->children[j];
                            if (declarator && declarator->type == NODE_INITIALIZER) {
                                generate_ir(declarator);
                            }
                        }
                    }
                    // Also process the main child if it's an initializer
                    if (child->type == NODE_INITIALIZER) {
                        generate_ir(child);
                    }
                }
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
                char* cond_result = generate_ir(node->children[0]);
                
                char* else_label = newLabel();
                if (cond_result) {
                    // Use appropriate comparison based on condition data type
                    const char* dataType = (node->children[0] && node->children[0]->dataType) ? 
                                          node->children[0]->dataType : "int";
                    emitConditionalJump("IF_FALSE_GOTO", cond_result, else_label, dataType);
                }
                
                // Then block
                if (node->childCount > 1) {
                    generate_ir(node->children[1]);
                }
                
                emit("LABEL", else_label, "", "");
            }
            else if (strcmp(node->value, "if_else") == 0) {
                char* cond_result = generate_ir(node->children[0]);
                
                char* else_label = newLabel();
                char* end_label = newLabel();
                
                if (cond_result) {
                    // Use appropriate comparison based on condition data type
                    const char* dataType = (node->children[0] && node->children[0]->dataType) ? 
                                          node->children[0]->dataType : "int";
                    emitConditionalJump("IF_FALSE_GOTO", cond_result, else_label, dataType);
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
                
                // 1. Generate code for the switch expression
                char* switch_expr = generate_ir(node->children[0]);
                char* switch_end = newLabel();
                
                // Get the current switch ID (before pushing)
                int current_switch_id = switchCount;
                
                // === PASS 1: Collect all case/default labels ===
                std::vector<CaseLabel> case_labels;
                char* default_label = NULL;
                
                if (node->childCount > 1) {
                    find_case_labels(node->children[1], case_labels, &default_label, current_switch_id);
                }
                
                char* final_default_label = default_label ? default_label : switch_end;
                
                // Push switch context with proper default label
                pushSwitchLabel(switch_end, final_default_label);
                
                // === GENERATE DISPATCH LOGIC ===
                for (const auto& cl : case_labels) {
                    // Compare switch_expr == case_value
                    char* temp_const = newTemp();
                    emit("ASSIGN", cl.value, "", temp_const);
                    char* temp_cmp = newTemp();
                    emit("EQ", switch_expr, temp_const, temp_cmp);
                    // If true, jump to the case's label
                    emit("IF_TRUE_GOTO", temp_cmp, cl.label, "");
                }
                
                // 4. If no cases match, jump to default (or end)
                emit("GOTO", final_default_label, "", "");
                
                // === PASS 2: Generate switch body ===
                // This will now just emit the labels and the code inside them
                if (node->childCount > 1) {
                    generate_ir(node->children[1]);
                }
                
                // 6. Emit the final end label
                emit("LABEL", switch_end, "", "");
                popSwitchLabel();

                // Free allocated memory
                for (auto& cl : case_labels) {
                    free(cl.value);
                    free(cl.label);
                }
                if (default_label) {
                    free(default_label);
                }
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
                // FIX for Bug 2: Correct do-while logic
                // AST from parser:
                // Child 0: expression (condition)
                // Child 1: marker
                // Child 2: statement (body)
                
                char* start_label = newLabel();
                char* test_label = newLabel();  // New label for the condition test
                char* end_label = newLabel();
                
                // 'continue' in do-while jumps to the test (test_label)
                // 'break' jumps to the end (end_label)
                pushLoopLabels(test_label, end_label); 
                
                emit("LABEL", start_label, "", "");
                
                // Loop body (Child 2)
                if (node->childCount > 2) {
                    generate_ir(node->children[2]); // This will generate the printf AND k++
                }
                
                // 'continue' statements will jump here
                emit("LABEL", test_label, "", ""); 
                
                // Condition (Child 0)
                char* cond_result = NULL;
                if (node->childCount > 0) { 
                    cond_result = generate_ir(node->children[0]); // This generates (k < 3)
                }
                
                if (cond_result) {
                    if (strcmp(node->value, "do_while") == 0) {
                        // if (k < 3) is true, go back to start
                        emit("IF_TRUE_GOTO", cond_result, start_label, "");
                    } else {
                        // if (k < 3) is false, go back to start
                        emit("IF_FALSE_GOTO", cond_result, start_label, "");
                    }
                }
                
                // 'break' statements will jump here
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
                
                // Init - handle both declarations and expressions
                if (node->childCount > 0 && node->children[0]) {
                    // Check if it's not an empty expression statement
                    if (!(node->children[0]->type == NODE_EXPRESSION_STATEMENT && 
                          (!node->children[0]->value || strlen(node->children[0]->value) == 0))) {
                        generate_ir(node->children[0]);
                    }
                }
                
                emit("LABEL", cond_label, "", "");
                
                pushLoopLabels(incr_label, end_label);
                
                // Condition - handle empty condition (infinite loop)
                if (node->childCount > 1 && node->children[1]) {
                    // Check if condition is not empty
                    if (!(node->children[1]->type == NODE_EXPRESSION_STATEMENT && 
                          (!node->children[1]->value || strlen(node->children[1]->value) == 0))) {
                        char* cond_result = generate_ir(node->children[1]);
                        if (cond_result && strlen(cond_result) > 0) {
                            emit("IF_FALSE_GOTO", cond_result, end_label, "");
                        }
                    }
                    // If condition is empty, no conditional jump - infinite loop until break
                }
                
                // Body
                if (node->childCount > 3) {
                    generate_ir(node->children[3]);
                }
                
                emit("LABEL", incr_label, "", "");
                
                // Increment - handle empty increment
                if (node->childCount > 2 && node->children[2]) {
                    // Check if increment is not empty
                    if (!(node->children[2]->type == NODE_EXPRESSION_STATEMENT && 
                          (!node->children[2]->value || strlen(node->children[2]->value) == 0))) {
                        generate_ir(node->children[2]);
                    }
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
                // The dispatch logic is now handled by the 'switch' block.
                // Here, we just need to find the constant value and emit the LABEL.
                if (node->childCount > 0) {
                    char* const_val = get_constant_value_from_expression(node->children[0]);
                    if (const_val) {
                        int switch_id = getCurrentSwitchId();
                        if (switch_id >= 0) {
                            char case_label[128];
                            sprintf(case_label, "SWITCH_%d_CASE_%s", switch_id, const_val);
                            emit("LABEL", case_label, "", "");
                        } else {
                            // Fallback to old behavior if no switch context
                            char case_label[128];
                            sprintf(case_label, "CASE_%s", const_val);
                            emit("LABEL", case_label, "", "");
                        }
                    } else {
                        // This is an error, case expressions must be constant
                        cerr << "Error: case expression is not a simple constant." << endl;
                    }
                }
                // Generate the statement(s) for this case
                if (node->childCount > 1) {
                    generate_ir(node->children[1]);
                }
            }
            else if (strcmp(node->value, "default") == 0) {
                int switch_id = getCurrentSwitchId();
                if (switch_id >= 0) {
                    char default_label[128];
                    sprintf(default_label, "SWITCH_%d_DEFAULT", switch_id);
                    emit("LABEL", default_label, "", "");
                } else {
                    // Fallback to old behavior if no switch context
                    emit("LABEL", "DEFAULT", "", "");
                }
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
                // Child 0: lhs (could be identifier, member access, array access, etc.)
                // Child 1: rhs (expression)
                
                // Generate RHS first
                char* rhs_result = generate_ir(node->children[1]);
                
                // Handle type conversion if needed
                if (node->children[0]->dataType && node->children[1]->dataType) {
                    rhs_result = convertType(rhs_result, node->children[1]->dataType, node->children[0]->dataType);
                }
                
                // Handle different types of LHS
                TreeNode* lhs = node->children[0];
                if (lhs->type == NODE_IDENTIFIER) {
                    // Simple variable assignment: x = rhs
                    emit("ASSIGN", rhs_result, "", lhs->value);
                    return lhs->value;
                }
                else if (lhs->type == NODE_POSTFIX_EXPRESSION && strcmp(lhs->value, ".") == 0) {
                    // Struct member assignment: struct.member = rhs
                    char* struct_var = lhs->children[0]->value;
                    char* member = lhs->children[1]->value;
                    emit("ASSIGN_MEMBER", member, struct_var, rhs_result);
                    return struct_var;
                }
                else if (lhs->type == NODE_POSTFIX_EXPRESSION && strcmp(lhs->value, "->") == 0) {
                    // Pointer member assignment: ptr->member = rhs
                    char* struct_ptr = generate_ir(lhs->children[0]);
                    char* member = lhs->children[1]->value;
                    emit("ASSIGN_ARROW", member, struct_ptr, rhs_result);
                    return struct_ptr;
                }
                else if (lhs->type == NODE_POSTFIX_EXPRESSION && strcmp(lhs->value, "[]") == 0) {
                    // Array assignment: arr[index] = rhs
                    char* array = generate_ir(lhs->children[0]);
                    char* index = generate_ir(lhs->children[1]);
                    emit("ASSIGN_ARRAY", index, array, rhs_result); // Fixed parameter order
                    return array;
                }
                else if (lhs->type == NODE_UNARY_EXPRESSION && strcmp(lhs->value, "*") == 0) {
                    // Pointer dereference assignment: *ptr = rhs
                    char* ptr = generate_ir(lhs->children[0]);
                    emit("ASSIGN_DEREF", rhs_result, ptr, "");
                    return ptr;
                }
                else {
                    // Fallback: generate LHS and try direct assignment
                    char* lhs_result = generate_ir(lhs);
                    if (lhs_result) {
                        emit("ASSIGN", rhs_result, "", lhs_result);
                        return lhs_result;
                    }
                }
                return rhs_result;
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
            // Implement proper short-circuit evaluation
            char* result_temp = newTemp();
            char* true_label = newLabel();
            char* end_label = newLabel();
            
            // 1. Evaluate left side
            char* left = generate_ir(node->children[0]);
            
            // 2. Short-circuit: if left is true, set result to 1 and skip right
            const char* leftType = (node->children[0] && node->children[0]->dataType) ? 
                                  node->children[0]->dataType : "int";
            emitConditionalJump("IF_TRUE_GOTO", left, true_label, leftType);
            
            // 3. Left was false, evaluate right side
            char* right = generate_ir(node->children[1]);
            
            // 4. Set result based on right side
            const char* rightType = (node->children[1] && node->children[1]->dataType) ? 
                                   node->children[1]->dataType : "int";
            emitConditionalJump("IF_TRUE_GOTO", right, true_label, rightType);
            
            // 5. Both were false: assign 0
            emit("ASSIGN", "0", "", result_temp);
            emit("GOTO", end_label, "", "");
            
            // 6. True label: assign 1
            emit("LABEL", true_label, "", "");
            emit("ASSIGN", "1", "", result_temp);
            
            // 7. End label
            emit("LABEL", end_label, "", "");
            
            return result_temp;
        }

        case NODE_LOGICAL_AND_EXPRESSION:
        {
            // Implement proper short-circuit evaluation  
            char* result_temp = newTemp();
            char* false_label = newLabel();
            char* end_label = newLabel();
            
            // 1. Evaluate left side
            char* left = generate_ir(node->children[0]);
            
            // 2. Short-circuit: if left is false, set result to 0 and skip right
            const char* leftType = (node->children[0] && node->children[0]->dataType) ? 
                                  node->children[0]->dataType : "int";
            emitConditionalJump("IF_FALSE_GOTO", left, false_label, leftType);
            
            // 3. Left was true, evaluate right side
            char* right = generate_ir(node->children[1]);
            
            // 4. Set result based on right side
            const char* rightType = (node->children[1] && node->children[1]->dataType) ? 
                                   node->children[1]->dataType : "int";
            emitConditionalJump("IF_FALSE_GOTO", right, false_label, rightType);
            
            // 5. Both were true: assign 1
            emit("ASSIGN", "1", "", result_temp);
            emit("GOTO", end_label, "", "");
            
            // 6. False label: assign 0
            emit("LABEL", false_label, "", "");
            emit("ASSIGN", "0", "", result_temp);
            
            // 7. End label
            emit("LABEL", end_label, "", "");
            
            return result_temp;
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
            
            // Special handling for pointer arithmetic
            bool is_pointer_arithmetic = false;
            if (node->children[0]->dataType && node->children[1]->dataType) {
                // Check if this is pointer + integer or array + integer
                // Simple pointer check: contains '*'
                bool is_pointer = (strchr(node->children[0]->dataType, '*') != nullptr);
                if ((is_pointer || isArrayType(node->children[0]->dataType)) &&
                    isIntegerType(node->children[1]->dataType)) {
                    is_pointer_arithmetic = true;
                }
            }
            
            char* temp = newTemp();
            
            if (strcmp(node->value, "+") == 0) {
                if (is_pointer_arithmetic) {
                    // For pointer arithmetic, use address computation instead of type casting
                    // For arrays, use address of first element
                    if (isArrayType(node->children[0]->dataType)) {
                        char* addr_temp = newTemp();
                        emit("ADDR", left, "", addr_temp);  // Get address of array start
                        emit("PTR_ADD", addr_temp, right, temp);  // Proper pointer arithmetic
                    } else {
                        emit("PTR_ADD", left, right, temp);  // Proper pointer arithmetic
                    }
                } else {
                    // Handle regular arithmetic type conversions
                    if (node->children[0]->dataType && node->children[1]->dataType) {
                        char* result_type = usualArithConv(node->children[0]->dataType, node->children[1]->dataType);
                        left = convertType(left, node->children[0]->dataType, result_type);
                        right = convertType(right, node->children[1]->dataType, result_type);
                    }
                    emit("ADD", left, right, temp);
                }
            } else if (strcmp(node->value, "-") == 0) {
                if (is_pointer_arithmetic) {
                    emit("PTR_SUB", left, right, temp);  // Proper pointer arithmetic
                } else {
                    // Handle regular arithmetic type conversions
                    if (node->children[0]->dataType && node->children[1]->dataType) {
                        char* result_type = usualArithConv(node->children[0]->dataType, node->children[1]->dataType);
                        left = convertType(left, node->children[0]->dataType, result_type);
                        right = convertType(right, node->children[1]->dataType, result_type);
                    }
                    emit("SUB", left, right, temp);
                }
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
            if (strcmp(node->value, "++_pre") == 0) {
                // Pre-increment - optimize to direct increment
                char* operand = node->children[0]->value;
                emit("INC", operand, "", operand);  // Direct increment without temp
                return operand;
            }
            else if (strcmp(node->value, "--_pre") == 0) {
                // Pre-decrement - optimize to direct decrement
                char* operand = node->children[0]->value;
                emit("DEC", operand, "", operand);  // Direct decrement without temp
                return operand;
            }
            else if (strcmp(node->value, "&") == 0) {
                // Address-of - special handling for array elements
                TreeNode* operand_node = node->children[0];
                
                // Check if operand is array access: &arr[index]
                if (operand_node->type == NODE_POSTFIX_EXPRESSION && 
                    strcmp(operand_node->value, "[]") == 0) {
                    // Generate: temp = arr + index (proper address arithmetic)
                    char* array = generate_ir(operand_node->children[0]);
                    char* index = generate_ir(operand_node->children[1]);
                    char* temp = newTemp();
                    emit("ARRAY_ADDR", array, index, temp);
                    return temp;
                }
                else {
                    // Regular address-of for simple variables
                    char* operand = generate_ir(operand_node);
                    char* temp = newTemp();
                    emit("ADDR", operand, "", temp);
                    return temp;
                }
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
                
                // Process arguments in reverse order for MIPS (right-to-left)
                int arg_count = 0;
                if (node->childCount > 1) {
                    TreeNode* args = node->children[1];
                    arg_count = args->childCount;
                    
                    // Push arguments right-to-left for MIPS calling convention
                    for (int i = args->childCount - 1; i >= 0; i--) {
                        char* arg = generate_ir(args->children[i]);
                        
                        // Handle float-to-double promotion for variadic functions like printf
                        if (func_name && (strcmp(func_name, "printf") == 0 || 
                                         strcmp(func_name, "fprintf") == 0 || 
                                         strcmp(func_name, "sprintf") == 0) && 
                            args->children[i]->dataType && 
                            strcmp(args->children[i]->dataType, "float") == 0) {
                            // Promote float to double for variadic functions
                            char* promoted_arg = newTemp();
                            emit("FLOAT_TO_DOUBLE", arg, "", promoted_arg);
                            emit("PARAM", promoted_arg, "", "");
                        } else {
                            emit("PARAM", arg, "", "");
                        }
                    }
                }
                
                char* temp = newTemp();
                char arg_count_str[32];
                sprintf(arg_count_str, "%d", arg_count);
                emit("CALL", func_name, arg_count_str, temp);
                return temp;
            }
            else if (strcmp(node->value, ".") == 0) {
                char* struct_var = node->children[0]->value;
                char* member = node->children[1]->value;
                char* temp = newTemp();
                emit("LOAD_MEMBER", struct_var, member, temp);
                return temp;
            }
            else if (strcmp(node->value, "->") == 0) {
                char* struct_ptr = generate_ir(node->children[0]);
                char* member = node->children[1]->value;
                char* temp = newTemp();
                emit("LOAD_ARROW", struct_ptr, member, temp);
                return temp;
            }
            else if (strcmp(node->value, "++_post") == 0) {
                char* operand = node->children[0]->value;
                char* temp = newTemp();
                emit("ASSIGN", operand, "", temp);  // Save old value
                emit("INC", operand, "", operand);  // Direct increment
                return temp;  // Return old value
            }
            else if (strcmp(node->value, "--_post") == 0) {
                char* operand = node->children[0]->value;
                char* temp = newTemp();
                emit("ASSIGN", operand, "", temp);  // Save old value
                emit("DEC", operand, "", operand);  // Direct decrement
                return temp;  // Return old value
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
            // Handle variable initialization
            if (node->childCount > 1 && strcmp(node->value, "=") == 0) {
                // Child 0: declarator (e.g., NODE_IDENTIFIER "j")
                // Child 1: initializer (e.g., NODE_INTEGER_CONSTANT "0" or initializer list)
                
                TreeNode* declarator = node->children[0];
                char* lhs_name = NULL;
                
                // Extract the actual variable name from the declarator
                if (declarator->type == NODE_IDENTIFIER) {
                    lhs_name = declarator->value;
                } else if (declarator->childCount > 0) {
                    // Look for identifier in children (for complex declarators)
                    for (int i = 0; i < declarator->childCount; i++) {
                        if (declarator->children[i]->type == NODE_IDENTIFIER) {
                            lhs_name = declarator->children[i]->value;
                            break;
                        }
                    }
                }
                
                if (!lhs_name) {
                    lhs_name = declarator->value; // fallback
                }
                
                TreeNode* rhs = node->children[1];
                
                // Check if RHS is specifically an initializer list (braced initialization)
                // This should be distinguished from function calls or other multi-child expressions
                if (rhs->childCount > 1 && rhs->type == NODE_INITIALIZER) {
                    // Array initialization: arr = {1, 2, 3}
                    for (int i = 0; i < rhs->childCount; i++) {
                        char* value = generate_ir(rhs->children[i]);
                        char index_str[32];
                        sprintf(index_str, "%d", i);
                        emit("ASSIGN_ARRAY", index_str, lhs_name, value);
                    }
                    return lhs_name;
                } else {
                    // Simple initialization (including function calls, pointer initialization, etc.)
                    char* rhs_result = generate_ir(rhs); 
                    if (lhs_name && rhs_result) {
                        emit("ASSIGN", rhs_result, "", lhs_name);
                        return lhs_name;
                    }
                }
            } 
            // Handle other initializer forms (like lists) if you have them
            else if (node->childCount > 0) {
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
