#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "ir_generator.h"
#include "ir_context.h"
#include "symbol_table.h"

extern int yyparse();
extern FILE* yyin;
extern int error_count;
extern TreeNode* ast_root; // The global pointer set by the parser

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }
    
    yyin = fopen(argv[1], "r");
    if (!yyin) {
        fprintf(stderr, "Error: Cannot open file %s\n", argv[1]);
        return 1;
    }

    printf("============================================\n");
    printf("      C COMPILER - SYNTAX ANALYZER\n");
    printf("============================================\n");
    printf("Input file: %s\n", argv[1]);
    printf("============================================\n");

    if (yyparse() == 0 && error_count == 0) { // yyparse() returns 0 on success
        printf("\n=== PARSING SUCCESSFUL ===\n");
        printSymbolTable();

        // === NEW IR GENERATION STEP ===
        if (ast_root) {
            printf("\n=== GENERATING IR CODE ===\n");
            generate_ir(ast_root); // Pass the root of your AST

            // Create output file name
            char output_file[256];
            char* last_dot = strrchr(argv[1], '.');
            if (last_dot) {
                int base_length = last_dot - argv[1];
                snprintf(output_file, sizeof(output_file), "%.*s.ir", base_length, argv[1]);
            } else {
                snprintf(output_file, sizeof(output_file), "%s.ir", argv[1]);
            }
            
            printIR(output_file);
            printf("\n=== CODE GENERATION COMPLETED ===\n");
        } else {
            printf("\n=== PARSING SUCCEEDED BUT NO AST WAS GENERATED ===\n");
        }

    } else {
        printf("\n=== PARSING FAILED ===\n");
        printf("Total errors: %d\n", error_count);
    }

    fclose(yyin);
    // freeNode(ast_root); // Good idea to add this for memory cleanup
    return (error_count > 0) ? 1 : 0;
}
