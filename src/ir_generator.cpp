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

// Track current function name for static variable naming
static char current_function_name[256] = "";

// Helper function to check if a variable is static
static int isStaticVariable(const char* var_name) {
    // During IR generation, scope context is not maintained the same way as during parsing
    // So we need to search through the symbol table manually
    extern Symbol symtab[];
    extern int symCount;
    
    // First try: exact match in current function context
    if (current_function_name[0] != '\0') {
        for (int i = 0; i < symCount; i++) {
            if (strcmp(symtab[i].name, var_name) == 0 &&
                strcmp(symtab[i].function_scope, current_function_name) == 0 &&
                symtab[i].is_static && !symtab[i].is_function) {
                return 1;
            }
        }
    }
    
    // Second try: file-scope static (no function scope)
    for (int i = 0; i < symCount; i++) {
        if (strcmp(symtab[i].name, var_name) == 0 &&
            symtab[i].function_scope[0] == '\0' &&
            symtab[i].is_static && !symtab[i].is_function) {
            return 1;
        }
    }
    
    return 0;
}

// Helper function to get the full name for a static variable
static char* getStaticVarName(const char* var_name) {
    extern Symbol symtab[];
    extern int symCount;
    
    // Search for the symbol to determine its scope
    Symbol* sym = NULL;
    
    // First try: exact match in current function context
    if (current_function_name[0] != '\0') {
        for (int i = 0; i < symCount; i++) {
            if (strcmp(symtab[i].name, var_name) == 0 &&
                strcmp(symtab[i].function_scope, current_function_name) == 0 &&
                symtab[i].is_static && !symtab[i].is_function) {
                sym = &symtab[i];
                break;
            }
        }
    }
    
    // Second try: file-scope static (no function scope)
    if (!sym) {
        for (int i = 0; i < symCount; i++) {
            if (strcmp(symtab[i].name, var_name) == 0 &&
                symtab[i].function_scope[0] == '\0' &&
                symtab[i].is_static && !symtab[i].is_function) {
                sym = &symtab[i];
                break;
            }
        }
    }
    
    if (!sym || !sym->is_static) {
        return strdup(var_name);
    }
    
    // For file-scope static variables, keep the name as-is
    if (sym->scope_level == 0 || sym->function_scope[0] == '\0') {
        return strdup(var_name);
    }
    
    // For function-scope static variables, use functionName.variableName format
    static char full_name[512];
    sprintf(full_name, "%s.%s", sym->function_scope, var_name);
    return strdup(full_name);
}

// Helper function to extract identifier name from declarator tree
static const char* extractIdentifierFromDeclarator(TreeNode* node) {
    if (!node) return NULL;
    if (node->type == NODE_IDENTIFIER) return node->value;
    
    // Traverse through pointers and other declarator constructs
    for (int i = 0; i < node->childCount; i++) {
        const char* name = extractIdentifierFromDeclarator(node->children[i]);
        if (name) return name;
    }
    return NULL;
}

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
            
            // For static variables, return the full name
            if (isStaticVariable(node->value)) {
                return getStaticVarName(node->value);
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
            char* func_name = NULL;
            if (declarator && declarator->value) {
                func_name = declarator->value;
                // Set current function name for static variable tracking
                strcpy(current_function_name, func_name);
                
                emit("FUNC_BEGIN", func_name, "", "");
            }
            
            // Generate code for function body
            if (node->childCount > 2) {
                generate_ir(node->children[2]);
            }
            
            // Add explicit return for void functions (for clarity)
            if (func_name) {
                extern Symbol* lookupSymbol(const char* name);
                Symbol* func_sym = lookupSymbol(func_name);
                if (func_sym && func_sym->is_function && strcmp(func_sym->return_type, "void") == 0) {
                    // Void function - add explicit return with no value
                    emit("RETURN", "", "", "");
                }
                
                emit("FUNC_END", func_name, "", "");
                // Clear current function name
                current_function_name[0] = '\0';
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
                    // Use full name for static variables
                    char* lhs_name = lhs->value;
                    if (isStaticVariable(lhs_name)) {
                        lhs_name = getStaticVarName(lhs_name);
                    }
                    emit("ASSIGN", rhs_result, "", lhs_name);
                    return lhs_name;
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
                if (isStaticVariable(lhs)) {
                    lhs = getStaticVarName(lhs);
                }
                char* rhs = generate_ir(node->children[1]);
                emit("ADD", lhs, rhs, lhs);
                return lhs;
            }
            else if (strcmp(node->value, "-=") == 0) {
                char* lhs = node->children[0]->value;
                if (isStaticVariable(lhs)) {
                    lhs = getStaticVarName(lhs);
                }
                char* rhs = generate_ir(node->children[1]);
                emit("SUB", lhs, rhs, lhs);
                return lhs;
            }
            else if (strcmp(node->value, "*=") == 0) {
                char* lhs = node->children[0]->value;
                if (isStaticVariable(lhs)) {
                    lhs = getStaticVarName(lhs);
                }
                char* rhs = generate_ir(node->children[1]);
                emit("MUL", lhs, rhs, lhs);
                return lhs;
            }
            else if (strcmp(node->value, "/=") == 0) {
                char* lhs = node->children[0]->value;
                if (isStaticVariable(lhs)) {
                    lhs = getStaticVarName(lhs);
                }
                char* rhs = generate_ir(node->children[1]);
                emit("DIV", lhs, rhs, lhs);
                return lhs;
            }
            else if (strcmp(node->value, "%=") == 0) {
                char* lhs = node->children[0]->value;
                if (isStaticVariable(lhs)) {
                    lhs = getStaticVarName(lhs);
                }
                char* rhs = generate_ir(node->children[1]);
                emit("MOD", lhs, rhs, lhs);
                return lhs;
            }
            else if (strcmp(node->value, "&=") == 0) {
                char* lhs = node->children[0]->value;
                if (isStaticVariable(lhs)) {
                    lhs = getStaticVarName(lhs);
                }
                char* rhs = generate_ir(node->children[1]);
                emit("BITAND", lhs, rhs, lhs);
                return lhs;
            }
            else if (strcmp(node->value, "|=") == 0) {
                char* lhs = node->children[0]->value;
                if (isStaticVariable(lhs)) {
                    lhs = getStaticVarName(lhs);
                }
                char* rhs = generate_ir(node->children[1]);
                emit("BITOR", lhs, rhs, lhs);
                return lhs;
            }
            else if (strcmp(node->value, "^=") == 0) {
                char* lhs = node->children[0]->value;
                if (isStaticVariable(lhs)) {
                    lhs = getStaticVarName(lhs);
                }
                char* rhs = generate_ir(node->children[1]);
                emit("BITXOR", lhs, rhs, lhs);
                return lhs;
            }
            else if (strcmp(node->value, "<<=") == 0) {
                char* lhs = node->children[0]->value;
                if (isStaticVariable(lhs)) {
                    lhs = getStaticVarName(lhs);
                }
                char* rhs = generate_ir(node->children[1]);
                emit("LSHIFT", lhs, rhs, lhs);
                return lhs;
            }
            else if (strcmp(node->value, ">>=") == 0) {
                char* lhs = node->children[0]->value;
                if (isStaticVariable(lhs)) {
                    lhs = getStaticVarName(lhs);
                }
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
            // Implement proper short-circuit evaluation with support for nested && expressions
            char* result_temp = newTemp();
            char* true_label = newLabel();
            char* end_label = newLabel();
            
            // Helper function to generate short-circuit code for left side
            // If left is a NODE_LOGICAL_AND_EXPRESSION, we need special handling
            TreeNode* left_node = node->children[0];
            if (left_node->type == NODE_LOGICAL_AND_EXPRESSION) {
                // For (A && B) in (A && B) || C:
                // - Evaluate A
                // - If A is false, jump to evaluate C (not to false result)
                // - Evaluate B
                // - If B is false, jump to evaluate C
                // - If B is true, jump to true_label
                
                char* false_label = newLabel();  // Will evaluate right side (C)
                
                // Evaluate A
                char* a_result = generate_ir(left_node->children[0]);
                const char* a_type = (left_node->children[0] && left_node->children[0]->dataType) ? 
                                    left_node->children[0]->dataType : "int";
                emitConditionalJump("IF_FALSE_GOTO", a_result, false_label, a_type);
                
                // Evaluate B
                char* b_result = generate_ir(left_node->children[1]);
                const char* b_type = (left_node->children[1] && left_node->children[1]->dataType) ? 
                                    left_node->children[1]->dataType : "int";
                emitConditionalJump("IF_FALSE_GOTO", b_result, false_label, b_type);
                
                // Both A and B were true, so the OR is true
                emit("ASSIGN", "1", "", result_temp);
                emit("GOTO", end_label, "", "");
                
                // false_label: A or B was false, evaluate C
                emit("LABEL", false_label, "", "");
                char* right = generate_ir(node->children[1]);
                const char* rightType = (node->children[1] && node->children[1]->dataType) ? 
                                       node->children[1]->dataType : "int";
                emitConditionalJump("IF_TRUE_GOTO", right, true_label, rightType);
                
                // C was also false
                emit("ASSIGN", "0", "", result_temp);
                emit("GOTO", end_label, "", "");
                
                // true_label: C was true
                emit("LABEL", true_label, "", "");
                emit("ASSIGN", "1", "", result_temp);
                
                emit("LABEL", end_label, "", "");
                return result_temp;
            } else {
                // Standard OR evaluation when left is not an AND expression
                // 1. Evaluate left side
                char* left = generate_ir(left_node);
                
                // 2. Short-circuit: if left is true, set result to 1 and skip right
                const char* leftType = (left_node && left_node->dataType) ? 
                                      left_node->dataType : "int";
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
            bool is_ptr_ptr_subtraction = false;
            if (node->children[0]->dataType && node->children[1]->dataType) {
                // Check if this is pointer + integer or array + integer
                // Simple pointer check: contains '*'
                bool left_is_pointer = (strchr(node->children[0]->dataType, '*') != nullptr);
                bool right_is_pointer = (strchr(node->children[1]->dataType, '*') != nullptr);
                
                if ((left_is_pointer || isArrayType(node->children[0]->dataType)) &&
                    isIntegerType(node->children[1]->dataType)) {
                    is_pointer_arithmetic = true;
                }
                
                // Check for pointer - pointer subtraction
                if (left_is_pointer && right_is_pointer && strcmp(node->value, "-") == 0) {
                    is_ptr_ptr_subtraction = true;
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
                if (is_ptr_ptr_subtraction) {
                    // Pointer-pointer subtraction: no type conversion needed
                    emit("PTR_SUB", left, right, temp);
                } else if (is_pointer_arithmetic) {
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
            // Generate cast operation in IR
            if (node->childCount >= 2) {
                // Child 0: type_name (the target type)
                // Child 1: expression being cast
                TreeNode* type_node = node->children[0];
                TreeNode* expr_node = node->children[1];
                
                char* expr_result = generate_ir(expr_node);
                if (!expr_result) return NULL;
                
                // For now, just pass through the expression
                // In a full implementation, you might generate explicit CAST operations
                // For pointer casts especially, we should document the cast in IR
                char* temp = newTemp();
                char cast_info[256];
                
                // Build cast information from source and target types
                const char* src_type = expr_node->dataType ? expr_node->dataType : "unknown";
                const char* tgt_type = node->dataType ? node->dataType : "unknown";
                
                sprintf(cast_info, "CAST_%s_to_%s", src_type, tgt_type);
                emit(cast_info, expr_result, "", temp);
                return temp;
            } else if (node->childCount > 0) {
                // Fallback: just process the expression
                return generate_ir(node->children[0]);
            }
            return NULL;
        }

        case NODE_UNARY_EXPRESSION:
        {
            // Special case: if this node already has the complete value (e.g., "-2147483647")
            // as a pre-computed literal, use it directly instead of generating IR
            if (node->value && (node->value[0] == '-' || node->value[0] == '+') && 
                node->childCount == 1 && 
                (node->children[0]->type == NODE_CONSTANT || 
                 node->children[0]->type == NODE_INTEGER_CONSTANT ||
                 node->children[0]->type == NODE_HEX_CONSTANT ||
                 node->children[0]->type == NODE_OCTAL_CONSTANT ||
                 node->children[0]->type == NODE_BINARY_CONSTANT ||
                 node->children[0]->type == NODE_FLOAT_CONSTANT)) {
                // The value is already a complete literal (like "-2147483647" or "-0x7FFFFFFF")
                return strdup(node->value);
            }
            
            if (strcmp(node->value, "++_pre") == 0) {
                // Pre-increment - use temp-based pattern: t = var + 1, var = t, return t
                char* operand = node->children[0]->value;
                if (isStaticVariable(operand)) {
                    operand = getStaticVarName(operand);
                }
                char* temp = newTemp();
                emit("ADD", operand, "1", temp);  // t = var + 1
                emit("ASSIGN", temp, "", operand);  // var = t
                return temp;  // Return new value
            }
            else if (strcmp(node->value, "--_pre") == 0) {
                // Pre-decrement - use temp-based pattern: t = var - 1, var = t, return t
                char* operand = node->children[0]->value;
                if (isStaticVariable(operand)) {
                    operand = getStaticVarName(operand);
                }
                char* temp = newTemp();
                emit("SUB", operand, "1", temp);  // t = var - 1
                emit("ASSIGN", temp, "", operand);  // var = t
                return temp;  // Return new value
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
                // Function call (direct or indirect via function pointer)
                char* func_name = NULL;
                int is_function_pointer = 0;
                
                // Check if this is a function pointer call
                if (node->children[0]->type == NODE_IDENTIFIER) {
                    func_name = node->children[0]->value;
                    
                    // Check if this identifier is a registered function pointer
                    extern int isFunctionPointerName(const char* name);
                    is_function_pointer = isFunctionPointerName(func_name);
                } else {
                    // If it's not an identifier, it might be an expression that evaluates to a function pointer
                    func_name = generate_ir(node->children[0]);
                    is_function_pointer = 1;  // Assume it's a function pointer if it's an expression
                }
                
                // Process arguments in order (left-to-right as they appear in source)
                int arg_count = 0;
                if (node->childCount > 1) {
                    TreeNode* args = node->children[1];
                    arg_count = args->childCount;
                    
                    // FIXED: Evaluate all arguments FIRST, then emit PARAM instructions
                    // This ensures nested function calls complete before outer call params are emitted
                    #define MAX_FUNC_ARGS 32
                    char* arg_results[MAX_FUNC_ARGS];
                    
                    // Step 1: Evaluate all arguments (this may generate nested function calls)
                    for (int i = 0; i < args->childCount && i < MAX_FUNC_ARGS; i++) {
                        arg_results[i] = generate_ir(args->children[i]);
                    }
                    
                    // Step 2: Now emit all PARAM instructions in order
                    for (int i = 0; i < args->childCount && i < MAX_FUNC_ARGS; i++) {
                        char* arg = arg_results[i];
                        
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
                
                char arg_count_str[32];
                sprintf(arg_count_str, "%d", arg_count);
                
                // Check if this is a void function - if so, don't assign result to temporary
                bool is_void_function = false;
                if (func_name && !is_function_pointer) {
                    // Look up the function in the symbol table
                    extern Symbol* lookupSymbol(const char* name);
                    Symbol* func_sym = lookupSymbol(func_name);
                    if (func_sym && func_sym->is_function) {
                        is_void_function = (strcmp(func_sym->return_type, "void") == 0);
                    }
                }
                
                if (is_void_function) {
                    // Void function - no return value, don't create temporary
                    emit("CALL", func_name, arg_count_str, "");
                    return NULL;  // No result to return
                } else {
                    // Non-void function - capture return value in temporary
                    char* temp = newTemp();
                    if (is_function_pointer) {
                        // Indirect call through function pointer
                        emit("INDIRECT_CALL", func_name, arg_count_str, temp);
                    } else {
                        // Direct function call
                        emit("CALL", func_name, arg_count_str, temp);
                    }
                    return temp;
                }
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
                if (isStaticVariable(operand)) {
                    operand = getStaticVarName(operand);
                }
                char* old_val = newTemp();
                char* new_val = newTemp();
                emit("ASSIGN", operand, "", old_val);  // old_val = var (save old value)
                emit("ADD", operand, "1", new_val);    // new_val = var + 1
                emit("ASSIGN", new_val, "", operand);  // var = new_val
                return old_val;  // Return old value
            }
            else if (strcmp(node->value, "--_post") == 0) {
                char* operand = node->children[0]->value;
                if (isStaticVariable(operand)) {
                    operand = getStaticVarName(operand);
                }
                char* old_val = newTemp();
                char* new_val = newTemp();
                emit("ASSIGN", operand, "", old_val);  // old_val = var (save old value)
                emit("SUB", operand, "1", new_val);    // new_val = var - 1
                emit("ASSIGN", new_val, "", operand);  // var = new_val
                return old_val;  // Return old value
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
                
                // Extract the actual variable name from the declarator using helper
                lhs_name = (char*)extractIdentifierFromDeclarator(declarator);
                
                if (!lhs_name) {
                    lhs_name = declarator->value; // fallback
                }
                
                // Check if this is a static variable
                if (isStaticVariable(lhs_name)) {
                    // For static variables, register the initialization but don't emit code here
                    TreeNode* rhs = node->children[1];
                    
                    // Get the initialization value
                    char* init_value = NULL;
                    if (rhs->childCount == 0 && rhs->value) {
                        // Simple constant initializer
                        init_value = rhs->value;
                    } else if (rhs->childCount > 0) {
                        // Expression initializer - evaluate it
                        init_value = generate_ir(rhs);
                    }
                    
                    // Get the full static variable name
                    char* static_var_name = getStaticVarName(lhs_name);
                    
                    // Register the static variable for later initialization
                    registerStaticVar(static_var_name, init_value ? init_value : "0");
                    
                    free(static_var_name);
                    return lhs_name;
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