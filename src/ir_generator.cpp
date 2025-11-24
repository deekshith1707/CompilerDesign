#include "ir_generator.h"
#include "ir_context.h"
#include "symbol_table.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <set>

using namespace std;

static std::set<std::string> variables_to_allocate;
static char current_function_name[256] = "";
static int isReferenceVariable(const char* var_name) {
    extern Symbol symtab[];
    extern int symCount;
    extern Symbol* lookupSymbol(const char* name);
    
    Symbol* sym = lookupSymbol(var_name);
    return (sym && sym->is_reference);
}

static int isStaticVariable(const char* var_name) {
    extern Symbol symtab[];
    extern int symCount;
    
    if (current_function_name[0] != '\0') {
        for (int i = 0; i < symCount; i++) {
            if (strcmp(symtab[i].name, var_name) == 0 &&
                strcmp(symtab[i].function_scope, current_function_name) == 0 &&
                symtab[i].is_static && !symtab[i].is_function) {
                return 1;
            }
        }
    }
    
    for (int i = 0; i < symCount; i++) {
        if (strcmp(symtab[i].name, var_name) == 0 &&
            symtab[i].function_scope[0] == '\0' &&
            symtab[i].is_static && !symtab[i].is_function) {
            return 1;
        }
    }
    
    return 0;
}

static char* getStaticVarName(const char* var_name) {
    extern Symbol symtab[];
    extern int symCount;
    
    Symbol* sym = NULL;
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
    
    if (sym->scope_level == 0 || sym->function_scope[0] == '\0') {
        return strdup(var_name);
    }
    
    static char full_name[512];
    sprintf(full_name, "%s.%s", sym->function_scope, var_name);
    return strdup(full_name);
}

static const char* extractIdentifierFromDeclarator(TreeNode* node) {
    if (!node) return NULL;
    if (node->type == NODE_IDENTIFIER) return node->value;
    
    for (int i = 0; i < node->childCount; i++) {
        const char* name = extractIdentifierFromDeclarator(node->children[i]);
        if (name) return name;
    }
    return NULL;
}

struct CaseLabel {
    char* value;
    char* label;
};

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

#define MAX_LOOP_DEPTH 100
static LoopContext loopStack[MAX_LOOP_DEPTH];
static int loopDepth = 0;
static SwitchContext switchStack[MAX_LOOP_DEPTH];
static int switchDepth = 0;
static int switchCount = 0;

static JumpList* break_lists[MAX_LOOP_DEPTH] = {NULL};
static bool last_was_unconditional_jump = false;
static void pushLoopLabels(char* continue_label, char* break_label) {
    if (loopDepth >= MAX_LOOP_DEPTH) {
        cerr << "Error: Loop nesting too deep" << endl;
        return;
    }
    loopStack[loopDepth].continue_label = continue_label;
    loopStack[loopDepth].break_label = break_label;
    break_lists[loopDepth] = NULL;
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

static void addBreakJump(int jump_index) {
    if (loopDepth > 0) {
        JumpList* new_jump = makelist(jump_index);
        break_lists[loopDepth - 1] = merge(break_lists[loopDepth - 1], new_jump);
    }
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
    
    // Use a rotating set of static buffers to avoid reuse issues
    static char value_buffers[4][64];
    static int buffer_index = 0;
    char* current_buffer = value_buffers[buffer_index];
    buffer_index = (buffer_index + 1) % 4;
    
    switch (node->type) {
        case NODE_CONSTANT:
        case NODE_INTEGER_CONSTANT:
        case NODE_HEX_CONSTANT:
        case NODE_OCTAL_CONSTANT:
        case NODE_BINARY_CONSTANT:
        case NODE_CHAR_CONSTANT:
            return node->value;
        
        case NODE_IDENTIFIER: {
            extern Symbol* lookupSymbol(const char* name);
            Symbol* sym = lookupSymbol(node->value);
            if (sym && strcmp(sym->kind, "enum_constant") == 0) {
                sprintf(current_buffer, "%d", sym->offset);
                return current_buffer;
            }
            return NULL; // Not a constant
        }
        
        case NODE_UNARY_EXPRESSION: {
            // Check if this node already has the computed value (for constant folding like -1)
            if (node->value && node->value[0] >= '0' && node->value[0] <= '9') {
                return node->value;
            }
            if (node->value && node->value[0] == '-' && node->value[1] >= '0' && node->value[1] <= '9') {
                return node->value;
            }
            
            // Handle unary minus for negative constants
            if (node->value && (strcmp(node->value, "-") == 0 || strcmp(node->value, "-_unary") == 0) && node->childCount == 1) {
                char* child_val = get_constant_value_from_expression(node->children[0]);
                if (child_val) {
                    if (child_val[0] == '-') {
                        // Double negative, remove the minus
                        snprintf(current_buffer, 64, "%s", child_val + 1);
                    } else {
                        // Add minus sign
                        snprintf(current_buffer, 64, "-%s", child_val);
                    }
                    return current_buffer;
                }
            }
            // Handle unary plus (just return child value)
            if (node->value && (strcmp(node->value, "+") == 0 || strcmp(node->value, "+_unary") == 0) && node->childCount == 1) {
                return get_constant_value_from_expression(node->children[0]);
            }
            return NULL;
        }
    }
    
    if (node->childCount == 1) {
        return get_constant_value_from_expression(node->children[0]);
    }
    
    return NULL; // Not a simple constant
}

static void find_case_labels(TreeNode* node, std::vector<CaseLabel>& cases, char** default_label, int switch_id) {
    if (!node) return;

    if (node->type == NODE_LABELED_STATEMENT) {
        if (strcmp(node->value, "case") == 0 && node->childCount > 0) {
            TreeNode* case_expr = node->children[0];
            char* const_val = get_constant_value_from_expression(case_expr);
            
            if (const_val) {
                if (strchr(const_val, '.') != NULL) {
                    extern int yylineno;
                    fprintf(stderr, "Semantic Error on line %d: floating-point constant '%s' in case label (only integer constants allowed)\n", 
                            yylineno, const_val);
                }
                
                for (const auto& existing_case : cases) {
                    if (strcmp(existing_case.value, const_val) == 0) {
                        extern int yylineno;
                        fprintf(stderr, "Semantic Error on line %d: duplicate case value '%s' in switch statement\n", 
                                yylineno, const_val);
                        break;
                    }
                }
                
                if (case_expr->type == NODE_IDENTIFIER) {
                    extern Symbol* lookupSymbol(const char* name);
                    Symbol* sym = lookupSymbol(case_expr->value);
                    if (sym && sym->is_const) {
                        extern int yylineno;
                        fprintf(stderr, "Semantic Error on line %d: case label '%s' is not an integer constant expression (const variable not allowed)\n", 
                                yylineno, case_expr->value);
                    }
                }
                
                char label_name[128];
                // Sanitize const_val for use in label (replace '-' with 'NEG_')
                char sanitized_val[64];
                if (const_val[0] == '-') {
                    snprintf(sanitized_val, sizeof(sanitized_val), "NEG_%s", const_val + 1);
                } else {
                    snprintf(sanitized_val, sizeof(sanitized_val), "%s", const_val);
                }
                sprintf(label_name, "SWITCH_%d_CASE_%s", switch_id, sanitized_val);
                cases.push_back({strdup(const_val), strdup(label_name)});
            }
            if (node->childCount > 1) {
                find_case_labels(node->children[1], cases, default_label, switch_id);
            }
            return; // Stop recursion down this branch, we've handled it.
        }
        else if (strcmp(node->value, "default") == 0) {
            char default_name[128];
            sprintf(default_name, "SWITCH_%d_DEFAULT", switch_id);
            *default_label = strdup(default_name);
            if (node->childCount > 0) {
                find_case_labels(node->children[0], cases, default_label, switch_id);
            }
            return; // Stop recursion
        }
    }

    if (node->type == NODE_SELECTION_STATEMENT && strcmp(node->value, "switch") == 0) {
        return;
    }

    for (int i = 0; i < node->childCount; i++) {
        find_case_labels(node->children[i], cases, default_label, switch_id);
    }
}


static char* convertType(char* place, const char* from_type, const char* to_type) {
    if (strcmp(from_type, to_type) == 0) return place;
    
    if (strstr(from_type, "[") != NULL || strstr(to_type, "[") != NULL) {
        return place;
    }
    
    if (strstr(from_type, "*") != NULL || strstr(to_type, "*") != NULL) {
        return place;
    }
    
    char* resolved_from = resolveTypedef(from_type);
    char* resolved_to = resolveTypedef(to_type);
    if (resolved_from && resolved_to && strcmp(resolved_from, resolved_to) == 0) {
        return place;
    }
    
    bool from_is_enum_or_int = (resolved_from && (strcmp(resolved_from, "enum") == 0 || strcmp(resolved_from, "int") == 0));
    bool to_is_enum_or_int = (resolved_to && (strcmp(resolved_to, "enum") == 0 || strcmp(resolved_to, "int") == 0));
    if (from_is_enum_or_int && to_is_enum_or_int) {
        return place;
    }
    
    char* temp = newTemp();
    char cast_op[128];
    auto sanitize_type = [](const char* t, char* out, int out_size) {
        int j = 0;
        for (int i = 0; t[i] && j < out_size - 1; i++) {
            char c = t[i];
            if (c == ' ' || c == '*' || c == '&' || c == '(' || c == ')' || c == ',') {
                out[j++] = '_';
            } else {
                out[j++] = c;
            }
        }
        out[j] = '\0';
    };
    char from_buf[64], to_buf[64];
    sanitize_type(from_type, from_buf, sizeof(from_buf));
    sanitize_type(to_type, to_buf, sizeof(to_buf));
    sprintf(cast_op, "CAST_%s_to_%s", from_buf, to_buf);
    emit(cast_op, place, "", temp);
    return temp;
}

char* generate_ir(TreeNode* node) {
    if (!node) return NULL;

    switch (node->type) {
        
        
        case NODE_CONSTANT:
        case NODE_INTEGER_CONSTANT:
        case NODE_HEX_CONSTANT:
        case NODE_OCTAL_CONSTANT:
        case NODE_BINARY_CONSTANT:
        case NODE_FLOAT_CONSTANT:
        case NODE_CHAR_CONSTANT:
        case NODE_STRING_LITERAL:
        {
            return strdup(node->value);
        }
        
        case NODE_IDENTIFIER:
        {
            for (int i = 0; i < node->childCount; i++) {
                generate_ir(node->children[i]);
            }
            
            // Check if this is an enum constant - treat as integer constant
            extern Symbol* lookupSymbol(const char* name);
            Symbol* sym = lookupSymbol(node->value);
            if (sym && strcmp(sym->kind, "enum_constant") == 0) {
                // Return the enum value as a string
                char* enum_value = (char*)malloc(32);
                sprintf(enum_value, "%d", sym->offset);
                return enum_value;
            }
            
            if (isStaticVariable(node->value)) {
                return getStaticVarName(node->value);
            }
            
            if (isReferenceVariable(node->value)) {
                char* temp = newTemp();
                char deref_expr[256];
                sprintf(deref_expr, "[%s]", node->value);
                emit("LOAD", deref_expr, "", temp);
                return temp;
            }
            
            return strdup(node->value);
        }

        
        case NODE_PROGRAM:
        case NODE_DECLARATION_SPECIFIERS:
        case NODE_TYPE_SPECIFIER:
        case NODE_STORAGE_CLASS_SPECIFIER:
        case NODE_TYPE_QUALIFIER:
        {
            for (int i = 0; i < node->childCount; i++) {
                generate_ir(node->children[i]);
            }
            return NULL;
        }

        case NODE_FUNCTION_DEFINITION:
        {
            
            TreeNode* declarator = node->children[1];
            char* func_name = NULL;
            if (declarator && declarator->value) {
                func_name = declarator->value;
                strcpy(current_function_name, func_name);
                
                extern int current_scope;
                int saved_scope = current_scope;
                current_scope = 1;  // Function scope
                
                variables_to_allocate.clear();
                
                emit("FUNC_BEGIN", func_name, "", "");
            
                if (node->childCount > 2) {
                    generate_ir(node->children[2]);
                }
                
                extern Symbol* lookupSymbol(const char* name);
                Symbol* func_sym = lookupSymbol(func_name);
                if (func_sym && func_sym->is_function && strcmp(func_sym->return_type, "void") == 0) {
                    emit("RETURN", "", "", "");
                }
                
                emit("FUNC_END", func_name, "", "");
                
                current_scope = saved_scope;
                current_function_name[0] = '\0';
                variables_to_allocate.clear();
            } else {
                if (node->childCount > 2) {
                    generate_ir(node->children[2]);
                }
            }
            
            return NULL;
        }

        case NODE_DECLARATION:
        {
            for (int i = 1; i < node->childCount; i++) {
                TreeNode* child = node->children[i];
                
                if (child) {
                    if (child->type == NODE_INITIALIZER) {
                        generate_ir(child);
                    }
                    else {
                        generate_ir(child);
                    }
                }
            }
            return NULL;
        }

        
        case NODE_COMPOUND_STATEMENT:
        {
            bool saved_flag = last_was_unconditional_jump;
            
            for (int i = 0; i < node->childCount; i++) {
                last_was_unconditional_jump = false;
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
                
                char* end_label = newLabel();
                
                int jump_index = emitWithIndex("IF_FALSE_GOTO", cond_result, end_label, "");
                
                if (node->childCount > 1) {
                    generate_ir(node->children[1]);
                }
                
                emit("LABEL", end_label, "", "");
            }
            else if (strcmp(node->value, "if_else") == 0) {
                char* cond_result = generate_ir(node->children[0]);
                
                char* else_label = newLabel();
                char* end_label = newLabel();
                
                int false_jump_index = emitWithIndex("IF_FALSE_GOTO", cond_result, else_label, "");
                
                last_was_unconditional_jump = false;
                
                if (node->childCount > 1) {
                    generate_ir(node->children[1]);
                }
                
                JumpList* end_list = NULL;
                if (!last_was_unconditional_jump) {
                    int end_jump_index = emitWithIndex("GOTO", "PLACEHOLDER", "", "");
                    end_list = makelist(end_jump_index);
                }
                
                last_was_unconditional_jump = false;
                
                emit("LABEL", else_label, "", "");
                
                if (node->childCount > 2) {
                    generate_ir(node->children[2]);
                }
                
                if (end_list != NULL) {
                    backpatch(end_list, end_label);
                    freeJumpList(end_list);
                    emit("LABEL", end_label, "", "");
                } else if (!last_was_unconditional_jump) {
                    emit("LABEL", end_label, "", "");
                }
                
                last_was_unconditional_jump = false;
            }
            else if (strcmp(node->value, "switch") == 0) {
                
                char* switch_expr = generate_ir(node->children[0]);
                char* switch_end = newLabel();
                
                int current_switch_id = switchCount;
                
                std::vector<CaseLabel> case_labels;
                char* default_label = NULL;
                
                if (node->childCount > 1) {
                    find_case_labels(node->children[1], case_labels, &default_label, current_switch_id);
                }
                
                char* final_default_label = default_label ? default_label : switch_end;
                
                pushSwitchLabel(switch_end, final_default_label);
                
                for (const auto& cl : case_labels) {
                    char* temp_const = newTemp();
                    emit("ASSIGN", cl.value, "", temp_const);
                    char* temp_cmp = newTemp();
                    emit("EQ", switch_expr, temp_const, temp_cmp);
                    emit("IF_TRUE_GOTO", temp_cmp, cl.label, "");
                }
                
                emit("GOTO", final_default_label, "", "");
                
                if (node->childCount > 1) {
                    generate_ir(node->children[1]);
                }
                
                emit("LABEL", switch_end, "", "");
                popSwitchLabel();

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
                
                char* start_label = newLabel();
                char* end_label = newLabel();
                
                emit("LABEL", start_label, "", "");
                
                char* cond_result = generate_ir(node->children[0]);
                
                int false_jump_index = emitWithIndex("IF_FALSE_GOTO", cond_result, end_label, "");
                
                pushLoopLabels(start_label, end_label);
                
                if (node->childCount > 1) {
                    generate_ir(node->children[1]);
                }
                
                emit("GOTO", start_label, "", "");
                
                JumpList* break_list = (loopDepth > 0) ? break_lists[loopDepth - 1] : NULL;
                backpatch(break_list, end_label);
                freeJumpList(break_list);
                
                if (loopDepth > 0) {
                    break_lists[loopDepth - 1] = NULL;
                }
                
                emit("LABEL", end_label, "", "");
                
                popLoopLabels();
            }
            else if (strcmp(node->value, "do_while") == 0 || strcmp(node->value, "do_until") == 0) {
                
                char* start_label = newLabel();
                char* test_label = newLabel();
                char* end_label = newLabel();
                
                emit("LABEL", start_label, "", "");
                
                pushLoopLabels(test_label, end_label);
                
                if (node->childCount > 2) {
                    generate_ir(node->children[2]);
                }
                
                emit("LABEL", test_label, "", "");
                
                char* cond_result = NULL;
                if (node->childCount > 0) { 
                    cond_result = generate_ir(node->children[0]);
                }
                
                if (cond_result) {
                    if (strcmp(node->value, "do_while") == 0) {
                        emit("IF_TRUE_GOTO", cond_result, start_label, "");
                    } else {
                        emit("IF_FALSE_GOTO", cond_result, start_label, "");
                    }
                }
                
                JumpList* break_list = (loopDepth > 0) ? break_lists[loopDepth - 1] : NULL;
                backpatch(break_list, end_label);
                freeJumpList(break_list);
                
                if (loopDepth > 0) {
                    break_lists[loopDepth - 1] = NULL;
                }
                
                emit("LABEL", end_label, "", "");
                
                popLoopLabels();
            }
            else if (strcmp(node->value, "for") == 0) {
                
                if (node->childCount > 0 && node->children[0]) {
                    if (!(node->children[0]->type == NODE_EXPRESSION_STATEMENT && 
                          (!node->children[0]->value || strlen(node->children[0]->value) == 0))) {
                        generate_ir(node->children[0]);
                    }
                }
                
                char* cond_label = newLabel();
                char* incr_label = newLabel();
                char* end_label = newLabel();
                
                emit("LABEL", cond_label, "", "");
                
                if (node->childCount > 1 && node->children[1]) {
                    if (!(node->children[1]->type == NODE_EXPRESSION_STATEMENT && 
                          (!node->children[1]->value || strlen(node->children[1]->value) == 0))) {
                        char* cond_result = generate_ir(node->children[1]);
                        if (cond_result && strlen(cond_result) > 0) {
                            emit("IF_FALSE_GOTO", cond_result, end_label, "");
                        }
                    }
                }
                
                pushLoopLabels(incr_label, end_label);
                
                if (node->childCount > 3) {
                    generate_ir(node->children[3]);
                }
                
                emit("LABEL", incr_label, "", "");
                
                if (node->childCount > 2 && node->children[2]) {
                    if (!(node->children[2]->type == NODE_EXPRESSION_STATEMENT && 
                          (!node->children[2]->value || strlen(node->children[2]->value) == 0))) {
                        generate_ir(node->children[2]);
                    }
                }
                
                emit("GOTO", cond_label, "", "");
                
                JumpList* break_list = (loopDepth > 0) ? break_lists[loopDepth - 1] : NULL;
                backpatch(break_list, end_label);
                freeJumpList(break_list);
                
                if (loopDepth > 0) {
                    break_lists[loopDepth - 1] = NULL;
                }
                
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
                    last_was_unconditional_jump = true;
                }
            }
            else if (strcmp(node->value, "continue") == 0) {
                char* continue_label = getCurrentLoopContinue();
                if (continue_label) {
                    emit("GOTO", continue_label, "", "");
                    last_was_unconditional_jump = true;
                } else {
                    cerr << "Error: continue statement outside loop" << endl;
                }
            }
            else if (strcmp(node->value, "break") == 0) {
                // CRITICAL FIX: Check switch context FIRST before loop context
                // When break is inside a switch that's inside a loop,
                // it should break from the switch, NOT the loop
                char* switch_end_label = getCurrentSwitchEnd();
                if (switch_end_label) {
                    // Inside a switch - break from switch
                    emit("GOTO", switch_end_label, "", "");
                    last_was_unconditional_jump = true;
                } else if (loopDepth > 0) {
                    // Inside a loop (but not in a switch) - break from loop
                    int break_jump_index = emitWithIndex("GOTO", "0", "", "");
                    addBreakJump(break_jump_index);
                    last_was_unconditional_jump = true;
                } else {
                    cerr << "Error: break statement outside loop or switch" << endl;
                }
            }
            else if (strcmp(node->value, "return") == 0) {
                if (node->childCount > 0) {
                    char* ret_val = generate_ir(node->children[0]);
                    emit("RETURN", ret_val, "", "");
                } else {
                    emit("RETURN", "", "", "");
                }
                last_was_unconditional_jump = true;
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
                    char* const_val = get_constant_value_from_expression(node->children[0]);
                    if (const_val) {
                        int switch_id = getCurrentSwitchId();
                        // Sanitize const_val for use in label (replace '-' with 'NEG_')
                        char sanitized_val[64];
                        if (const_val[0] == '-') {
                            snprintf(sanitized_val, sizeof(sanitized_val), "NEG_%s", const_val + 1);
                        } else {
                            snprintf(sanitized_val, sizeof(sanitized_val), "%s", const_val);
                        }
                        
                        if (switch_id >= 0) {
                            char case_label[128];
                            sprintf(case_label, "SWITCH_%d_CASE_%s", switch_id, sanitized_val);
                            emit("LABEL", case_label, "", "");
                        } else {
                            char case_label[128];
                            sprintf(case_label, "CASE_%s", sanitized_val);
                            emit("LABEL", case_label, "", "");
                        }
                    } else {
                        cerr << "Error: case expression is not a simple constant." << endl;
                    }
                }
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
                    emit("LABEL", "DEFAULT", "", "");
                }
                if (node->childCount > 0) {
                    generate_ir(node->children[0]);
                }
            }
            return NULL;
        }

        
        case NODE_EXPRESSION:
        {
            char* result = NULL;
            for (int i = 0; i < node->childCount; i++) {
                result = generate_ir(node->children[i]);
            }
            return result;
        }

        case NODE_ASSIGNMENT_EXPRESSION:
        {
            if (strcmp(node->value, "=") == 0) {
                
                char* rhs_result = generate_ir(node->children[1]);
                
                TreeNode* lhs = node->children[0];
                if (lhs->type == NODE_IDENTIFIER) {
                    
                    char* lhs_name = lhs->value;
                    if (isStaticVariable(lhs_name)) {
                        lhs_name = getStaticVarName(lhs_name);
                    }
                    
                    if (node->children[0]->dataType && node->children[1]->dataType &&
                        strstr(node->children[0]->dataType, "*") != NULL &&
                        strstr(node->children[1]->dataType, "[") != NULL) {
                        if (isReferenceVariable(lhs_name)) {
                            char deref_lhs[256];
                            sprintf(deref_lhs, "[%s]", lhs_name);
                            emit("STORE", rhs_result, "", deref_lhs);
                        } else {
                            emit("ASSIGN", rhs_result, "", lhs_name);
                        }
                    }
                    else if (node->children[0]->dataType && node->children[1]->dataType &&
                             strstr(node->children[0]->dataType, "[") == NULL &&
                             strstr(node->children[1]->dataType, "[") == NULL) {
                        
                        extern Symbol* lookupSymbol(const char* name);
                        Symbol* lhs_sym = lookupSymbol(lhs_name);
                        Symbol* rhs_sym = lookupSymbol(rhs_result);
                        
                        if (lhs_sym && strncmp(lhs_sym->type, "struct ", 7) == 0 &&
                            rhs_sym && strncmp(rhs_sym->type, "struct ", 7) == 0) {
                            const char* struct_name = lhs_sym->type + 7;
                            extern StructDef* lookupStruct(const char* name);
                            StructDef* struct_def = lookupStruct(struct_name);
                            
                            if (struct_def) {
                                char* lhs_addr = newTemp();
                                char* rhs_addr = newTemp();
                                emit("ADDR", lhs_name, "", lhs_addr);
                                emit("ADDR", rhs_result, "", rhs_addr);
                                
                                for (int i = 0; i < struct_def->member_count; i++) {
                                    char offset_str[32];
                                    sprintf(offset_str, "%d", struct_def->members[i].offset);
                                    
                                    char* temp_val = newTemp();
                                    emit("LOAD_OFFSET", rhs_addr, offset_str, temp_val);
                                    
                                    emit("STORE_OFFSET", lhs_addr, offset_str, temp_val);
                                }
                            } else {
                                emit("ASSIGN", rhs_result, "", lhs_name);
                            }
                        } else {
                            rhs_result = convertType(rhs_result, node->children[1]->dataType, node->children[0]->dataType);
                            
                            if (isReferenceVariable(lhs_name)) {
                                char deref_lhs[256];
                                sprintf(deref_lhs, "[%s]", lhs_name);
                                emitTyped("STORE", rhs_result, "", deref_lhs, node->children[0]->dataType);
                            } else {
                                emitTyped("ASSIGN", rhs_result, "", lhs_name, node->children[0]->dataType);
                            }
                        }
                    }
                    else {
                        if (isReferenceVariable(lhs_name)) {
                            char deref_lhs[256];
                            sprintf(deref_lhs, "[%s]", lhs_name);
                            emit("STORE", rhs_result, "", deref_lhs);
                        } else {
                            extern Symbol* lookupSymbol(const char* name);
                            Symbol* lhs_sym = lookupSymbol(lhs_name);
                            Symbol* rhs_sym = lookupSymbol(rhs_result);
                            
                            if (lhs_sym && lhs_sym->type && strncmp(lhs_sym->type, "struct ", 7) == 0 &&
                                rhs_sym && rhs_sym->type && strncmp(rhs_sym->type, "struct ", 7) == 0) {
                                const char* struct_name = lhs_sym->type + 7;
                                extern StructDef* lookupStruct(const char* name);
                                StructDef* struct_def = lookupStruct(struct_name);
                                
                                if (struct_def) {
                                    char* lhs_addr = newTemp();
                                    char* rhs_addr = newTemp();
                                    emit("ADDR", lhs_name, "", lhs_addr);
                                    emit("ADDR", rhs_result, "", rhs_addr);
                                    
                                    for (int i = 0; i < struct_def->member_count; i++) {
                                        char offset_str[32];
                                        sprintf(offset_str, "%d", struct_def->members[i].offset);
                                        
                                        char* temp_val = newTemp();
                                        emit("LOAD_OFFSET", rhs_addr, offset_str, temp_val);
                                        
                                        emit("STORE_OFFSET", lhs_addr, offset_str, temp_val);
                                    }
                                } else {
                                    emit("ASSIGN", rhs_result, "", lhs_name);
                                }
                            } else {
                                TreeNode* rhs_node = node->children[1];
                                char* resolved_lhs_type = lhs_sym ? resolveTypedef(lhs_sym->type) : NULL;
                                if (resolved_lhs_type && strstr(resolved_lhs_type, "char[") != NULL &&
                                    rhs_node->type == NODE_STRING_LITERAL) {
                                    const char* str_value = rhs_result;
                                    
                                    if (str_value[0] == '"') {
                                        str_value++; // Skip opening quote
                                    }
                                    
                                    char* arr_addr = newTemp();
                                    emit("ADDR", lhs_name, "", arr_addr);
                                    
                                    int idx = 0;
                                    while (*str_value && *str_value != '"') {
                                        char char_literal[8];
                                        if (*str_value == '\\' && *(str_value + 1)) {
                                            str_value++;
                                            switch (*str_value) {
                                                case 'n': sprintf(char_literal, "'\\n'"); break;
                                                case 't': sprintf(char_literal, "'\\t'"); break;
                                                case 'r': sprintf(char_literal, "'\\r'"); break;
                                                case '0': sprintf(char_literal, "'\\0'"); break;
                                                case '\\': sprintf(char_literal, "'\\\\'"); break;
                                                case '"': sprintf(char_literal, "'\\\"'"); break;
                                                default: sprintf(char_literal, "'%c'", *str_value); break;
                                            }
                                        } else {
                                            sprintf(char_literal, "'%c'", *str_value);
                                        }
                                        
                                        char offset_str[32];
                                        sprintf(offset_str, "%d", idx);
                                        emit("STORE_OFFSET", arr_addr, offset_str, char_literal);
                                        idx++;
                                        str_value++;
                                    }
                                    
                                    char offset_str[32];
                                    sprintf(offset_str, "%d", idx);
                                    emit("STORE_OFFSET", arr_addr, offset_str, "'\\0'");
                                    free(resolved_lhs_type);
                                } else {
                                    if (resolved_lhs_type) free(resolved_lhs_type);
                                    emit("ASSIGN", rhs_result, "", lhs_name);
                                }
                            }
                        }
                    }
                    return lhs_name;
                }
                else if (lhs->type == NODE_POSTFIX_EXPRESSION && strcmp(lhs->value, ".") == 0) {
                    TreeNode* base = lhs->children[0];
                    char* member = lhs->children[1]->value;
                    
                    if (base->type == NODE_POSTFIX_EXPRESSION && strcmp(base->value, "[]") == 0) {
                        char* array = generate_ir(base->children[0]);
                        char* index = generate_ir(base->children[1]);
                        
                        int member_offset = 0;
                        int struct_size = 0;
                        const char* member_type = NULL;
                        
                        if (base->dataType) {
                            // CRITICAL: Resolve typedef to get actual struct type
                            char* resolved_type = resolveTypedef(base->dataType);
                            const char* actual_type = resolved_type ? resolved_type : base->dataType;
                            
                            if (strncmp(actual_type, "struct ", 7) == 0) {
                                const char* struct_name = actual_type + 7;
                                extern StructDef* lookupStruct(const char* name);
                                StructDef* struct_def = lookupStruct(struct_name);
                                
                                if (struct_def) {
                                    struct_size = struct_def->total_size;
                                    for (int i = 0; i < struct_def->member_count; i++) {
                                        if (strcmp(struct_def->members[i].name, member) == 0) {
                                            member_offset = struct_def->members[i].offset;
                                            member_type = struct_def->members[i].type;
                                            break;
                                        }
                                    }
                                }
                            }
                            
                            if (resolved_type) free(resolved_type);
                        }
                        
                        if (member_type && node->children[1]->dataType && 
                            strcmp(member_type, node->children[1]->dataType) != 0) {
                            rhs_result = convertType(rhs_result, node->children[1]->dataType, member_type);
                        }
                        
                        char* elem_addr = newTemp();
                        if (struct_size > 0) {
                            char* scaled_index = newTemp();
                            char size_str[32];
                            sprintf(size_str, "%d", struct_size);
                            emit("MUL", index, size_str, scaled_index);
                            emit("ADD", array, scaled_index, elem_addr);
                        } else {
                            emit("ARRAY_ADDR", array, index, elem_addr);
                        }
                        
                        char offset_str[32];
                        sprintf(offset_str, "%d", member_offset);
                        emit("STORE_OFFSET", elem_addr, offset_str, rhs_result);
                        return array;
                    } else {
                        char* struct_var = NULL;
                        if (base->type == NODE_POSTFIX_EXPRESSION && strcmp(base->value, ".") == 0) {
                            char* base_path;
                            if (base->children[0]->type == NODE_IDENTIFIER) {
                                base_path = base->children[0]->value;
                            } else {
                                base_path = generate_ir(base->children[0]);
                            }
                            char* base_member = base->children[1]->value;
                            
                            static char full_path[256];
                            snprintf(full_path, sizeof(full_path), "%s.%s.%s", base_path, base_member, member);
                            emit("ASSIGN", rhs_result, "", full_path);
                            return base_path;
                        } else {
                            struct_var = base->value;
                        }
                        
                        extern Symbol* lookupSymbol(const char* name);
                        Symbol* sym = lookupSymbol(struct_var);
                        const char* target_type = NULL;
                        
                        if (sym && sym->type) {
                            // CRITICAL: Resolve typedef to get actual struct type
                            char* resolved_type = resolveTypedef(sym->type);
                            const char* actual_type = resolved_type ? resolved_type : sym->type;
                            
                            if (strncmp(actual_type, "struct ", 7) == 0) {
                                const char* struct_name = actual_type + 7;
                                extern StructDef* lookupStruct(const char* name);
                                StructDef* struct_def = lookupStruct(struct_name);
                                
                                if (struct_def) {
                                    for (int i = 0; i < struct_def->member_count; i++) {
                                        if (strcmp(struct_def->members[i].name, member) == 0) {
                                            target_type = struct_def->members[i].type;
                                            break;
                                        }
                                    }
                                }
                            }
                            
                            if (resolved_type) free(resolved_type);
                        }
                        
                        if (target_type && node->children[1]->dataType && 
                            strcmp(target_type, node->children[1]->dataType) != 0) {
                            rhs_result = convertType(rhs_result, node->children[1]->dataType, target_type);
                        }
                        
                        char* struct_addr = newTemp();
                        emit("ADDR", struct_var, "", struct_addr);
                        
                        int member_offset = 0;
                        if (sym && sym->type) {
                            // CRITICAL: Resolve typedef to get actual struct type
                            char* resolved_type = resolveTypedef(sym->type);
                            const char* actual_type = resolved_type ? resolved_type : sym->type;
                            
                            if (strncmp(actual_type, "struct ", 7) == 0) {
                                const char* struct_name = actual_type + 7;
                                extern StructDef* lookupStruct(const char* name);
                                StructDef* struct_def = lookupStruct(struct_name);
                                if (struct_def) {
                                    for (int i = 0; i < struct_def->member_count; i++) {
                                        if (strcmp(struct_def->members[i].name, member) == 0) {
                                            member_offset = struct_def->members[i].offset;
                                            break;
                                        }
                                    }
                                }
                            }
                            
                            if (resolved_type) free(resolved_type);
                        }
                        
                        char offset_str[32];
                        sprintf(offset_str, "%d", member_offset);
                        emit("STORE_OFFSET", struct_addr, offset_str, rhs_result);
                        return struct_var;
                    }
                }
                else if (lhs->type == NODE_POSTFIX_EXPRESSION && strcmp(lhs->value, "->") == 0) {
                    char* struct_ptr = generate_ir(lhs->children[0]);
                    char* member = lhs->children[1]->value;
                    
                    int member_offset = 0;
                    const char* member_type = NULL;
                    
                    TreeNode* ptr_node = lhs->children[0];
                    if (ptr_node->dataType && strstr(ptr_node->dataType, "*")) {
                        char struct_type[128];
                        strncpy(struct_type, ptr_node->dataType, sizeof(struct_type) - 1);
                        struct_type[sizeof(struct_type) - 1] = '\0';
                        
                        char* ptr_char = strrchr(struct_type, '*');
                        if (ptr_char) *ptr_char = '\0';
                        
                        int len = strlen(struct_type);
                        while (len > 0 && struct_type[len-1] == ' ') {
                            struct_type[--len] = '\0';
                        }
                        
                        if (strncmp(struct_type, "struct ", 7) == 0) {
                            const char* struct_name = struct_type + 7;
                            extern StructDef* lookupStruct(const char* name);
                            StructDef* struct_def = lookupStruct(struct_name);
                            
                            if (struct_def) {
                                for (int i = 0; i < struct_def->member_count; i++) {
                                    if (strcmp(struct_def->members[i].name, member) == 0) {
                                        member_offset = struct_def->members[i].offset;
                                        member_type = struct_def->members[i].type;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    
                    if (member_type && node->children[1]->dataType && 
                        strcmp(member_type, node->children[1]->dataType) != 0) {
                        rhs_result = convertType(rhs_result, node->children[1]->dataType, member_type);
                    }
                    
                    char offset_str[32];
                    sprintf(offset_str, "%d", member_offset);
                    emit("STORE_OFFSET", struct_ptr, offset_str, rhs_result);
                    return struct_ptr;
                }
                else if (lhs->type == NODE_POSTFIX_EXPRESSION && strcmp(lhs->value, "[]") == 0) {
                    char* array = generate_ir(lhs->children[0]);
                    char* index = generate_ir(lhs->children[1]);
                    emit("ASSIGN_ARRAY", index, array, rhs_result); // Fixed parameter order
                    return array;
                }
                else if (lhs->type == NODE_UNARY_EXPRESSION && strcmp(lhs->value, "*") == 0) {
                    char* ptr = generate_ir(lhs->children[0]);
                    emit("ASSIGN_DEREF", rhs_result, ptr, "");
                    return ptr;
                }
                else {
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
            if (node->childCount >= 3) {
                char* cond = generate_ir(node->children[0]);
                char* true_val = generate_ir(node->children[1]);
                char* false_val = generate_ir(node->children[2]);
                
                char* temp = newTemp();
                emit("TERNARY", cond, true_val, temp);
                return temp;
            }
            return NULL;
        }

        case NODE_LOGICAL_OR_EXPRESSION:
        {
            char* true_label = newLabel();
            char* end_label = newLabel();
            char* result_temp = newTemp();  // Create ONE result temp for all branches
            
            TreeNode* left_node = node->children[0];
            if (left_node->type == NODE_LOGICAL_AND_EXPRESSION) {
                
                char* false_label = newLabel();  // Will evaluate right side (C)
                
                char* a_result = generate_ir(left_node->children[0]);
                const char* a_type = (left_node->children[0] && left_node->children[0]->dataType) ? 
                                    left_node->children[0]->dataType : "int";
                emitConditionalJump("IF_FALSE_GOTO", a_result, false_label, a_type);
                
                char* b_result = generate_ir(left_node->children[1]);
                const char* b_type = (left_node->children[1] && left_node->children[1]->dataType) ? 
                                    left_node->children[1]->dataType : "int";
                emitConditionalJump("IF_FALSE_GOTO", b_result, false_label, b_type);
                
                emit("ASSIGN", "1", "", result_temp);
                emit("GOTO", end_label, "", "");
                
                emit("LABEL", false_label, "", "");
                char* right = generate_ir(node->children[1]);
                const char* rightType = (node->children[1] && node->children[1]->dataType) ? 
                                       node->children[1]->dataType : "int";
                emitConditionalJump("IF_TRUE_GOTO", right, true_label, rightType);
                
                emit("ASSIGN", "0", "", result_temp);
                emit("GOTO", end_label, "", "");
                
                emit("LABEL", true_label, "", "");
                emit("ASSIGN", "1", "", result_temp);
                
                emit("LABEL", end_label, "", "");
            } else {
                char* left = generate_ir(left_node);
                
                const char* leftType = (left_node && left_node->dataType) ? 
                                      left_node->dataType : "int";
                emitConditionalJump("IF_TRUE_GOTO", left, true_label, leftType);
                
                char* right = generate_ir(node->children[1]);
                
                const char* rightType = (node->children[1] && node->children[1]->dataType) ? 
                                       node->children[1]->dataType : "int";
                emitConditionalJump("IF_TRUE_GOTO", right, true_label, rightType);
                
                emit("ASSIGN", "0", "", result_temp);
                emit("GOTO", end_label, "", "");
                
                emit("LABEL", true_label, "", "");
                emit("ASSIGN", "1", "", result_temp);
                
                emit("LABEL", end_label, "", "");
            }
            
            node->dataType = strdup("int");
            return result_temp;
        }

        case NODE_LOGICAL_AND_EXPRESSION:
        {
            char* false_label = newLabel();
            char* end_label = newLabel();
            char* result_temp = newTemp();
            
            char* left = generate_ir(node->children[0]);
            
            const char* leftType = (node->children[0] && node->children[0]->dataType) ? 
                                  node->children[0]->dataType : "int";
            emitConditionalJump("IF_FALSE_GOTO", left, false_label, leftType);
            
            char* right = generate_ir(node->children[1]);
            
            const char* rightType = (node->children[1] && node->children[1]->dataType) ? 
                                   node->children[1]->dataType : "int";
            emitConditionalJump("IF_FALSE_GOTO", right, false_label, rightType);
            
            emit("ASSIGN", "1", "", result_temp);
            emit("GOTO", end_label, "", "");
            
            emit("LABEL", false_label, "", "");
            emit("ASSIGN", "0", "", result_temp);
            
            emit("LABEL", end_label, "", "");
            
            node->dataType = strdup("int");
            return result_temp;
        }

        case NODE_INCLUSIVE_OR_EXPRESSION:
        {
            char* left = generate_ir(node->children[0]);
            char* right = generate_ir(node->children[1]);
            
            if (node->children[0]->dataType && node->children[1]->dataType) {
                char* common_type = usualArithConv(node->children[0]->dataType, node->children[1]->dataType);
                left = convertType(left, node->children[0]->dataType, common_type);
                right = convertType(right, node->children[1]->dataType, common_type);
                node->dataType = common_type;
            }
            
            char* temp = newTemp();
            emit("BITOR", left, right, temp);
            return temp;
        }

        case NODE_EXCLUSIVE_OR_EXPRESSION:
        {
            char* left = generate_ir(node->children[0]);
            char* right = generate_ir(node->children[1]);
            
            if (node->children[0]->dataType && node->children[1]->dataType) {
                char* common_type = usualArithConv(node->children[0]->dataType, node->children[1]->dataType);
                left = convertType(left, node->children[0]->dataType, common_type);
                right = convertType(right, node->children[1]->dataType, common_type);
                node->dataType = common_type;
            }
            
            char* temp = newTemp();
            emit("BITXOR", left, right, temp);
            return temp;
        }

        case NODE_AND_EXPRESSION:
        {
            char* left = generate_ir(node->children[0]);
            char* right = generate_ir(node->children[1]);
            
            if (node->children[0]->dataType && node->children[1]->dataType) {
                char* common_type = usualArithConv(node->children[0]->dataType, node->children[1]->dataType);
                left = convertType(left, node->children[0]->dataType, common_type);
                right = convertType(right, node->children[1]->dataType, common_type);
                node->dataType = common_type;
            }
            
            char* temp = newTemp();
            emit("BITAND", left, right, temp);
            return temp;
        }

        case NODE_EQUALITY_EXPRESSION:
        {
            char* left = generate_ir(node->children[0]);
            char* right = generate_ir(node->children[1]);
            
            if (node->children[0]->dataType && node->children[1]->dataType) {
                char* common_type = usualArithConv(node->children[0]->dataType, node->children[1]->dataType);
                left = convertType(left, node->children[0]->dataType, common_type);
                right = convertType(right, node->children[1]->dataType, common_type);
            }
            
            char* temp = newTemp();
            
            if (strcmp(node->value, "==") == 0) {
                emit("EQ", left, right, temp);
            } else if (strcmp(node->value, "!=") == 0) {
                emit("NE", left, right, temp);
            }
            
            node->dataType = strdup("int");
            return temp;
        }

        case NODE_RELATIONAL_EXPRESSION:
        {
            char* left = generate_ir(node->children[0]);
            char* right = generate_ir(node->children[1]);
            
            if (node->children[0]->dataType && node->children[1]->dataType) {
                char* common_type = usualArithConv(node->children[0]->dataType, node->children[1]->dataType);
                left = convertType(left, node->children[0]->dataType, common_type);
                right = convertType(right, node->children[1]->dataType, common_type);
            }
            
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
            
            node->dataType = strdup("int");
            return temp;
        }

        case NODE_SHIFT_EXPRESSION:
        {
            char* left = generate_ir(node->children[0]);
            char* right = generate_ir(node->children[1]);
            
            if (node->children[1]->dataType && !isIntegerType(node->children[1]->dataType)) {
                right = convertType(right, node->children[1]->dataType, "int");
            }
            
            char* temp = newTemp();
            
            if (strcmp(node->value, "<<") == 0) {
                emit("LSHIFT", left, right, temp);
            } else if (strcmp(node->value, ">>") == 0) {
                emit("RSHIFT", left, right, temp);
            }
            
            if (node->children[0]->dataType) {
                node->dataType = node->children[0]->dataType;
            }
            
            return temp;
        }

        case NODE_ADDITIVE_EXPRESSION:
        {
            char* left = generate_ir(node->children[0]);
            char* right = generate_ir(node->children[1]);
            
            bool is_pointer_arithmetic = false;
            bool is_ptr_ptr_subtraction = false;
            bool left_is_array_name = false;
            
            // Check if left operand is an array name (not subscripted)
            if (node->children[0]->type == NODE_IDENTIFIER) {
                Symbol* sym = lookupSymbol(node->children[0]->value);
                if (sym && sym->is_array) {
                    left_is_array_name = true;
                    is_pointer_arithmetic = true;
                }
            }
            
            if (node->children[0]->dataType && node->children[1]->dataType) {
                bool left_is_pointer = (strchr(node->children[0]->dataType, '*') != nullptr);
                bool right_is_pointer = (strchr(node->children[1]->dataType, '*') != nullptr);
                
                if ((left_is_pointer || isArrayType(node->children[0]->dataType)) &&
                    isIntegerType(node->children[1]->dataType)) {
                    is_pointer_arithmetic = true;
                }
                
                if (left_is_pointer && right_is_pointer && strcmp(node->value, "-") == 0) {
                    is_ptr_ptr_subtraction = true;
                }
            }
            
            if (strcmp(node->value, "+") == 0) {
                if (is_pointer_arithmetic) {
                    char* temp = newTemp();
                    // For array names with integer offset, use ARRAY_ADDR for better code generation
                    if (left_is_array_name) {
                        emit("ARRAY_ADDR", left, right, temp);
                    } else {
                        emit("PTR_ADD", left, right, temp);
                    }
                    return temp;
                } else {
                    if (node->children[0]->dataType && node->children[1]->dataType) {
                        char* result_type = usualArithConv(node->children[0]->dataType, node->children[1]->dataType);
                        left = convertType(left, node->children[0]->dataType, result_type);
                        right = convertType(right, node->children[1]->dataType, result_type);
                        if (!node->dataType) {
                            node->dataType = result_type;
                        }
                    }
                    char* temp = newTemp();
                    emitTyped("ADD", left, right, temp, node->dataType ? node->dataType : "int");
                    return temp;
                }
            } else if (strcmp(node->value, "-") == 0) {
                if (is_ptr_ptr_subtraction) {
                    char* temp = newTemp();
                    emit("PTR_SUB", left, right, temp);
                    return temp;
                } else if (is_pointer_arithmetic) {
                    char* temp = newTemp();
                    // For array names with integer offset, could use ARRAY_ADDR with negative index
                    // but PTR_SUB works fine too since the codegen handles it
                    if (left_is_array_name) {
                        // Convert arr - 3 to ARRAY_ADDR arr, -3, temp
                        // Need to negate the right operand
                        char negated[64];
                        if (isdigit(right[0]) || right[0] == '-') {
                            // Constant - can negate directly
                            int val = atoi(right);
                            sprintf(negated, "%d", -val);
                            emit("ARRAY_ADDR", left, negated, temp);
                        } else {
                            // Variable - use PTR_SUB
                            emit("PTR_SUB", left, right, temp);
                        }
                    } else {
                        emit("PTR_SUB", left, right, temp);
                    }
                    return temp;
                } else {
                    if (node->children[0]->dataType && node->children[1]->dataType) {
                        char* result_type = usualArithConv(node->children[0]->dataType, node->children[1]->dataType);
                        left = convertType(left, node->children[0]->dataType, result_type);
                        right = convertType(right, node->children[1]->dataType, result_type);
                        if (!node->dataType) {
                            node->dataType = result_type;
                        }
                    }
                    char* temp = newTemp();
                    emitTyped("SUB", left, right, temp, node->dataType ? node->dataType : "int");
                    return temp;
                }
            }
            return NULL;
        }

        case NODE_MULTIPLICATIVE_EXPRESSION:
        {
            char* left = generate_ir(node->children[0]);
            char* right = generate_ir(node->children[1]);
            
            if (node->children[0]->dataType && node->children[1]->dataType) {
                char* result_type = usualArithConv(node->children[0]->dataType, node->children[1]->dataType);
                left = convertType(left, node->children[0]->dataType, result_type);
                right = convertType(right, node->children[1]->dataType, result_type);
                if (!node->dataType) {
                    node->dataType = result_type;
                }
            }
            
            char* temp = newTemp();
            
            if (strcmp(node->value, "*") == 0) {
                emitTyped("MUL", left, right, temp, node->dataType ? node->dataType : "int");
            } else if (strcmp(node->value, "/") == 0) {
                emitTyped("DIV", left, right, temp, node->dataType ? node->dataType : "int");
            } else if (strcmp(node->value, "%") == 0) {
                emitTyped("MOD", left, right, temp, node->dataType ? node->dataType : "int");
            }
            return temp;
        }

        case NODE_CAST_EXPRESSION:
        {
            if (node->childCount >= 2) {
                TreeNode* type_node = node->children[0];
                TreeNode* expr_node = node->children[1];
                
                char* expr_result = generate_ir(expr_node);
                if (!expr_result) return NULL;
                
                const char* src_type = expr_node->dataType ? expr_node->dataType : "unknown";
                const char* tgt_type = node->dataType ? node->dataType : "unknown";
                
                char* temp = newTemp();
                
                if (strstr(tgt_type, "*") != NULL || strstr(src_type, "*") != NULL) {
                    char cast_notation[256];
                    snprintf(cast_notation, sizeof(cast_notation), "(%s)%s", tgt_type, expr_result);
                    emit("ASSIGN", cast_notation, "", temp);
                    return temp;
                }
                
                char cast_info[256];
                
                auto sanitize = [](const char* t, char* out, int size) {
                    int j = 0;
                    for (int i = 0; t[i] && j < size - 1; i++) {
                        char c = t[i];
                        if (c == ' ' || c == '*' || c == '&' || c == '[' || c == ']' || 
                            c == '(' || c == ')' || c == ',') {
                            out[j++] = '_';
                        } else {
                            out[j++] = c;
                        }
                    }
                    out[j] = '\0';
                };
                
                char from_buf[64], to_buf[64];
                sanitize(src_type, from_buf, sizeof(from_buf));
                sanitize(tgt_type, to_buf, sizeof(to_buf));
                sprintf(cast_info, "CAST_%s_to_%s", from_buf, to_buf);
                emit(cast_info, expr_result, "", temp);
                return temp;
            } else if (node->childCount > 0) {
                return generate_ir(node->children[0]);
            }
            return NULL;
        }

        case NODE_UNARY_EXPRESSION:
        {
            if (node->value && (node->value[0] == '-' || node->value[0] == '+') && 
                node->childCount == 1 && 
                (node->children[0]->type == NODE_CONSTANT || 
                 node->children[0]->type == NODE_INTEGER_CONSTANT ||
                 node->children[0]->type == NODE_HEX_CONSTANT ||
                 node->children[0]->type == NODE_OCTAL_CONSTANT ||
                 node->children[0]->type == NODE_BINARY_CONSTANT ||
                 node->children[0]->type == NODE_FLOAT_CONSTANT)) {
                return strdup(node->value);
            }
            
            if (strcmp(node->value, "++_pre") == 0) {
                char* operand = node->children[0]->value;
                if (isStaticVariable(operand)) {
                    operand = getStaticVarName(operand);
                }
                char* temp = newTemp();
                emit("ADD", operand, "1", temp);  
                emit("ASSIGN", temp, "", operand); 
                return temp;  
            }
            else if (strcmp(node->value, "--_pre") == 0) {
                char* operand = node->children[0]->value;
                if (isStaticVariable(operand)) {
                    operand = getStaticVarName(operand);
                }
                char* temp = newTemp();
                emit("SUB", operand, "1", temp);  
                emit("ASSIGN", temp, "", operand);
                return temp; 
            }
            else if (strcmp(node->value, "&") == 0) {
                TreeNode* operand_node = node->children[0];
                
                if (operand_node->type == NODE_POSTFIX_EXPRESSION && 
                    strcmp(operand_node->value, "[]") == 0) {
                    char* array = generate_ir(operand_node->children[0]);
                    char* index = generate_ir(operand_node->children[1]);
                    char* temp = newTemp();
                    emit("ARRAY_ADDR", array, index, temp);
                    return temp;
                }
                else {
                    char* operand = generate_ir(operand_node);
                    char* temp = newTemp();
                    emit("ADDR", operand, "", temp);
                    return temp;
                }
            }
            else if (strcmp(node->value, "*") == 0) {
                char* operand = generate_ir(node->children[0]);
                char* temp = newTemp();
                emit("DEREF", operand, "", temp);
                return temp;
            }
            else if (strcmp(node->value, "+") == 0) {
                return generate_ir(node->children[0]);
            }
            else if (strcmp(node->value, "-") == 0 || strcmp(node->value, "-_unary") == 0) {
                char* operand = generate_ir(node->children[0]);
                char* temp = newTemp();
                emit("NEG", operand, "", temp);
                if (node->children[0]->dataType) {
                    node->dataType = node->children[0]->dataType;
                }
                return temp;
            }
            else if (strcmp(node->value, "~") == 0) {
                char* operand = generate_ir(node->children[0]);
                char* temp = newTemp();
                emit("BITNOT", operand, "", temp);
                if (node->children[0]->dataType) {
                    node->dataType = node->children[0]->dataType;
                }
                return temp;
            }
            else if (strcmp(node->value, "!") == 0) {
                char* operand = generate_ir(node->children[0]);
                char* temp = newTemp();
                emit("NOT", operand, "", temp);
                node->dataType = strdup("int");
                return temp;
            }
            else if (strcmp(node->value, "sizeof") == 0) {
                char* temp = newTemp();
                char size_str[32];
                
                if (node->children[0]->dataType) {
                    sprintf(size_str, "%d", getTypeSize(node->children[0]->dataType));
                } else {
                    sprintf(size_str, "4");
                }
                
                emit("ASSIGN", size_str, "", temp);
                node->dataType = strdup("int");
                return temp;
            }
            return NULL;
        }

        case NODE_POSTFIX_EXPRESSION:
        {
            if (strcmp(node->value, "[]") == 0) {
                char* array = generate_ir(node->children[0]);
                char* index = generate_ir(node->children[1]);
                char* temp = newTemp();
                emit("ARRAY_ACCESS", array, index, temp);
                return temp;
            }
            else if (strcmp(node->value, "()") == 0) {
                char* func_name = NULL;
                int is_function_pointer = 0;
                
                if (node->children[0]->type == NODE_IDENTIFIER) {
                    func_name = node->children[0]->value;
                    
                    extern int isFunctionPointerName(const char* name);
                    is_function_pointer = isFunctionPointerName(func_name);
                } else {
                    func_name = generate_ir(node->children[0]);
                    is_function_pointer = 1;  // Assume it's a function pointer if it's an expression
                }
                
                int arg_count = 0;
                if (node->childCount > 1) {
                    TreeNode* args = node->children[1];
                    arg_count = args->childCount;
                    
                    #define MAX_FUNC_ARGS 32
                    char* arg_results[MAX_FUNC_ARGS];
                    
                    for (int i = 0; i < args->childCount && i < MAX_FUNC_ARGS; i++) {
                        arg_results[i] = generate_ir(args->children[i]);
                    }
                    
                    extern Symbol* lookupSymbol(const char* name);
                    Symbol* func_sym = NULL;
                    const char* func_type = NULL;
                    
                    if (func_name && !is_function_pointer) {
                        func_sym = lookupSymbol(func_name);
                    } else if (func_name && is_function_pointer) {
                        func_sym = lookupSymbol(func_name);
                        if (func_sym) {
                            func_type = func_sym->type;  // e.g., "void (*)(int &, char &, bool &)"
                        }
                    }
                    
                    for (int i = 0; i < args->childCount && i < MAX_FUNC_ARGS; i++) {
                        char* arg = arg_results[i];
                        int is_ref_param = 0;
                        
                        if (func_sym && func_sym->is_function && i < func_sym->param_count) {
                            const char* param_type = func_sym->param_types[i];
                            if (strstr(param_type, " &") != NULL || strstr(param_type, "&") == param_type + strlen(param_type) - 1) {
                                is_ref_param = 1;
                            }
                        } else if (func_type && is_function_pointer) {
                            const char* params_start = strchr(func_type, '(');
                            if (params_start) {
                                params_start = strchr(params_start + 1, '(');  // Find second '(' for parameter list
                                if (params_start) {
                                    const char* param_ptr = params_start + 1;
                                    int param_idx = 0;
                                    int paren_depth = 1;
                                    const char* param_start = param_ptr;
                                    
                                    while (*param_ptr && paren_depth > 0) {
                                        if (*param_ptr == '(') paren_depth++;
                                        else if (*param_ptr == ')') {
                                            paren_depth--;
                                            if (paren_depth == 0) {
                                                if (param_idx == i) {
                                                    while (param_start < param_ptr && *param_start == ' ') param_start++;
                                                    const char* param_end = param_ptr;
                                                    while (param_end > param_start && (*(param_end-1) == ' ' || *(param_end-1) == ')')) param_end--;
                                                    
                                                    for (const char* p = param_start; p < param_end; p++) {
                                                        if (*p == '&') {
                                                            is_ref_param = 1;
                                                            break;
                                                        }
                                                    }
                                                }
                                                break;
                                            }
                                        } else if (*param_ptr == ',' && paren_depth == 1) {
                                            if (param_idx == i) {
                                                while (param_start < param_ptr && *param_start == ' ') param_start++;
                                                const char* param_end = param_ptr;
                                                while (param_end > param_start && *(param_end-1) == ' ') param_end--;
                                                
                                                for (const char* p = param_start; p < param_end; p++) {
                                                    if (*p == '&') {
                                                        is_ref_param = 1;
                                                        break;
                                                    }
                                                }
                                                break;
                                            }
                                            param_idx++;
                                            param_start = param_ptr + 1;
                                        }
                                        param_ptr++;
                                    }
                                }
                            }
                        }
                        
                        if (is_ref_param) {
                            char* addr_temp = newTemp();
                            emit("ADDR", arg, "", addr_temp);
                            emit("PARAM", addr_temp, "", "");
                        } else {
                            int needs_load = 0;
                            if (arg && arg[0] != 't' && !isdigit(arg[0]) && arg[0] != '\'' && arg[0] != '"') {
                                extern Symbol* lookupSymbol(const char* name);
                                Symbol* arg_sym = lookupSymbol(arg);
                                if (arg_sym && !arg_sym->is_function) {
                                    needs_load = 1;
                                }
                            }
                            
                            if (needs_load) {
                                char* value_temp = newTemp();
                                emit("ASSIGN", arg, "", value_temp);
                                arg = value_temp;
                            }
                            
                            if (func_name && (strcmp(func_name, "printf") == 0 || 
                                             strcmp(func_name, "fprintf") == 0 || 
                                             strcmp(func_name, "sprintf") == 0) && 
                                args->children[i]->dataType && 
                                strcmp(args->children[i]->dataType, "float") == 0) {
                                char* promoted_arg = newTemp();
                                emit("FLOAT_TO_DOUBLE", arg, "", promoted_arg);
                                emit("PARAM", promoted_arg, "", "");
                            } else {
                                emit("PARAM", arg, "", "");
                            }
                        }
                    }
                }
                
                char arg_count_str[32];
                sprintf(arg_count_str, "%d", arg_count);
                
                bool is_void_function = false;
                extern Symbol* lookupSymbol(const char* name);
                
                if (func_name) {
                    Symbol* func_sym = lookupSymbol(func_name);
                    if (is_function_pointer && func_sym) {
                        char return_type_buf[128] = {0};
                        const char* lparen = strstr(func_sym->type, "(*");
                        if (lparen) {
                            int len = lparen - func_sym->type;
                            if (len > 0 && len < 127) {
                                strncpy(return_type_buf, func_sym->type, len);
                                return_type_buf[len] = '\0';
                                char* end = return_type_buf + len - 1;
                                while (end >= return_type_buf && (*end == ' ' || *end == '\t')) {
                                    *end = '\0';
                                    end--;
                                }
                                if (strcmp(return_type_buf, "void") == 0) {
                                    is_void_function = true;
                                }
                            }
                        }
                    } else if (!is_function_pointer && func_sym && func_sym->is_function) {
                        is_void_function = (strcmp(func_sym->return_type, "void") == 0);
                    }
                }
                
                if (is_void_function) {
                    if (is_function_pointer) {
                        emit("INDIRECT_CALL", func_name, arg_count_str, "");
                    } else {
                        emit("CALL", func_name, arg_count_str, "");
                    }
                    return NULL;  // No result to return
                } else {
                    char* temp = newTemp();
                    
                    char return_type_buf[128] = {0};
                    Symbol* func_sym = lookupSymbol(func_name);
                    
                    if (is_function_pointer) {
                        emit("INDIRECT_CALL", func_name, arg_count_str, temp);
                        
                        if (func_sym && func_sym->type) {
                            const char* lparen = strstr(func_sym->type, "(*");
                            if (lparen) {
                                int len = lparen - func_sym->type;
                                if (len > 0 && len < 127) {
                                    strncpy(return_type_buf, func_sym->type, len);
                                    return_type_buf[len] = '\0';
                                    char* end = return_type_buf + len - 1;
                                    while (end >= return_type_buf && (*end == ' ' || *end == '\t')) {
                                        *end = '\0';
                                        end--;
                                    }
                                    if (strlen(return_type_buf) > 0) {
                                        node->dataType = strdup(return_type_buf);
                                    }
                                }
                            }
                        }
                    } else {
                        emit("CALL", func_name, arg_count_str, temp);
                        
                        if (func_sym && func_sym->is_function && func_sym->return_type) {
                            node->dataType = strdup(func_sym->return_type);
                        }
                    }
                    return temp;
                }
            }
            else if (strcmp(node->value, ".") == 0) {
                char* struct_var = NULL;
                TreeNode* base = node->children[0];
                
                if (base->type == NODE_POSTFIX_EXPRESSION && strcmp(base->value, "[]") == 0) {
                    char* array = generate_ir(base->children[0]);
                    char* index = generate_ir(base->children[1]);
                    char* member = node->children[1]->value;
                    
                    int member_offset = 0;
                    int struct_size = 0;
                    
                    if (base->dataType) {
                        // CRITICAL: Resolve typedef to get actual struct type
                        char* resolved_type = resolveTypedef(base->dataType);
                        const char* actual_type = resolved_type ? resolved_type : base->dataType;
                        
                        if (strncmp(actual_type, "struct ", 7) == 0) {
                            const char* struct_name = actual_type + 7;
                            extern StructDef* lookupStruct(const char* name);
                            StructDef* struct_def = lookupStruct(struct_name);
                            
                            if (struct_def) {
                                struct_size = struct_def->total_size;
                                for (int i = 0; i < struct_def->member_count; i++) {
                                    if (strcmp(struct_def->members[i].name, member) == 0) {
                                        member_offset = struct_def->members[i].offset;
                                        break;
                                    }
                                }
                            }
                        }
                        
                        if (resolved_type) free(resolved_type);
                    }
                    
                    char* elem_addr = newTemp();
                    if (struct_size > 0) {
                        char* scaled_index = newTemp();
                        char size_str[32];
                        sprintf(size_str, "%d", struct_size);
                        emit("MUL", index, size_str, scaled_index);
                        emit("ADD", array, scaled_index, elem_addr);
                    } else {
                        emit("ARRAY_ADDR", array, index, elem_addr);
                    }
                    
                    char offset_str[32];
                    sprintf(offset_str, "%d", member_offset);
                    char* temp = newTemp();
                    emit("LOAD_OFFSET", elem_addr, offset_str, temp);
                    return temp;
                } else {
                    char* member = node->children[1]->value;
                    
                    if (base->type == NODE_POSTFIX_EXPRESSION && strcmp(base->value, ".") == 0) {
                        char* base_path;
                        if (base->children[0]->type == NODE_IDENTIFIER) {
                            base_path = base->children[0]->value;
                        } else {
                            base_path = generate_ir(base->children[0]);
                        }
                        char* base_member = base->children[1]->value;
                        
                        static char full_path[256];
                        snprintf(full_path, sizeof(full_path), "%s.%s.%s", base_path, base_member, member);
                        
                        char* temp = newTemp();
                        emit("ASSIGN", full_path, "", temp);
                        
                        return temp;
                    }
                    
                    char* struct_var = base->value;
                    
                    int member_offset = 0;
                    extern Symbol* lookupSymbol(const char* name);
                    Symbol* sym = lookupSymbol(struct_var);
                    if (sym && sym->type) {
                        // CRITICAL: Resolve typedef to get actual struct/union type
                        char* resolved_type = resolveTypedef(sym->type);
                        const char* actual_type = resolved_type ? resolved_type : sym->type;
                        
                        if (strncmp(actual_type, "struct ", 7) == 0) {
                            const char* struct_name = actual_type + 7;
                            extern StructDef* lookupStruct(const char* name);
                            StructDef* struct_def = lookupStruct(struct_name);
                            if (struct_def) {
                                for (int i = 0; i < struct_def->member_count; i++) {
                                    if (strcmp(struct_def->members[i].name, member) == 0) {
                                        node->dataType = strdup(struct_def->members[i].type);
                                        member_offset = struct_def->members[i].offset;
                                        break;
                                    }
                                }
                            }
                        }
                        else if (strncmp(actual_type, "union ", 6) == 0) {
                            const char* union_name = actual_type + 6;
                            extern StructDef* lookupUnion(const char* name);
                            StructDef* union_def = lookupUnion(union_name);
                            if (union_def) {
                                for (int i = 0; i < union_def->member_count; i++) {
                                    if (strcmp(union_def->members[i].name, member) == 0) {
                                        node->dataType = strdup(union_def->members[i].type);
                                        member_offset = union_def->members[i].offset;
                                        break;
                                    }
                                }
                            }
                        }
                        
                        if (resolved_type) free(resolved_type);
                    }
                    
                    char* struct_addr = newTemp();
                    emit("ADDR", struct_var, "", struct_addr);
                    
                    char offset_str[32];
                    sprintf(offset_str, "%d", member_offset);
                    char* temp = newTemp();
                    emit("LOAD_OFFSET", struct_addr, offset_str, temp);
                    return temp;
                }
            }
            else if (strcmp(node->value, "->") == 0) {
                char* struct_ptr = generate_ir(node->children[0]);
                char* member = node->children[1]->value;
                
                int member_offset = 0;
                
                TreeNode* ptr_node = node->children[0];
                if (ptr_node->dataType && strstr(ptr_node->dataType, "*")) {
                    char struct_type[128];
                    strncpy(struct_type, ptr_node->dataType, sizeof(struct_type) - 1);
                    struct_type[sizeof(struct_type) - 1] = '\0';
                    
                    char* ptr_char = strrchr(struct_type, '*');
                    if (ptr_char) *ptr_char = '\0';
                    
                    int len = strlen(struct_type);
                    while (len > 0 && struct_type[len-1] == ' ') {
                        struct_type[--len] = '\0';
                    }
                    
                    // CRITICAL: Resolve typedef to get actual struct type
                    char* resolved_type = resolveTypedef(struct_type);
                    const char* actual_type = resolved_type ? resolved_type : struct_type;
                    
                    if (strncmp(actual_type, "struct ", 7) == 0) {
                        const char* struct_name = actual_type + 7;
                        extern StructDef* lookupStruct(const char* name);
                        StructDef* struct_def = lookupStruct(struct_name);
                        
                        if (struct_def) {
                            for (int i = 0; i < struct_def->member_count; i++) {
                                if (strcmp(struct_def->members[i].name, member) == 0) {
                                    member_offset = struct_def->members[i].offset;
                                    break;
                                }
                            }
                        }
                    }
                    
                    if (resolved_type) free(resolved_type);
                }
                
                char offset_str[32];
                sprintf(offset_str, "%d", member_offset);
                char* temp = newTemp();
                emit("LOAD_OFFSET", struct_ptr, offset_str, temp);
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
            if (node->childCount > 0) {
                return generate_ir(node->children[0]);
            }
            return node->value ? strdup(node->value) : NULL;
        }

        case NODE_INITIALIZER:
        {
            if (node->childCount > 1 && strcmp(node->value, "=") == 0) {
                
                TreeNode* declarator = node->children[0];
                char* lhs_name = NULL;
                
                lhs_name = (char*)extractIdentifierFromDeclarator(declarator);
                
                if (!lhs_name) {
                    lhs_name = declarator->value; // fallback
                }
                
                // Check if this is a static variable or global variable (need to register in DATA section)
                bool isStatic = isStaticVariable(lhs_name);
                bool isGlobal = false;
                
                if (!isStatic) {
                    // Check if it's a global variable (scope_level == 0)
                    extern Symbol* lookupSymbol(const char* name);
                    Symbol* sym = lookupSymbol(lhs_name);
                    if (sym && sym->scope_level == 0 && !sym->is_function) {
                        isGlobal = true;
                    }
                }
                
                if (isStatic || isGlobal) {
                    TreeNode* rhs = node->children[1];
                    
                    char* init_value = NULL;
                    if (rhs->childCount == 0 && rhs->value) {
                        init_value = rhs->value;
                    } else if (rhs->childCount > 0) {
                        init_value = generate_ir(rhs);
                    }
                    
                    // For static variables, use the scoped name; for globals, use the plain name
                    char* var_name_for_data;
                    if (isStatic) {
                        var_name_for_data = getStaticVarName(lhs_name);
                    } else {
                        var_name_for_data = strdup(lhs_name);
                    }
                    
                    registerStaticVar(var_name_for_data, init_value ? init_value : "0");
                    
                    free(var_name_for_data);
                    
                    for (int i = 2; i < node->childCount; i++) {
                        generate_ir(node->children[i]);
                    }
                    
                    return lhs_name;
                }
                
                TreeNode* rhs = node->children[1];
                
                if (rhs->childCount > 1 && rhs->type == NODE_INITIALIZER) {
                    extern Symbol* lookupSymbol(const char* name);
                    Symbol* sym = lookupSymbol(lhs_name);
                    
                    bool is_struct_init = false;
                    StructDef* struct_def = NULL;
                    
                    if (sym && strncmp(sym->type, "struct ", 7) == 0) {
                        is_struct_init = true;
                        const char* struct_name = sym->type + 7;
                        extern StructDef* lookupStruct(const char* name);
                        struct_def = lookupStruct(struct_name);
                    }
                    
                    if (is_struct_init && struct_def) {
                        char* struct_addr = newTemp();
                        emit("ADDR", lhs_name, "", struct_addr);
                        
                        for (int i = 0; i < rhs->childCount && i < struct_def->member_count; i++) {
                            char* value = generate_ir(rhs->children[i]);
                            int member_offset = struct_def->members[i].offset;
                            char offset_str[32];
                            sprintf(offset_str, "%d", member_offset);
                            emit("STORE_OFFSET", struct_addr, offset_str, value);
                        }
                    } else {
                        extern Symbol* lookupSymbol(const char* name);
                        Symbol* sym = lookupSymbol(lhs_name);
                        int array_size = 0;
                        
                        if (sym && sym->is_array && sym->num_dims > 0) {
                            array_size = sym->array_dims[0];
                        }
                        
                        for (int i = 0; i < rhs->childCount; i++) {
                            char* value = generate_ir(rhs->children[i]);
                            char index_str[32];
                            sprintf(index_str, "%d", i);
                            emit("ASSIGN_ARRAY", index_str, lhs_name, value);
                        }
                        
                        if (array_size > 0 && rhs->childCount < array_size) {
                            for (int i = rhs->childCount; i < array_size; i++) {
                                char index_str[32];
                                sprintf(index_str, "%d", i);
                                emit("ASSIGN_ARRAY", index_str, lhs_name, "0");
                            }
                        }
                    }
                    
                    for (int i = 2; i < node->childCount; i++) {
                        generate_ir(node->children[i]);
                    }
                    
                    return lhs_name;
                } else {
                    char* rhs_result = generate_ir(rhs);
                    if (lhs_name && rhs_result) {
                        extern Symbol* lookupSymbol(const char* name);
                        Symbol* sym = lookupSymbol(lhs_name);
                        
                        char* resolved_type = sym ? resolveTypedef(sym->type) : NULL;
                        if (resolved_type && strstr(resolved_type, "char[") != NULL && 
                            rhs->type == NODE_STRING_LITERAL && rhs_result) {
                            const char* str_value = rhs_result;
                            if (str_value[0] == '"') {
                                str_value++; // Skip opening quote
                            }
                            
                            int idx = 0;
                            while (*str_value && *str_value != '"') {
                                char char_literal[8];
                                if (*str_value == '\\' && *(str_value + 1)) {
                                    str_value++;
                                    switch (*str_value) {
                                        case 'n': sprintf(char_literal, "'\\n'"); break;
                                        case 't': sprintf(char_literal, "'\\t'"); break;
                                        case 'r': sprintf(char_literal, "'\\r'"); break;
                                        case '0': sprintf(char_literal, "'\\0'"); break;
                                        case '\\': sprintf(char_literal, "'\\\\'"); break;
                                        case '"': sprintf(char_literal, "'\\\"'"); break;
                                        default: sprintf(char_literal, "'%c'", *str_value); break;
                                    }
                                } else {
                                    sprintf(char_literal, "'%c'", *str_value);
                                }
                                
                                char index_str[32];
                                sprintf(index_str, "%d", idx);
                                emit("ASSIGN_ARRAY", index_str, lhs_name, char_literal);
                                idx++;
                                str_value++;
                            }
                            
                            char index_str[32];
                            sprintf(index_str, "%d", idx);
                            emit("ASSIGN_ARRAY", index_str, lhs_name, "'\\0'");
                            free(resolved_type);
                        }
                        else if (sym && sym->type && rhs->dataType && 
                                 strcmp(sym->type, rhs->dataType) != 0 &&
                                 strstr(sym->type, "[") == NULL && strstr(rhs->dataType, "[") == NULL) {
                            if (resolved_type) free(resolved_type);
                            rhs_result = convertType(rhs_result, rhs->dataType, sym->type);
                            // Check if this is a reference variable initialization
                            if (sym && sym->is_reference) {
                                // Reference initialization: store ADDRESS of rhs into the reference
                                char* addr_temp = newTemp();
                                emit("ADDR", rhs_result, "", addr_temp);
                                emit("ASSIGN", addr_temp, "", lhs_name);
                            } else {
                                emit("ASSIGN", rhs_result, "", lhs_name);
                            }
                        }
                        else {
                            if (resolved_type) free(resolved_type);
                            // Check if this is a reference variable initialization
                            if (sym && sym->is_reference) {
                                // Reference initialization: store ADDRESS of rhs into the reference
                                char* addr_temp = newTemp();
                                emit("ADDR", rhs_result, "", addr_temp);
                                emit("ASSIGN", addr_temp, "", lhs_name);
                            } else {
                                emit("ASSIGN", rhs_result, "", lhs_name);
                            }
                        }
                    }
                    
                    for (int i = 2; i < node->childCount; i++) {
                        generate_ir(node->children[i]);
                    }
                    
                    return lhs_name;
                }
            } 
            else if (node->childCount > 0) {
                return generate_ir(node->children[0]);
            }
            return NULL;
        }

        case NODE_MARKER:
        {
            for (int i = 0; i < node->childCount; i++) {
                generate_ir(node->children[i]);
            }
            return NULL;
        }

        
        default:
        {
            for (int i = 0; i < node->childCount; i++) {
                generate_ir(node->children[i]);
            }
            return NULL;
        }
    }
}   