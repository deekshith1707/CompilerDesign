CC = gcc
CFLAGS = -Wall -I$(SRC_DIR)
LEX = flex
LEXFLAGS = 
YACC = bison
YACCFLAGS = -d -v

TARGET = syntax_analyzer
SRC_DIR = src
OBJ_DIR = obj

LEXER_SRC = $(SRC_DIR)/lexer.l
PARSER_SRC = $(SRC_DIR)/parser.y

# Source files for the refactored modules
C_SOURCES = $(SRC_DIR)/main.c $(SRC_DIR)/ast.c $(SRC_DIR)/symbol_table.c $(SRC_DIR)/ir_context.c $(SRC_DIR)/ir_generator.c

LEXER_GEN_SRC = $(OBJ_DIR)/lex.yy.c
PARSER_GEN_SRC = $(OBJ_DIR)/parser.tab.c
PARSER_GEN_HDR = $(OBJ_DIR)/parser.tab.h

LEXER_GEN_OBJ = $(OBJ_DIR)/lex.yy.o
PARSER_GEN_OBJ = $(OBJ_DIR)/parser.tab.o

# Object files for the refactored modules
C_OBJECTS = $(OBJ_DIR)/main.o $(OBJ_DIR)/ast.o $(OBJ_DIR)/symbol_table.o $(OBJ_DIR)/ir_context.o $(OBJ_DIR)/ir_generator.o

OBJECTS = $(LEXER_GEN_OBJ) $(PARSER_GEN_OBJ) $(C_OBJECTS)

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
$(OBJ_DIR)/main.o: $(SRC_DIR)/main.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/ast.o: $(SRC_DIR)/ast.c $(SRC_DIR)/ast.h
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/symbol_table.o: $(SRC_DIR)/symbol_table.c $(SRC_DIR)/symbol_table.h
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/ir_context.o: $(SRC_DIR)/ir_context.c $(SRC_DIR)/ir_context.h
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/ir_generator.o: $(SRC_DIR)/ir_generator.c $(SRC_DIR)/ir_generator.h
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

help:
	@echo "Available targets:"
	@echo "  all       - Build the syntax analyzer (default)"
	@echo "  clean     - Remove generated files"
	@echo "  help      - Show this help message"
	@echo ""
	@echo "Usage: make [target]"
	@echo "       ./$(TARGET) <input_file>"