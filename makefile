CC = gcc
CFLAGS = -Wall
LEX = flex
LEXFLAGS = 
YACC = bison
YACCFLAGS = -d -v

TARGET = syntax_analyzer
SRC_DIR = src
OBJ_DIR = obj

LEXER_SRC = $(SRC_DIR)/lexer.l
PARSER_SRC = $(SRC_DIR)/parser.y

LEXER_GEN_SRC = $(OBJ_DIR)/lex.yy.c
PARSER_GEN_SRC = $(OBJ_DIR)/parser.tab.c
PARSER_GEN_HDR = $(OBJ_DIR)/parser.tab.h

LEXER_GEN_OBJ = $(OBJ_DIR)/lex.yy.o
PARSER_GEN_OBJ = $(OBJ_DIR)/parser.tab.o

OBJECTS = $(LEXER_GEN_OBJ) $(PARSER_GEN_OBJ)

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