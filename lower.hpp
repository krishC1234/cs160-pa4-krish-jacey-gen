#ifndef LOWER_HPP
#define LOWER_HPP

#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <map>

#include "ast.hpp"
#include "parse.hpp"

using namespace std;

struct LIR{
	virtual void toString() = 0;
};

typedef struct Label: LIR{
	string name;
	void toString();
	Label(){};
	Label(string name):name(name){};
} Label;

// ArithmeticOp
// | Add
// | Sub
// | Mult
// | Div

typedef struct ArithmeticOp: LIR{
  enum type {Add, Sub, Mul, Div} type;
  void toString();
  ArithmeticOp(){};
  ArithmeticOp(enum type t):type(t){};
} ArithmeticOp;

// ComparisonOp
// | Equal
// | NotEq
// | Lt
// | Lte
// | Gt
// | Gte

typedef struct ComparisonOp: LIR{
    enum type {Equal, NotEq, Lt, Lte, Gt, Gte} type;
    void toString();
	ComparisonOp(){};
	ComparisonOp(enum type t):type(t){};
} ComparisonOp;


// Operand
// | Const { num: int32_t }
// | Var { id: VarId }

typedef struct Operand: LIR{
	enum type{Const, Var} type;

	union Value{

	struct {
		int32_t num;
	} Const;

	struct {
		string id;
	} Var;
		
    Value(){memset(this, 0, sizeof(Value));}
    ~Value(){}
	} value;

	void toString();
	
	Operand(){};
	Operand(enum type t):type(t){};
} Operand;


// LirInst
// | Alloc { lhs: VarId, num: Operand }
// | Arith { lhs: VarId, aop: ArithmeticOp, left: Operand, right: Operand }
// | CallExt { lhs: option<VarId>, callee: FuncId, args: vector<Operand> }
// | Cmp { lhs: VarId, aop: ComparisonOp, left: Operand, right: Operand }
// | Copy { lhs: VarId, op: Operand }
// | Gep { lhs: VarId, src: VarId, idx: Operand }
// | Gfp { lhs: VarId, src: VarId, field: string }
// | Load { lhs: VarId, src: VarId }
// | Store { dst: VarId, op: Operand }

typedef struct LirInst : LIR{
	enum type{Alloc, Arith, CallExt, Cmp, Copy, Gep, Gfp, Load, Store} type;

	union Value{

	struct {
		string lhs;
		Operand* num;
	} Alloc;

	struct {
		string lhs;
		ArithmeticOp* aop;
		Operand* left;
		Operand* right;
	} Arith;

	struct {
		string lhs = "";
		string callee;
		vector<Operand*> args;
	} CallExt;

	struct {
		string lhs;
		ComparisonOp* aop;
		Operand* left;
		Operand* right;
	} Cmp;
		
	struct {
		string lhs;
		Operand* op;
	} Copy;
		
	struct {
		string lhs;
		string src;
		Operand* idx;
	} Gep;
		
	struct {
		string lhs;
		string src;
		string field;
	} Gfp;

	struct {
		string lhs;
		string src; 
	} Load;

	struct {
		string dst; 
		Operand* op;
	} Store;

	Value(){memset(this, 0, sizeof(Value));}
	~Value(){}
	} value;

	void toString();
	LirInst(){};
	LirInst(enum type t):type(t){};
	void codeGenString();
} LirInst;

// Terminal
// | Branch { guard: Operand, tt: BbId, ff: BbId }
// | CallDirect { lhs: option<VarId>, callee: FuncId, args: vector<Operands>, next_bb: BbId }
// | CallIndirect { lhs: option<VarId>, callee: VarId, args: vector<Operands>, next_bb: BbId }
// | Jump { next_bb: BbId }
// | Ret { op: option<Operand> }

typedef struct Terminal: LIR{
	enum type{Branch, CallDirect, CallIndirect, Jump, Ret} type;

	union Value{
	struct {
		Operand* guard;
		string tt; // BbId
		string ff; // BbId
	} Branch;

	struct {
		string lhs = NULL; //optional
		string callee; //FuncId
		vector<Operand*> args;
		string next_bb;
	} CallDirect;
        
	struct {
		string lhs = NULL; //optional
		string callee; //VarId
		vector<Operand*> args;
		string next_bb;
	} CallIndirect;

  struct {
		string next_bb; //Uses the basic block id
	} Jump;
        
  struct {
    Operand* op = NULL; //optional
	} Ret;
        
    Value(){memset(this, 0, sizeof(Value));}
    ~Value(){}
	} value;

	void toString();
	Terminal(){};
	Terminal(enum type t):type(t){};
	void codeGenString();
} Terminal;

// BasicBlock
// - label: BbId
// - insts: vector<LirInst>
// - term: Terminal

typedef struct BasicBlock : LIR{
	string label; //Will store BbId
	vector<LirInst*> insts; //The translational vector
	Terminal* term;
	void toString();
	bool reachable = false;
	void set_reachable(map<string, BasicBlock*>& body);
	void codeGenString();
} BasicBlock;

// Function
// - name: FuncId
// - params: vector<(VarId, Type)> <-- this is a map now
// - rettyp: option<Type>
// - locals: VarId -> Type
// - body: BbId -> BasicBlock

typedef struct LIR_Function : LIR{
	string name; // stores FuncId
	map<string, Type*> params;
	Type* rettyp = NULL; //optional
	map<string, Type*> locals; 
	map<string, BasicBlock*> body;
	void toString();
	void codeGenString();
} LIR_Function;


// Program
// - globals: VarId -> Type
// - structs: StructId -> (FieldId -> Type)
// - externs: FuncId -> Type
// - functions: FuncId -> Function

typedef struct LIR_Program : LIR{
	map<string, Type*> globals;
	map<string, map<string, Type*>> structs;
	map<string, Type*> externs;
	map<string, LIR_Function*> functions;
	void toString();
	void codeGenString();
} LIR_Program;

LIR_Program* lower(Program* prog);

// Overarching lowering methods
void stmt_lower(LIR_Function* lir_func, Stmt* stmt, vector<LIR*>& translation_vector);
Operand* exp_lower(LIR_Function* lir_func, Exp* exp, vector<LIR*>& translation_vector);
Operand* lval_lower(LIR_Function* lir_func, Lval* lval, vector<LIR*>& translation_vector);
Operand* lval_exp_lower(LIR_Function* lir_func, Lval* lval, vector<LIR*>& translation_vector);

// for stmts
void if_lower(LIR_Function* lir_func, Stmt* stmt, vector<LIR*>& translation_vector);
void while_lower(LIR_Function* lir_func, Stmt* stmt, vector<LIR*>& translation_vector);
void assign_exp_lower(LIR_Function* lir_func, Stmt* stmt, vector<LIR*>& translation_vector);
void assign_new_lower(LIR_Function* lir_func, Stmt* stmt, vector<LIR*>& translation_vector);
void call_lower(LIR_Function* lir_func, Stmt* stmt, vector<LIR*>& translation_vector);
void continue_lower(LIR_Function* lir_func, Stmt* stmt, vector<LIR*>& translation_vector);
void break_lower(LIR_Function* lir_func, Stmt* stmt, vector<LIR*>& translation_vector);
void return_none_lower(LIR_Function* lir_func, Stmt* stmt, vector<LIR*>& translation_vector);
void return_one_lower(LIR_Function* lir_func, Stmt* stmt, vector<LIR*>& translation_vector);

// for exps
Operand* num_lower(LIR_Function* lir_func, Exp* exp, vector<LIR*>& translation_vector);
Operand* id_lower(LIR_Function* lir_func, Exp* exp, vector<LIR*>& translation_vector);
Operand* nil_lower(LIR_Function* lir_func, Exp* exp, vector<LIR*>& translation_vector);
Operand* unop_neg_lower(LIR_Function* lir_func, Exp* exp, vector<LIR*>& translation_vector);
Operand* unop_deref_lower(LIR_Function* lir_func, Exp* exp, vector<LIR*>& translation_vector);
Operand* binop_arith_lower(LIR_Function* lir_func, Exp* exp, vector<LIR*>& translation_vector);
Operand* binop_compare_lower(LIR_Function* lir_func, Exp* exp, vector<LIR*>& translation_vector);
Operand* arrayaccess_lower(LIR_Function* lir_func, Exp* exp, vector<LIR*>& translation_vector);
Operand* fieldaccess_lower(LIR_Function* lir_func, Exp* exp, vector<LIR*>& translation_vector);
Operand* call_lower(LIR_Function* lir_func, Exp* exp, vector<LIR*>& translation_vector);

// for lvals
Operand* deref_lower(LIR_Function* lir_func, Lval* lval, vector<LIR*>& translation_vector);
Operand* arrayaccess_lower(LIR_Function* lir_func, Lval* lval, vector<LIR*>& translation_vector);
Operand* fieldaccess_lower(LIR_Function* lir_func, Lval* lval, vector<LIR*>& translation_vector);

// for lval as expressions
Operand* id_lower(LIR_Function* lir_func, Lval* lval, vector<LIR*>& translation_vector);
Operand* not_id_lower(LIR_Function* lir_func, Lval* lval, vector<LIR*>& translation_vector);

string create_fresh_var(LIR_Function* lir_func, Type* type);

string create_fresh_label(LIR_Function* lir_func);

#endif