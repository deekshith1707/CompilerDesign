/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_YY_PARSER_TAB_H_INCLUDED
# define YY_YY_PARSER_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif
/* "%code requires" blocks.  */
#line 659 "../src/parser.y"

    #include "ast.h"
    #include "symbol_table.h"

#line 54 "parser.tab.h"

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    IDENTIFIER = 258,              /* IDENTIFIER  */
    TYPE_NAME = 259,               /* TYPE_NAME  */
    INTEGER_CONSTANT = 260,        /* INTEGER_CONSTANT  */
    HEX_CONSTANT = 261,            /* HEX_CONSTANT  */
    OCTAL_CONSTANT = 262,          /* OCTAL_CONSTANT  */
    BINARY_CONSTANT = 263,         /* BINARY_CONSTANT  */
    FLOAT_CONSTANT = 264,          /* FLOAT_CONSTANT  */
    STRING_LITERAL = 265,          /* STRING_LITERAL  */
    CHAR_CONSTANT = 266,           /* CHAR_CONSTANT  */
    PREPROCESSOR = 267,            /* PREPROCESSOR  */
    IF = 268,                      /* IF  */
    ELSE = 269,                    /* ELSE  */
    WHILE = 270,                   /* WHILE  */
    FOR = 271,                     /* FOR  */
    DO = 272,                      /* DO  */
    SWITCH = 273,                  /* SWITCH  */
    CASE = 274,                    /* CASE  */
    DEFAULT = 275,                 /* DEFAULT  */
    BREAK = 276,                   /* BREAK  */
    CONTINUE = 277,                /* CONTINUE  */
    RETURN = 278,                  /* RETURN  */
    INT = 279,                     /* INT  */
    CHAR_TOKEN = 280,              /* CHAR_TOKEN  */
    FLOAT_TOKEN = 281,             /* FLOAT_TOKEN  */
    DOUBLE = 282,                  /* DOUBLE  */
    LONG = 283,                    /* LONG  */
    SHORT = 284,                   /* SHORT  */
    UNSIGNED = 285,                /* UNSIGNED  */
    SIGNED = 286,                  /* SIGNED  */
    VOID = 287,                    /* VOID  */
    BOOL = 288,                    /* BOOL  */
    STRUCT = 289,                  /* STRUCT  */
    ENUM = 290,                    /* ENUM  */
    UNION = 291,                   /* UNION  */
    TYPEDEF = 292,                 /* TYPEDEF  */
    STATIC = 293,                  /* STATIC  */
    EXTERN = 294,                  /* EXTERN  */
    AUTO = 295,                    /* AUTO  */
    REGISTER = 296,                /* REGISTER  */
    CONST = 297,                   /* CONST  */
    VOLATILE = 298,                /* VOLATILE  */
    GOTO = 299,                    /* GOTO  */
    UNTIL = 300,                   /* UNTIL  */
    SIZEOF = 301,                  /* SIZEOF  */
    ASSIGN = 302,                  /* ASSIGN  */
    PLUS_ASSIGN = 303,             /* PLUS_ASSIGN  */
    MINUS_ASSIGN = 304,            /* MINUS_ASSIGN  */
    MUL_ASSIGN = 305,              /* MUL_ASSIGN  */
    DIV_ASSIGN = 306,              /* DIV_ASSIGN  */
    MOD_ASSIGN = 307,              /* MOD_ASSIGN  */
    AND_ASSIGN = 308,              /* AND_ASSIGN  */
    OR_ASSIGN = 309,               /* OR_ASSIGN  */
    XOR_ASSIGN = 310,              /* XOR_ASSIGN  */
    LSHIFT_ASSIGN = 311,           /* LSHIFT_ASSIGN  */
    RSHIFT_ASSIGN = 312,           /* RSHIFT_ASSIGN  */
    EQ = 313,                      /* EQ  */
    NE = 314,                      /* NE  */
    LT = 315,                      /* LT  */
    GT = 316,                      /* GT  */
    LE = 317,                      /* LE  */
    GE = 318,                      /* GE  */
    LOGICAL_AND = 319,             /* LOGICAL_AND  */
    LOGICAL_OR = 320,              /* LOGICAL_OR  */
    LSHIFT = 321,                  /* LSHIFT  */
    RSHIFT = 322,                  /* RSHIFT  */
    INCREMENT = 323,               /* INCREMENT  */
    DECREMENT = 324,               /* DECREMENT  */
    ARROW = 325,                   /* ARROW  */
    PLUS = 326,                    /* PLUS  */
    MINUS = 327,                   /* MINUS  */
    MULTIPLY = 328,                /* MULTIPLY  */
    DIVIDE = 329,                  /* DIVIDE  */
    MODULO = 330,                  /* MODULO  */
    BITWISE_AND = 331,             /* BITWISE_AND  */
    BITWISE_OR = 332,              /* BITWISE_OR  */
    BITWISE_XOR = 333,             /* BITWISE_XOR  */
    BITWISE_NOT = 334,             /* BITWISE_NOT  */
    LOGICAL_NOT = 335,             /* LOGICAL_NOT  */
    QUESTION = 336,                /* QUESTION  */
    LPAREN = 337,                  /* LPAREN  */
    RPAREN = 338,                  /* RPAREN  */
    LBRACE = 339,                  /* LBRACE  */
    RBRACE = 340,                  /* RBRACE  */
    LBRACKET = 341,                /* LBRACKET  */
    RBRACKET = 342,                /* RBRACKET  */
    SEMICOLON = 343,               /* SEMICOLON  */
    COMMA = 344,                   /* COMMA  */
    DOT = 345,                     /* DOT  */
    COLON = 346,                   /* COLON  */
    UNARY_MINUS = 347,             /* UNARY_MINUS  */
    UNARY_PLUS = 348,              /* UNARY_PLUS  */
    LOWER_THAN_ELSE = 349          /* LOWER_THAN_ELSE  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 667 "../src/parser.y"

    TreeNode* node;

#line 169 "parser.tab.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;


int yyparse (void);


#endif /* !YY_YY_PARSER_TAB_H_INCLUDED  */
