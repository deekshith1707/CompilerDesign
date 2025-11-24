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
#include "basic_block.h"
#include "mips_codegen.h"

using namespace std;

extern int yyparse();
extern FILE* yyin;
extern int error_count;
extern TreeNode* ast_root;

int main(int argc, char **argv) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <input_file> [options]" << endl;
        cerr << "Options:" << endl;
        cerr << "  --analyze-blocks       : Perform basic block analysis and print results" << endl;
        cerr << "  --activation-records   : Compute and print activation records for functions" << endl;
        cerr << "  --generate-mips        : Generate MIPS assembly code" << endl;
        return 1;
    }
    
    // Check for optional flags
    bool analyzeBlocks = false;
    bool computeActivationRecs = false;
    bool generateMIPS = false;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--analyze-blocks") == 0) {
            analyzeBlocks = true;
        } else if (strcmp(argv[i], "--activation-records") == 0) {
            computeActivationRecs = true;
        } else if (strcmp(argv[i], "--generate-mips") == 0) {
            generateMIPS = true;
            cout << "MIPS generation flag detected" << endl;
        }
    }
    
    yyin = fopen(argv[1], "r");
    if (!yyin) {
        cerr << "Error: Cannot open file " << argv[1] << endl;
        return 1;
    }

    // cout << "[" << argv[1] << "]" << endl;

    if (yyparse() == 0 && error_count == 0) {
        // printSymbolTable();  // Commented for clean MIPS output
        if (ast_root) {
            generate_ir(ast_root);

            string inputFile(argv[1]);
            string outputFile;
            size_t lastDot = inputFile.find_last_of('.');
            if (lastDot != string::npos) {
                outputFile = inputFile.substr(0, lastDot) + ".ir";
            } else {
                outputFile = inputFile + ".ir";
            }
            
            printIR(outputFile.c_str());
            
            // Perform basic block analysis if requested
            if (analyzeBlocks) {
                cout << "\n=== PERFORMING BASIC BLOCK ANALYSIS ===" << endl;
                analyzeIR();
                printBasicBlocks();
                printNextUseInfo();
                cout << "\n=== BASIC BLOCK ANALYSIS COMPLETED ===" << endl;
            }
            
            // Compute activation records if requested
            if (computeActivationRecs) {
                cout << "\n=== COMPUTING ACTIVATION RECORDS ===" << endl;
                testActivationRecords();
                cout << "\n=== ACTIVATION RECORDS COMPUTATION COMPLETED ===" << endl;
            }
            
            // Generate MIPS assembly code if requested
            if (generateMIPS) {
                // Generate .s filename from input file (same pattern as .ir)
                string asmFile;
                if (lastDot != string::npos) {
                    asmFile = inputFile.substr(0, lastDot) + ".s";
                } else {
                    asmFile = inputFile + ".s";
                }
                testMIPSCodeGeneration(asmFile.c_str());
            }
        } else {
            cout << "\n=== PARSING SUCCEEDED BUT NO AST WAS GENERATED ===" << endl;
        }

    } else {
        cout << "=== PARSING FAILED ===" << endl;
        if (error_count > 0) {
            cout << "Total errors: " << error_count << endl;
        }
    }

    fclose(yyin);
    return (error_count > 0) ? 1 : 0;
}
