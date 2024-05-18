#ifndef PRINT_AST_HPP
#define PRINT_AST_HPP
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <unordered_map>
using namespace std;

struct AST{
  virtual void toString() = 0;
};

void indent_print(string s, bool newline);

void single_print(string name, vector<AST*> ast, bool short_print);

/*
Type
| Int
| Struct { name: StructId }
| Fn { prms: vector<Type>, ret: option<Type> }
| Ptr { ref: Type } 
*/

typedef struct Type:AST {
  enum type{Int, Struct, Fn, Ptr, Any} type;

  union Value{
    ///For Struct type
    struct {
      string name;
    } Struct;

    // For the Fn type
    struct {
      vector<Type*> prms;
      Type* ret = NULL; //optional
    } Fn;

    //For Ptr type
    struct {
      Type* ref;
    } Ptr;

    Value(){memset(this, 0, sizeof(Value));}
    ~Value(){}
  } value;

  Type(){};
  Type(enum type t):type(t){};
  void toString();
  string type_string();
} Type;

/*
Decl
- name: string
- type: Type
*/

typedef struct Decl: AST{
  string name;
  Type* type;
  void toString();
  Type* to_type();
} Decl;

/*
Struct
- name: StructId
- fields: vector<Decl>
*/

typedef struct Struct: AST{
  string name;
  vector<Decl*> fields;
  void toString();
  Type* to_type(string fields);
} Struct;

/*
UnaryOp
| Neg
| Deref
*/

typedef struct UnaryOp : AST{
  enum type {Neg, Deref} type;
  void toString();
} UnaryOp;



/*
BinaryOp
| Add
| Sub
| Mul
| Div
| Equal
| NotEq
| Lt
| Lte
| Gt
| Gte
*/

typedef struct BinaryOp : AST{
  enum type {Add, Sub, Mul, Div, Equal, NotEq, Lt, Lte, Gt, Gte} type;
  void toString();
} BinaryOp;


/*
Exp
| Num { n: int32_t }
| Id { name: string }
| Nil
| UnOp { op: UnaryOp, operand:Exp }
| BinOp { op: BinaryOp, left: Exp, right: Exp }
| ArrayAccess { ptr: Exp, index: Exp }
| FieldAccess { ptr: Exp, field: string }
| Call { callee: Exp, args: vector<Exp> }
*/

typedef struct Exp : AST{
  enum {Num, Id, Nil, UnOp, BinOp, ArrayAccess, FieldAccess, Call} type;

  union Value{
    //For the Num type
    struct {
      int32_t n;
    } Num;

    //For the Id type
    struct {
      string name;
    } Id;

    //For the UnOp type
    struct {
      UnaryOp* op;
      Exp* operand;
    } UnOp;

    //For the BinOp type
    struct {
      BinaryOp* op; 
      Exp* left;
      Exp* right;
    } BinOp;

    //For the ArrayAccess type
    struct {
      Exp* ptr;
      Exp* index;
    } ArrayAccess;

    //For the FieldAccess type
    struct {
      Exp* ptr;
      string field;
    } FieldAccess;

    //For the Call type
    struct {
      Exp* callee;
      vector<Exp*> args;
    } Call;

    Value(){memset(this, 0, sizeof(Value));}
    ~Value(){}

  } value;
  void toString();
  Type* to_type(string func_name, unordered_map<string, Type*> gamma);
} Exp;


/*
Rhs
| RhsExp { exp: Exp }
| New { type: Type, amount: Exp }
*/

typedef struct Rhs : AST{
  enum {RhsExp, New} type;
  union{
    //For the RhsExp type
    struct {
      Exp* exp;
    } RhsExp;

    struct {
      Type* type;
      Exp* amount;
    } New;

  } value;
  void toString();
  Type* to_type(string func_name, unordered_map<string, Type*> gamma);
} Rhs;

/*
Lval
| Id { name: string }
| Deref { lval: Lval }
| ArrayAccess { ptr: Lval, index: Exp }
| FieldAccess { ptr: Lval, field: string }
*/

typedef struct Lval : AST{
  enum {Id, Deref, ArrayAccess, FieldAccess} type;
  
  union Value{
    // For the Id
    struct {
      string name;
    } Id;
    
    // For the Deref
    struct {
      Lval* lval;
    } Deref;

    // ArrayAccess
    struct{
      Lval* ptr;
      Exp* index;
    } ArrayAccess;

    // FieldAccess
    struct {
      Lval* ptr;
      string field;
    } FieldAccess;

    Value(){memset(this, 0, sizeof(Value));}
    ~Value(){}

  } value;
  void toString();
  Type* to_type(string func_name, unordered_map<string, Type*> gamma);
} Lval;

/*
Stmt
| Break
| Continue
| Return { exp: option<Exp> }
| Assign { lhs: Lval, rhs: Rhs }
| Call { callee: Lval, args: vector<Exp> }
| If { guard: Exp, tt: vector<Stmt>, ff: vector<Stmt> }
| While { guard: Exp, body: vector<Stmt> }
*/

typedef struct Stmt: AST{
  enum {Break, Continue, Return, Assign, Call, If, While} type;

  union Value{
    //break, continue statements have no val
    
    //For return statement
    struct {
      Exp* exp = NULL; //optional
    } Return;
    
    //For assignment statement
    struct {
      Lval* lhs;
      Rhs* rhs;
    } Assign;
    
    //For call statement
    struct {
      Lval* callee;
      vector<Exp*> args;
    } Call;

    //For if statement
    struct {
      Exp* guard;
      vector<Stmt*> tt;
      vector<Stmt*> ff;
    } If;
    
    struct {
      Exp* guard;
      vector<Stmt*> body;
    } While;

    Value(){memset(this, 0, sizeof(Value));}
    ~Value(){}

  } value;
  void toString();
  void to_verify(string func_name, unordered_map<string, Type*> gamma, Type* ret_type);
} Stmt;

/*
Function
- name: FuncId
- params: vector<Decl>
- rettyp: option<Type>
- locals: vector<(Decl, option<Exp>)>
- stmts: vector<Stmt>  
*/

typedef struct Function : AST{
  string name;
  vector<Decl*> params;
  Type* rettyp = NULL; //optional
  vector<pair<Decl*, Exp*>> locals; //Exp is optional
  vector<Stmt*> stmts;
  void toString();
  Type* to_type();
} Function;


/*
Program
- globals: vector<Decl>
- structs: vector<Struct>
- externs: vector<Decl>
- functions: vector<Function>
*/


typedef struct Program : AST{
  vector<Decl*> globals;
  vector<Struct*> structs;
  vector<Decl*> externs;
  vector<Function*> functions;
  void toString();
} Program;

void validate(Program* prog);

bool compare_recurse(Type* a, Type* b);
bool compare_recurse(Exp* a, Exp* b, unordered_map<string, Type*> gamma, string func_name);
string get_struct_name(Exp* a);
string get_struct_name(Lval* a);

#endif