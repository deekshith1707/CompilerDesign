#include <stdio.h>
#include "src/ir_context.h"

int main() {
    extern Quadruple IR[];
    extern int irCount;
    
    // Read IR from file (manually)
    FILE* f = fopen("testreturn/6_switch_cases.ir", "r");
    char line[512];
    int count = 0;
    
    while (fgets(line, sizeof(line), f) && count < 250) {
        // Look for SWITCH labels
        if (strstr(line, "SWITCH_7")) {
            printf("IR line %d: %s", count, line);
        }
        count++;
    }
    fclose(f);
    
    return 0;
}
