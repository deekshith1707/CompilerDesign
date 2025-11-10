CC = g++
CFLAGS = -Wall -I$(SRC_DIR) -std=c++11 -Wno-unused-variable -Wno-switch -Wno-address -Wno-unused-function -Wno-sign-compare -Wno-format-overflow
LEX = flex
LEXFLAGS = 
YACC = bison
YACCFLAGS = -d -v -Wno-conflicts-sr -Wno-conflicts-rr

TARGET = ir_generator
SRC_DIR = src
OBJ_DIR = obj

LEXER_SRC = $(SRC_DIR)/lexer.l
PARSER_SRC = $(SRC_DIR)/parser.y

# Source files for the refactored modules
CPP_SOURCES = $(SRC_DIR)/main.cpp $(SRC_DIR)/ast.cpp $(SRC_DIR)/symbol_table.cpp $(SRC_DIR)/ir_context.cpp $(SRC_DIR)/ir_generator.cpp $(SRC_DIR)/basic_block.cpp $(SRC_DIR)/mips_codegen.cpp

LEXER_GEN_SRC = $(OBJ_DIR)/lex.yy.c
PARSER_GEN_SRC = $(OBJ_DIR)/parser.tab.c
PARSER_GEN_HDR = $(OBJ_DIR)/parser.tab.h

LEXER_GEN_OBJ = $(OBJ_DIR)/lex.yy.o
PARSER_GEN_OBJ = $(OBJ_DIR)/parser.tab.o

# Object files for the refactored modules
CPP_OBJECTS = $(OBJ_DIR)/main.o $(OBJ_DIR)/ast.o $(OBJ_DIR)/symbol_table.o $(OBJ_DIR)/ir_context.o $(OBJ_DIR)/ir_generator.o $(OBJ_DIR)/basic_block.o $(OBJ_DIR)/mips_codegen.o

OBJECTS = $(LEXER_GEN_OBJ) $(PARSER_GEN_OBJ) $(CPP_OBJECTS)

.PHONY: all clean help

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ -lfl

$(LEXER_GEN_OBJ): $(LEXER_GEN_SRC)
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(PARSER_GEN_OBJ): $(PARSER_GEN_SRC)
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(LEXER_GEN_SRC): $(LEXER_SRC) $(PARSER_GEN_HDR)
	@mkdir -p $(OBJ_DIR)
	$(LEX) $(LEXFLAGS) -o $@ $<

$(PARSER_GEN_SRC) $(PARSER_GEN_HDR): $(PARSER_SRC)
	@mkdir -p $(OBJ_DIR)
	cd $(OBJ_DIR) && $(YACC) $(YACCFLAGS) ../$(PARSER_SRC)

# Compilation rules for the refactored modules
$(OBJ_DIR)/main.o: $(SRC_DIR)/main.cpp
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/ast.o: $(SRC_DIR)/ast.cpp $(SRC_DIR)/ast.h
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/symbol_table.o: $(SRC_DIR)/symbol_table.cpp $(SRC_DIR)/symbol_table.h
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/ir_context.o: $(SRC_DIR)/ir_context.cpp $(SRC_DIR)/ir_context.h
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/ir_generator.o: $(SRC_DIR)/ir_generator.cpp $(SRC_DIR)/ir_generator.h
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/basic_block.o: $(SRC_DIR)/basic_block.cpp $(SRC_DIR)/basic_block.h
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/mips_codegen.o: $(SRC_DIR)/mips_codegen.cpp $(SRC_DIR)/mips_codegen.h
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

help:
	@echo "Available targets:"
	@echo "  all       - Build the IR Generator(default)"
	@echo "  clean     - Remove generated files"
	@echo "  help      - Show this help message"
	@echo ""
	@echo "Usage: make [target]"
	@echo "       ./$(TARGET) <input_file>"
