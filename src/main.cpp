#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "ast.h"
#include "ir_generator.h"
#include "ir_context.h"
#include "symbol_table.h"

using namespace std;

extern int yyparse();
extern FILE* yyin;
extern int error_count;
extern TreeNode* ast_root; // The global pointer set by the parser

int main(int argc, char **argv) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <input_file>" << endl;
        return 1;
    }
    
    yyin = fopen(argv[1], "r");
    if (!yyin) {
        cerr << "Error: Cannot open file " << argv[1] << endl;
        return 1;
    }

    cout << "============================================" << endl;
    cout << "      C++ COMPILER - SYNTAX ANALYZER" << endl;
    cout << "============================================" << endl;
    cout << "Input file: " << argv[1] << endl;
    cout << "============================================" << endl;

    if (yyparse() == 0 && error_count == 0) { // yyparse() returns 0 on success
        cout << "\n=== PARSING SUCCESSFUL ===" << endl;
        printSymbolTable();

        // === NEW IR GENERATION STEP ===
        if (ast_root) {
            cout << "\n=== GENERATING IR CODE ===" << endl;
            generate_ir(ast_root); // Pass the root of your AST

            // Create output file name
            string inputFile(argv[1]);
            string outputFile;
            size_t lastDot = inputFile.find_last_of('.');
            if (lastDot != string::npos) {
                outputFile = inputFile.substr(0, lastDot) + ".ir";
            } else {
                outputFile = inputFile + ".ir";
            }
            
            printIR(outputFile.c_str());
            cout << "\n=== CODE GENERATION COMPLETED ===" << endl;
        } else {
            cout << "\n=== PARSING SUCCEEDED BUT NO AST WAS GENERATED ===" << endl;
        }

    } else {
        cout << "\n=== PARSING FAILED ===" << endl;
        cout << "Total errors: " << error_count << endl;
    }

    fclose(yyin);
    // freeNode(ast_root); // Good idea to add this for memory cleanup
    return (error_count > 0) ? 1 : 0;
}
