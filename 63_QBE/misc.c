#include "defs.h"
#include "data.h"
#include "decl.h"
#include <stdio.h>
#include <unistd.h>

// Miscellaneous functions
// Copyright (c) 2019 Warren Toomey, GPL3

// Ensure that the current token is t,
// and fetch the next token. Otherwise
// throw an error 
/*其作用是确保当前的标记（Token.token）与给定的标记 t 匹配。
如果匹配，就继续扫描获取下一个标记；如果不匹配，就调用 fatals 函数，抛出一个错误，*/
void match(int t, char *what) {
  if (Token.token == t) {
    scan(&Token);
  } else {
    fatals("Expected", what);
  }
}

// Match a semicolon and fetch the next token
void semi(void) {
  match(T_SEMI, ";");
}

// Match a left brace and fetch the next token
void lbrace(void) {
  match(T_LBRACE, "{");
}

// Match a right brace and fetch the next token
void rbrace(void) {
  match(T_RBRACE, "}");
}

// Match a left parenthesis and fetch the next token
void lparen(void) {
  match(T_LPAREN, "(");
}

// Match a right parenthesis and fetch the next token
void rparen(void) {
  match(T_RPAREN, ")");
}

// Match an identifer and fetch the next token
void ident(void) {
  match(T_IDENT, "identifier");
}

// Match a comma and fetch the next token
//看是否是逗号
void comma(void) {
  match(T_COMMA, "comma");
}

// Print out fatal messages
void fatal(char *s) {
  fprintf(stderr, "%s on line %d of %s\n", s, Line, Infilename);
  fclose(Outfile);
  unlink(Outfilename);
  exit(1);
}

void fatals(char *s1, char *s2) {
  fprintf(stderr, "%s:%s on line %d of %s\n", s1, s2, Line, Infilename);
  fclose(Outfile);
  unlink(Outfilename);
  exit(1);
}

void fatald(char *s, int d) {
  fprintf(stderr, "%s:%d on line %d of %s\n", s, d, Line, Infilename);
  fclose(Outfile);
  unlink(Outfilename);
  exit(1);
}

void fatalc(char *s, int c) {
  fprintf(stderr, "%s:%c on line %d of %s\n", s, c, Line, Infilename);
  fclose(Outfile);
  unlink(Outfilename);
  exit(1);
}
