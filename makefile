CC = gcc
CFLAGS = -Wall
LEX = flex
LEXFLAGS = 

TARGET = lexer
SRC_DIR = src
OBJ_DIR = obj

LEXER_SRC = $(SRC_DIR)/lexer.l
LEXER_GEN_SRC = $(OBJ_DIR)/lex.yy.c
LEXER_GEN_OBJ = $(OBJ_DIR)/lex.yy.o

OBJECTS = $(LEXER_GEN_OBJ)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/lex.yy.o: $(LEXER_GEN_SRC)
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(LEXER_GEN_SRC): $(LEXER_SRC)
	@mkdir -p $(OBJ_DIR)
	$(LEX) $(LEXFLAGS) -o $@ $<

clean:
	rm -rf $(OBJ_DIR) $(TARGET)
