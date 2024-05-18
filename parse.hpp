#ifndef PARSE_HPP
#define PARSE_HPP
#include <string>
#include <vector>
#include <utility>
#include <iostream>
#include "ast.hpp"
using namespace std;

// can function have multiple return types?

int main(int argc, char* argv[]);

Type* type();

Type* type_ad();

Type* type_op();

Type* type_fp();

Type* type_ar();

Type* funtype();

Type* rettyp();

Program* program();

vector<Decl*> glob();

vector<Decl*> decls();

Decl* decl();

Struct* typdef();

Decl* extrn();

Function* fundef();

vector<pair<Decl*, Exp*>> let();

Stmt* stmt();

Stmt* cond();

Stmt* loop();

vector<Stmt*> block();

Stmt* assign_or_call();

Rhs* rhs();

Lval* lval();

Lval* access(Lval* l_temp);

vector<Exp*> args();

Exp* exp();

Exp* exp_p4();

Exp* exp_p3();

Exp* exp_p2();

Exp* exp_p1();

Exp* exp_ac(Exp* e_temp);

UnaryOp* unop();

BinaryOp* binop_p1 ();

BinaryOp* binop_p2();

BinaryOp* binop_p3();


void consume(string expected);

string consume_id();

int consume_num();

#endif