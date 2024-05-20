#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <stack>
#include <string>

#include "lower.hpp"
#include "ast.hpp"
#include "parse.hpp"

using namespace std;

//Global hashmaps that map function names to counters for their variables or labels
unordered_map<string, int> fresh_vars;
unordered_map<string, int> fresh_labels;

// create empty LIR program
LIR_Program* lir;

stack<string> while_hdr_labels;
stack<string> while_end_labels;

int num_ret = 0;

bool DEBUG_LOWER = true;

/* 
 * 2.3 Lowering Expressions
 */
LIR_Program* lower(Program* prog){
	lir = new LIR_Program;
	// Copy prog.{globals, externs, structs, functions} into lir

	// Copy globals from AST --> LIR
	for(Decl* decl: prog->globals){
		lir->globals[decl->name] = decl->type;
	}

	// For all func in prog.functions add the following
	// Add func_name -> ptr_to_func to globals as well 
	for(Function* func: prog->functions){

		if(func->name == "main"){
			continue;
		}

		vector<Type*> temp;
		for(Decl* decl: func->params){
			temp.push_back(decl->type);
		}

		Type* fn = new Type;
		fn->type = Type::Fn;
		fn->value.Fn.prms = temp;
		fn->value.Fn.ret = func->rettyp;

		Type* ptr = new Type;
		ptr->type = Type::Ptr;
		ptr->value.Ptr.ref = fn;

		lir->globals[func->name] = ptr;
	}


	// Copy externs from AST --> LIR
	for(Decl* decl: prog->externs){
		lir->externs[decl->name] = decl->type;
	}

	// Copy structs from AST  --> LIR 
	for (Struct* s : prog->structs){
		for(Decl* decl: s->fields){
			lir->structs[s->name][decl->name] = decl->type;
		}
	}

	//For loop to insert func_names, func return types, etc
	for(Function* func : prog->functions){
		LIR_Function* lir_func = new LIR_Function;
		// populate the params
		for(Decl* decl: func->params){
			lir_func->params[decl->name] = decl->type;
		}
		// populate rettyp
		lir_func->rettyp = func->rettyp;
		lir_func->name = func->name;

		lir->functions[func->name] = lir_func;

	
	}

	//For loop just to run the statements in each of the function bodies
	for(Function* func : prog->functions){
		LIR_Function* lir_func = lir->functions[func->name];
		//Declare a function type that belongs to the LIR data structure
		
		//populate the locals + eliminate locals by turning them into assignments --> add them to func.stmts
		vector<Stmt*> local_assignment;
		for(pair<Decl*,Exp*> p: func->locals){
			Decl* decl = p.first;
			Exp* e = p.second;

			lir_func->locals[decl->name] = decl->type;

			if(e != NULL){
				Stmt* stmt = new Stmt; 
				stmt->type = Stmt::Assign;

				Lval* l = new Lval;
				l->type = Lval::Id;
				l->value.Id.name = decl->name;
				stmt->value.Assign.lhs = l; // string

				Rhs* r = new Rhs;
				r->type = Rhs::RhsExp;
				r->value.RhsExp.exp = e; // Exp*
				stmt->value.Assign.rhs = r;

				// func->stmts.insert(func->stmts.begin(), stmt);
				local_assignment.push_back(stmt);
			}
		}
		//vec1.insert(vec1.end(), vec2.begin(), vec2.end());
		func->stmts.insert(func->stmts.begin(), local_assignment.begin(), local_assignment.end());
		// TODO(): Compute func.stmts which will fill in the translation vector.

		// create vector that will hold the emitted instructions/labels
		// a string vector so we must to_string() the emitted_inst before we ever push onto translation vector
		vector<LIR*> translation_vector;

		// Its first and only element should be Label("entry").
		translation_vector.push_back(new Label("entry"));

		// stmt_lower(lir_func, func, translation_vector); previous version with a function as the parameter
		for (Stmt* stmt : func->stmts){
			stmt_lower(lir_func, stmt, translation_vector);
		}
		// TODO(): Take the final translation vector and construct the CFG for lir.functions[func].body.
		
		string cur_bb = "";
		// create a new BasicBlock
		for(LIR* lir : translation_vector){
			if(dynamic_cast<Label*>(lir) != NULL){
				Label* temp = dynamic_cast<Label*>(lir);
				BasicBlock* bb = new BasicBlock;
				bb->label = temp->name;
				bb->term = NULL;
				bb->insts = vector<LirInst*>();
				lir_func->body[temp->name] = bb;
				cur_bb = temp->name;
			}
			else if(dynamic_cast<Terminal*>(lir) != NULL){
				if(cur_bb == "") continue;
				Terminal* temp = dynamic_cast<Terminal*>(lir);
				lir_func->body[cur_bb]->term = temp;
				cur_bb = "";
			}
			else{ // if its not a Terminal or Label, it is a LirInst
				if(cur_bb == "") continue;
				LirInst* temp = dynamic_cast<LirInst*>(lir);
				lir_func->body[cur_bb]->insts.push_back(temp);
			}
		}

		num_ret = 0;
		
		if(DEBUG_LOWER){cout << "Before reachable" << endl;}
		// set reachable
		lir_func->body["entry"]->set_reachable(lir_func->body);
		
		// check return stuff
		if(num_ret > 1){
			string exit_label = create_fresh_label(lir_func);
			// emit Label(EXIT)
			BasicBlock* exit_bb = new BasicBlock;
			
			exit_bb->label = exit_label;
			exit_bb->term = new Terminal(Terminal::Ret);
			exit_bb->insts = vector<LirInst*>();
			exit_bb->reachable = true;
			

			if(lir_func->rettyp == NULL){
				// emit Return(None)
				exit_bb->term->value.Ret.op = NULL;
				// replace all previous Return(None) instructions with Jump(EXIT)
				for(auto bb : lir_func->body){
					if(bb.second->term->type == Terminal::Ret){
						bb.second->term = new Terminal(Terminal::Jump);
						bb.second->term->value.Jump.next_bb = exit_label;
					}
				}
			}
			else{
				string exit_var = create_fresh_var(lir_func, lir_func->rettyp);
				Operand* op = new Operand(Operand::Var);
				op->value.Var.id = exit_var;
				//emit Return(x)
				exit_bb->term->value.Ret.op = op;
				// replace all other Return(op) instructions with Copy(exit_var, op); Jump(EXIT)
				for(auto bb : lir_func->body){
					if(bb.second->term->type == Terminal::Ret){
						LirInst* copy = new LirInst(LirInst::Copy);
						copy->value.Copy.lhs = exit_var;
						copy->value.Copy.op = bb.second->term->value.Ret.op;
						bb.second->insts.push_back(copy);
						bb.second->term = new Terminal(Terminal::Jump);
						bb.second->term->value.Jump.next_bb = exit_label;
					}
				}
			}
			lir_func->body[exit_label] = exit_bb;
		}
	}
	
	return lir;
};

/*
 * 2.2 Lowering Statements
 * Summary: Takes a stmt and emits LIR instructions into the translation vector w/o returning anything
 * Parameters: LIR_Function* lir_func, Stmt* stmt, vector<string>& translation_vector
 * 
*/
void stmt_lower(LIR_Function* lir_func, Stmt* stmt, vector<LIR*>& translation_vector){
	if (DEBUG_LOWER) cout << "entered stmt_lower" << endl;
	if(stmt->type == Stmt::If){
		if (DEBUG_LOWER) cout << "  stmt if_lower" << endl;
		if_lower(lir_func, stmt, translation_vector);
	}
	else if(stmt->type == Stmt::While){
		if (DEBUG_LOWER) cout << "  stmt while_lower" << endl;
		while_lower(lir_func, stmt, translation_vector);
	}
	else if(stmt->type == Stmt::Assign){
		if (stmt->value.Assign.rhs->type == Rhs::RhsExp){
			if (DEBUG_LOWER) cout << "  stmt assign_exp_lower" << endl;
			assign_exp_lower(lir_func, stmt, translation_vector);
		} else {
			if (DEBUG_LOWER) cout << "  stmt assign_new_lower" << endl;
			assign_new_lower(lir_func, stmt, translation_vector);
		}
	}
	else if(stmt->type == Stmt::Call){
		if (DEBUG_LOWER) cout << "  stmt call_lower" << endl;
		call_lower(lir_func, stmt, translation_vector);
	}
	else if(stmt->type == Stmt::Continue){
		if (DEBUG_LOWER) cout << "  stmt continue_lower" << endl;
		continue_lower(lir_func, stmt, translation_vector);
	}
	else if(stmt->type == Stmt::Break){
		if (DEBUG_LOWER) cout << "  stmt break_lower" << endl;
		break_lower(lir_func, stmt, translation_vector);
	}
	else if(stmt->type == Stmt::Return){
		if (DEBUG_LOWER) cout << "  stmt return_lower" << endl;
		if (stmt->value.Return.exp == NULL){
			return_none_lower(lir_func, stmt, translation_vector);
		} else {
			return_one_lower(lir_func, stmt, translation_vector);
		}
		// Two case when it returns nothing and when it returns an exp
		// Return(None)^s: emit Return(None)
		// Return(e)^s: emit Return([e]^e)
	}
	else{
		cout << "Bad Bad" << endl;
	}
	if (DEBUG_LOWER) cout << "exited stmt_lower" << endl;
}

void if_lower(LIR_Function* lir_func, Stmt* stmt, vector<LIR*>& translation_vector){
	if (DEBUG_LOWER) cout << "entered if_lower" << endl;
	
	// let TT , FF , IF_END be fresh labels
	string TT = create_fresh_label(lir_func);
	string FF = create_fresh_label(lir_func);
	string IF_END = create_fresh_label(lir_func);
	
	// emit Branch([[Guard]]^e, TT, FF) 
	Terminal* emit_branch = new Terminal(Terminal::Branch);
	emit_branch->value.Branch.guard = exp_lower(lir_func, stmt->value.If.guard, translation_vector);
	emit_branch->value.Branch.tt = TT;
	emit_branch->value.Branch.ff = FF;
	translation_vector.push_back(emit_branch);

	// emit Label(TT)
	translation_vector.push_back(new Label(TT));

	// go through all TT statements: [TT]^s
	for(Stmt* stmt: stmt->value.If.tt){
		stmt_lower(lir_func, stmt, translation_vector);
	}

	// emit Jump(IF END)
	Terminal* Jump1 = new Terminal(Terminal::Jump);
	Jump1->value.Jump.next_bb = IF_END;
	translation_vector.push_back(Jump1);

	// emit Label(FF)
	translation_vector.push_back(new Label(FF));

	// go through all FF statements: [FF]^s
	for(Stmt* stmt: stmt->value.If.ff){
		stmt_lower(lir_func, stmt, translation_vector);
	}

	// emit Jump(IF END)
	Terminal* Jump2 = new Terminal(Terminal::Jump);
	Jump2->value.Jump.next_bb = IF_END;
	translation_vector.push_back(Jump2);

	// emit Label(IF END)
	translation_vector.push_back(new Label(IF_END));
	if (DEBUG_LOWER) cout << "exited if_lower" << endl;
}

void while_lower(LIR_Function* lir_func, Stmt* stmt, vector<LIR*>& translation_vector){

	// let WHILE_HDR , WHILE_BODY , WHILE_END be fresh labels
	string WHILE_HDR = create_fresh_label(lir_func);
	string WHILE_BODY = create_fresh_label(lir_func);
	string WHILE_END = create_fresh_label(lir_func);
	
	while_hdr_labels.push(WHILE_HDR);
	while_end_labels.push(WHILE_END);

	// emit Jump(WHILE HDR)
	Terminal* Jump1 = new Terminal(Terminal::Jump);
	Jump1->value.Jump.next_bb = WHILE_HDR;
	translation_vector.push_back(Jump1);

	// emit Label(WHILE HDR)
	translation_vector.push_back(new Label(WHILE_HDR));

	// emit Branch(guard, WHILE BODY, WHILE END)
	Terminal* emit_branch = new Terminal(Terminal::Branch);	
	emit_branch->value.Branch.guard = exp_lower(lir_func, stmt->value.While.guard, translation_vector);
	emit_branch->value.Branch.tt = WHILE_BODY;
	emit_branch->value.Branch.ff = WHILE_END;
	translation_vector.push_back(emit_branch);

	// emit Label(WHILE HDR)
	translation_vector.push_back(new Label(WHILE_BODY));
	
	// go through all body statements: [[body]]^s
	for(Stmt* stmt: stmt->value.While.body){
		stmt_lower(lir_func, stmt, translation_vector);
	}
	
	// emit Jump(WHILE HDR)
	Terminal* Jump2 = new Terminal(Terminal::Jump);
	Jump2->value.Jump.next_bb = WHILE_HDR;
	translation_vector.push_back(Jump2);
	
	// emit Label(WHILE END
	translation_vector.push_back(new Label(WHILE_END));

	while_hdr_labels.pop();
	while_end_labels.pop();
}


void assign_exp_lower(LIR_Function* lir_func, Stmt* stmt, vector<LIR*>& translation_vector){
	// if lhs is Id(name) then emit Copy(Var(name), [e]^e)
	if(stmt->value.Assign.lhs->type == Lval::Id){
		LirInst* copy = new LirInst(LirInst::Copy);
		copy->value.Copy.lhs = stmt->value.Assign.lhs->value.Id.name; // string
		copy->value.Copy.op = exp_lower(lir_func, stmt->value.Assign.rhs->value.RhsExp.exp, translation_vector); // Operand*
		translation_vector.push_back(copy);
	}
	else{
		// let x = [lhs]^l
		//TODO; Is type* x what it really returns?
		Operand* x = lval_lower(lir_func, stmt->value.Assign.lhs, translation_vector);
		// let y = [e]^e
		Operand* y = exp_lower(lir_func, stmt->value.Assign.rhs->value.RhsExp.exp, translation_vector);
		// emit Store(x, y)
		LirInst* store = new LirInst(LirInst::Store);
		store->value.Store.dst = x->value.Var.id;
		store->value.Store.op = y;
		translation_vector.push_back(store);
	}
}

void assign_new_lower(LIR_Function* lir_func, Stmt* stmt, vector<LIR*>& translation_vector){
	// 	if lhs is Id(name) then emit Alloc(Var(name), [e]^e)
	LirInst* alloc = new LirInst(LirInst::Alloc);
	if(stmt->value.Assign.lhs->type == Lval::Id){
		alloc->value.Alloc.lhs = stmt->value.Assign.lhs->value.Id.name;
		//TODO: Check this later below
		alloc->value.Alloc.num = exp_lower(lir_func, stmt->value.Assign.rhs->value.New.amount, translation_vector);

		translation_vector.push_back(alloc);
	}
	else{
		// let w be a fresh var with type &typ
		Type* typ = new Type(Type::Ptr);
		typ->value.Ptr.ref = stmt->value.Assign.rhs->value.New.type;
		string w = create_fresh_var(lir_func, typ);
		cout << "x: " << endl;
		// let x = [lhs]^l
		Operand* x = lval_lower(lir_func, stmt->value.Assign.lhs, translation_vector);
		// emit Alloc(w, [e]^e)
		alloc->value.Alloc.lhs = w;
		alloc->value.Alloc.num = exp_lower(lir_func, stmt->value.Assign.rhs->value.New.amount, translation_vector);
		translation_vector.push_back(alloc);
		// emit Store(x, w)
		LirInst* store = new LirInst(LirInst::Store);
		Operand* new_w = new Operand(Operand::Var);
		new_w->value.Var.id = w;
		store->value.Store.dst = x->value.Var.id;
		store->value.Store.op = new_w;
		translation_vector.push_back(store);
	}
}

void call_lower(LIR_Function* lir_func, Stmt* stmt, vector<LIR*>& translation_vector){
	// let aops = ∀a ∈ args . [a]e
	vector<Operand*> aops;
	for(Exp* arg: stmt->value.Call.args){
		aops.push_back(exp_lower(lir_func, arg, translation_vector));
	}
	// let direct = callee is Id(name) and name is not shadowed by a local / parameter
	bool direct = stmt->value.Call.callee->type == Lval::Id;
	direct = direct && lir_func->locals.find(stmt->value.Call.callee->value.Id.name) == lir_func->locals.end();
	// if direct and name is an extern then emit CallExt(None, name, aops)
	if(direct && lir->externs.find(stmt->value.Call.callee->value.Id.name) != lir->externs.end()){
		if(DEBUG_LOWER) cout << "Call to extern" << endl;
		LirInst* call_ext = new LirInst(LirInst::CallExt);
		call_ext->value.CallExt.callee = stmt->value.Call.callee->value.Id.name;
		call_ext->value.CallExt.args = aops;
		translation_vector.push_back(call_ext);
	}
	// else
	else{
		// let NEXT be a fresh label
		string NEXT = create_fresh_label(lir_func);
		// if direct and name is a function then emit CallDirect(None, name, aops, NEXT)
		if(direct && lir->functions.find(stmt->value.Call.callee->value.Id.name) != lir->functions.end()){
			if (DEBUG_LOWER) cout << "Call to function" << endl;
			Terminal* call_direct = new Terminal(Terminal::CallDirect);
			call_direct->value.CallDirect.callee = stmt->value.Call.callee->value.Id.name;
			call_direct->value.CallDirect.args = aops;
			call_direct->value.CallDirect.next_bb = NEXT;
			translation_vector.push_back(call_direct);
		}
		// else emit CallIndirect(None, [callee]ℓe, aops, NEXT)
		else{
			if (DEBUG_LOWER) cout << "Call to indirect" << endl;
			Terminal* call_indirect = new Terminal(Terminal::CallIndirect);
			call_indirect->value.CallIndirect.callee = lval_exp_lower(lir_func, stmt->value.Call.callee, translation_vector)->value.Var.id;
			call_indirect->value.CallIndirect.args = aops;
			call_indirect->value.CallIndirect.next_bb = NEXT;
			translation_vector.push_back(call_indirect);
		}
		// emit Label(NEXT)
		translation_vector.push_back(new Label(NEXT));
	}
}

void continue_lower(LIR_Function* lir_func, Stmt* stmt, vector<LIR*>& translation_vector){
	// find the nearest previous Label(WHILE HDR)
	string nearest_while_hdr = while_hdr_labels.top(); 
	// emit Jump(WHILE HDR)
	Terminal* jump = new Terminal(Terminal::Jump);
	jump->value.Jump.next_bb = nearest_while_hdr;
	translation_vector.push_back(jump);
}

void break_lower(LIR_Function* lir_func, Stmt* stmt, vector<LIR*>& translation_vector){
	// find the nearest previous Branch( , , WHILE END)
	string nearest_while_end = while_end_labels.top();
	// emit Jump(WHILE END)
	Terminal* jump = new Terminal(Terminal::Jump);
	jump->value.Jump.next_bb = nearest_while_end;
	translation_vector.push_back(jump);
}

void return_none_lower(LIR_Function* lir_func, Stmt* stmt, vector<LIR*>& translation_vector){
	translation_vector.push_back(new Terminal(Terminal::Ret));
}

void return_one_lower(LIR_Function* lir_func, Stmt* stmt, vector<LIR*>& translation_vector){
	Operand* op = exp_lower(lir_func, stmt->value.Return.exp, translation_vector);
	Terminal* ret = new Terminal(Terminal::Ret);
	ret->value.Ret.op = op;
	translation_vector.push_back(ret);
}

/*
 * 2.3 Lowering Expressions
 *  
 * Summary: Takes a exp and emits LIR instructions into the translation vector,
 * returning a LIR Operand (a variable or constant) containing the final value of the expression
 * Parameters: LIR_Function* lir_func, Exp* exp, vector<string>& translation_vector
*/
Operand* exp_lower(LIR_Function* lir_func, Exp* exp, vector<LIR*>& translation_vector){
	Operand* operand = NULL;
	if(exp->type == Exp::Num){
		if(DEBUG_LOWER) cout << "    Num<-" << endl;
		operand = num_lower(lir_func, exp, translation_vector);
		if(DEBUG_LOWER) cout << "    Num->" << endl;
	}
	else if(exp->type == Exp::Id){
		if(DEBUG_LOWER) cout << "    Id" << endl;
		operand = id_lower(lir_func, exp, translation_vector);
	}
	else if(exp->type == Exp::Nil){
		if(DEBUG_LOWER) cout << "    Nil" << endl;
		operand = nil_lower(lir_func, exp, translation_vector);
	}
	else if(exp->type == Exp::UnOp){
		if (exp->value.UnOp.op->type == UnaryOp::Neg){
			if(DEBUG_LOWER) cout << "    Neg" << endl;
			operand = unop_neg_lower(lir_func, exp, translation_vector);
		} else {
			if(DEBUG_LOWER) cout << "    Deref" << endl;
			operand = unop_deref_lower(lir_func, exp, translation_vector);

		}
	}
	else if(exp->type == Exp::BinOp){
		if (exp->value.BinOp.op->type == BinaryOp::Add ||
			exp->value.BinOp.op->type == BinaryOp::Sub ||
			exp->value.BinOp.op->type == BinaryOp::Mul ||
			exp->value.BinOp.op->type == BinaryOp::Div){
			if (DEBUG_LOWER) cout << "    Arith" << endl;
			operand = binop_arith_lower(lir_func, exp, translation_vector);	
		} else {
			if (DEBUG_LOWER) cout << "    Compare" << endl;
			operand = binop_compare_lower(lir_func, exp, translation_vector);	
		}
	}
	else if(exp->type == Exp::ArrayAccess){
		if(DEBUG_LOWER) cout << "    ArrayAccess" << endl;
		operand = arrayaccess_lower(lir_func, exp, translation_vector);
	}
	else if(exp->type == Exp::FieldAccess){
		if(DEBUG_LOWER) cout << "    FieldAccess" << endl;
		operand = fieldaccess_lower(lir_func, exp, translation_vector);
	}
	else if(exp->type == Exp::Call){
		if(DEBUG_LOWER) cout << "    Call" << endl;
		operand = call_lower(lir_func, exp, translation_vector);
	}	
	else{
		cout << "Bad Bad" << endl;
	}
	return operand;
}

Operand* num_lower(LIR_Function* lir_func, Exp* exp, vector<LIR*>& translation_vector){
	Operand* operand = new Operand(Operand::Const);
	operand->value.Const.num = exp->value.Num.n;
	return operand;
}

Operand* id_lower(LIR_Function* lir_func, Exp* exp, vector<LIR*>& translation_vector){
	Operand* operand = new Operand(Operand::Var);
	operand->value.Var.id = exp->value.Id.name;
	return operand;
}

Operand* nil_lower(LIR_Function* lir_func, Exp* exp, vector<LIR*>& translation_vector){
	Operand* operand = new Operand(Operand::Const);
	operand->value.Const.num = 0;
	return operand;
}

Operand* unop_neg_lower(LIR_Function* lir_func, Exp* exp, vector<LIR*>& translation_vector){
	//let lhs be a fresh var of type Int
	string lhs = create_fresh_var(lir_func, new Type(Type::Int));

	// emit Arith(lhs, Sub, Const(0), [e]^e)

	// create the LirInst of type Arith
	LirInst* arith_lir = new LirInst(LirInst::Arith);
	arith_lir->value.Arith.lhs = lhs;
	arith_lir->value.Arith.aop = new ArithmeticOp(ArithmeticOp::Sub);

	// create const(0)
	Operand* op = new Operand(Operand::Const);
	op->value.Const.num = 0;
	arith_lir->value.Arith.left = op;

	// create right [e]^e
	arith_lir->value.Arith.right = exp_lower(lir_func, exp->value.UnOp.operand, translation_vector); 

	// push the LirInst into the translation vector
	translation_vector.push_back(arith_lir);
	
	// return lhs
	Operand* to_be_returned_op = new Operand(Operand::Var);
	to_be_returned_op->value.Var.id = lhs;
	return to_be_returned_op;
}

Operand* unop_deref_lower(LIR_Function* lir_func, Exp* exp, vector<LIR*>& translation_vector){
	// let src = [e]e
	Operand* src = exp_lower(lir_func, exp->value.UnOp.operand, translation_vector);

	// let lhs be a fresh var of type τ s . t . src :&τ
	string lhs;
	if(lir_func->locals.find(src->value.Var.id) != lir_func->locals.end()){
		lhs = create_fresh_var(lir_func, lir_func->locals[src->value.Var.id]->value.Ptr.ref);
	}
	else if(lir->globals.find(src->value.Var.id) != lir->globals.end()){
		lhs = create_fresh_var(lir_func, lir->globals[src->value.Var.id]->value.Ptr.ref);
	}
	else{
		cout << "Bad Bad" << endl;
	}
	
	// emit Load(lhs, src)
	LirInst* load = new LirInst(LirInst::Load);
	load->value.Load.lhs = lhs;
	load->value.Load.src = src->value.Var.id;
	translation_vector.push_back(load);

	// Return lhs
	Operand* to_be_returned_op = new Operand(Operand::Var);
	to_be_returned_op->value.Var.id = lhs;
	return to_be_returned_op;
}

Operand* binop_arith_lower(LIR_Function* lir_func, Exp* exp, vector<LIR*>& translation_vector){
	// let op1 = left
	Operand* op1 = exp_lower(lir_func, exp->value.BinOp.left, translation_vector);
	// let op2 = right
	Operand* op2 = exp_lower(lir_func, exp->value.BinOp.right, translation_vector);
	// let lhs be a fresh var of type Int
	string lhs = create_fresh_var(lir_func, new Type(Type::Int));
	// emit Arith(lhs, op, op1, op2)
	LirInst* arith = new LirInst(LirInst::Arith);
	arith->value.Arith.lhs = lhs;
	arith->value.Arith.aop = new ArithmeticOp;
	arith->value.Arith.left = op1;
	arith->value.Arith.right = op2;
	BinaryOp* binop = exp->value.BinOp.op;
	if(binop->type == BinaryOp::Add){
		arith->value.Arith.aop->type = ArithmeticOp::Add;
	}
	else if(binop->type == BinaryOp::Sub){
		arith->value.Arith.aop->type = ArithmeticOp::Sub;
	}
	else if(binop->type == BinaryOp::Mul){
		arith->value.Arith.aop->type = ArithmeticOp::Mul;
	}
	else if(binop->type == BinaryOp::Div){
		arith->value.Arith.aop->type = ArithmeticOp::Div;
	}
	translation_vector.push_back(arith);
	// lhs
	Operand* to_be_returned_op = new Operand(Operand::Var);
	to_be_returned_op->value.Var.id = lhs;
	return to_be_returned_op;
}

Operand* binop_compare_lower(LIR_Function* lir_func, Exp* exp, vector<LIR*>& translation_vector){
	// let op1 = left
	Operand* op1 = exp_lower(lir_func, exp->value.BinOp.left, translation_vector);
	// let op2 = right
	Operand* op2 = exp_lower(lir_func, exp->value.BinOp.right, translation_vector);
	// let lhs be a fresh var of type Int
	string lhs = create_fresh_var(lir_func, new Type(Type::Int));
	// emit Cmp(lhs, op, op1, op2)
	LirInst* cmp = new LirInst(LirInst::Cmp);
	cmp->value.Cmp.lhs = lhs;
	cmp->value.Cmp.aop = new ComparisonOp;
	cmp->value.Cmp.left = op1;
	cmp->value.Cmp.right = op2;
	BinaryOp* binop = exp->value.BinOp.op;
	if(binop->type == BinaryOp::Equal){
		cmp->value.Cmp.aop->type = ComparisonOp::Equal;
	}
	else if(binop->type == BinaryOp::NotEq){
		cmp->value.Cmp.aop->type = ComparisonOp::NotEq;
	}
	else if(binop->type == BinaryOp::Lt){
		cmp->value.Cmp.aop->type = ComparisonOp::Lt;
	}
	else if(binop->type == BinaryOp::Lte){
		cmp->value.Cmp.aop->type = ComparisonOp::Lte;
	}
	else if(binop->type == BinaryOp::Gt){
		cmp->value.Cmp.aop->type = ComparisonOp::Gt;
	}
	else if(binop->type == BinaryOp::Gte){
		cmp->value.Cmp.aop->type = ComparisonOp::Gte;
	}
	translation_vector.push_back(cmp);
	// lhs
	Operand* to_be_returned_op = new Operand(Operand::Var);
	to_be_returned_op->value.Var.id = lhs;
	return to_be_returned_op;
}

Operand* arrayaccess_lower(LIR_Function* lir_func, Exp* exp, vector<LIR*>& translation_vector){
	// let src = ptr
	Operand* src = exp_lower(lir_func, exp->value.ArrayAccess.ptr, translation_vector);
	// let idx = index
	Operand* idx = exp_lower(lir_func, exp->value.ArrayAccess.index, translation_vector);
	// let elem be a fresh var of type &t s . t . src :&t
	string elem;
	if(lir_func->locals.find(src->value.Var.id) != lir_func->locals.end())
		elem = create_fresh_var(lir_func, lir_func->locals[src->value.Var.id]->value.Ptr.ref);
	else if(lir->globals.find(src->value.Var.id) != lir->globals.end())
		elem = create_fresh_var(lir_func, lir->globals[src->value.Var.id]->value.Ptr.ref);
	else
		cout << "Bad Bad" << endl;
	// let lhs be a fresh var of type t s . t . src :&t
	string lhs;
	if(lir_func->locals.find(src->value.Var.id) != lir_func->locals.end())
		lhs = create_fresh_var(lir_func, lir_func->locals[src->value.Var.id]->value.Ptr.ref);
	else if(lir->globals.find(src->value.Var.id) != lir->globals.end())
		lhs = create_fresh_var(lir_func, lir->globals[src->value.Var.id]->value.Ptr.ref);
	else
		cout << "Bad Bad" << endl;
	// emit Gep(elem, src, idx)
	LirInst* gep = new LirInst(LirInst::Gep);
	gep->value.Gep.lhs = elem;
	gep->value.Gep.src = src->value.Var.id;
	gep->value.Gep.idx = idx;
	translation_vector.push_back(gep);
	// emit Load(lhs, elem)
	LirInst* load = new LirInst(LirInst::Load);
	load->value.Load.lhs = lhs;
	load->value.Load.src = elem;
	translation_vector.push_back(load);
	// lhs
	Operand* to_be_returned_op = new Operand(Operand::Var);
	to_be_returned_op->value.Var.id = lhs;
	return to_be_returned_op;
}

Operand* fieldaccess_lower(LIR_Function* lir_func, Exp* exp, vector<LIR*>& translation_vector){
	// let src = ptr
	Operand* src = exp_lower(lir_func, exp->value.FieldAccess.ptr, translation_vector);
	// let fldp be a fresh var of type &t s . t . src :& Structid , id [ fld ]:t
	// cout << src->value.Var.id << ": " << lir_func->locals[src->value.Var.id]->type_string() << endl;
	string struct_name;
	if(lir_func->locals.find(src->value.Var.id) != lir_func->locals.end())
		struct_name = lir_func->locals[src->value.Var.id]->value.Ptr.ref->value.Struct.name;
	else if(lir->globals.find(src->value.Var.id) != lir->globals.end())
		struct_name = lir->globals[src->value.Var.id]->value.Ptr.ref->value.Struct.name;
	else
		cout << "Bad Bad" << endl;
	Type* fldp_type = new Type(Type::Ptr);
	fldp_type->value.Ptr.ref = lir->structs[struct_name][exp->value.FieldAccess.field];
	
	string fldp = create_fresh_var(lir_func, fldp_type);
	// let lhs be a fresh var of type t s . t . src :& Structid , id [ fld ]:t
	string lhs = create_fresh_var(lir_func, lir->structs[struct_name][exp->value.FieldAccess.field]);
	// emit Gfp(fldp, src, fld)
	LirInst* gfp = new LirInst(LirInst::Gfp);
	gfp->value.Gfp.lhs = fldp;
	gfp->value.Gfp.src = src->value.Var.id;
	gfp->value.Gfp.field = exp->value.FieldAccess.field;
	translation_vector.push_back(gfp);
	// emit Load(lhs, fldp)
	LirInst* load = new LirInst(LirInst::Load);
	load->value.Load.lhs = lhs;
	load->value.Load.src = fldp;
	translation_vector.push_back(load);
	// lhs
	Operand* to_be_returned_op = new Operand(Operand::Var);
	to_be_returned_op->value.Var.id = lhs;
	return to_be_returned_op;
}

Operand* call_lower(LIR_Function* lir_func, Exp* exp, vector<LIR*>& translation_vector){
	// let aops = ∀a ∈ args . [a]
	vector<Operand*> aops;
	for(Exp* arg: exp->value.Call.args){
		aops.push_back(exp_lower(lir_func, arg, translation_vector));
	}
	// let direct = callee is Id(name) and name is not shadowed by a local / parameter
	bool direct = exp->value.Call.callee->type == Exp::Id;
	direct = direct && lir_func->locals.find(exp->value.Call.callee->value.Id.name) == lir_func->locals.end();
	// let fun = callee
	Operand* fun = exp_lower(lir_func, exp->value.Call.callee, translation_vector);
	// let lhs be a fresh var of type τ s . t . fun :&( _ )→ τ
	string lhs;
	if(lir->functions.find(fun->value.Var.id) != lir->functions.end())
		lhs = create_fresh_var(lir_func, lir->functions[fun->value.Var.id]->rettyp);
	else
		lhs = create_fresh_var(lir_func, lir->externs[fun->value.Var.id]);
	// if direct and name is an extern then emit CallExt(lhs, name, aops)
	if(direct && lir->externs.find(exp->value.Call.callee->value.Id.name) != lir->externs.end()){
		LirInst* call_ext = new LirInst(LirInst::CallExt);
		call_ext->value.CallExt.lhs = lhs;
		call_ext->value.CallExt.callee = exp->value.Call.callee->value.Id.name;
		call_ext->value.CallExt.args = aops;
		translation_vector.push_back(call_ext);
	}
	// else
	else{
		// let NEXT be a fresh label
		string NEXT = create_fresh_label(lir_func);
		// if direct and name is a function then emit CallDirect(lhs, name, aops, NEXT)
		if(direct && lir->functions.find(exp->value.Call.callee->value.Id.name) != lir->functions.end()){
			Terminal* call_direct = new Terminal(Terminal::CallDirect);
			call_direct->value.CallDirect.lhs = lhs;
			call_direct->value.CallDirect.callee = exp->value.Call.callee->value.Id.name;
			call_direct->value.CallDirect.args = aops;
			call_direct->value.CallDirect.next_bb = NEXT;
			translation_vector.push_back(call_direct);
		}
		// else emit CallIndirect(lhs, fun, aops, NEXT)
		else{
			Terminal* call_indirect = new Terminal(Terminal::CallIndirect);
			call_indirect->value.CallIndirect.lhs = lhs;
			call_indirect->value.CallIndirect.callee = fun->value.Var.id;
			call_indirect->value.CallIndirect.args = aops;
			call_indirect->value.CallIndirect.next_bb = NEXT;
			translation_vector.push_back(call_indirect);
		}
		// emit Label(NEXT)
		translation_vector.push_back(new Label(NEXT));		
	}
	// lhs
	Operand* to_be_returned_op = new Operand(Operand::Var);
	to_be_returned_op->value.Var.id = lhs;
	return to_be_returned_op;
}

/* 
 * 2.4 Lowering Lvals, NOT AN ID
 *
 * These functions emit LIR instructions into the translation vector that will compute the location where a 
 * value should be stored and return a variable that contains a pointer to that location. Note that the argument
 * should not be an Id.
 */
Operand* lval_lower(LIR_Function* lir_func, Lval* lval, vector<LIR*>& translation_vector){
	Operand* operand = NULL;
	if (lval->type == Lval::Deref){
		operand = deref_lower(lir_func, lval, translation_vector);
	} 
	else if (lval->type == Lval::ArrayAccess){
		operand = arrayaccess_lower(lir_func, lval, translation_vector);
	} 
	else if (lval->type == Lval::FieldAccess){
		operand = fieldaccess_lower(lir_func, lval, translation_vector);
	} 
	else if (lval->type == Lval::Id){ 
		cout << "ID WAS INPUTTED, not valid" << endl;
	}
	return operand;
}

Operand* deref_lower(LIR_Function* lir_func, Lval* lval, vector<LIR*>& translation_vector){
	return lval_exp_lower(lir_func, lval->value.Deref.lval, translation_vector);
}

Operand* arrayaccess_lower(LIR_Function* lir_func, Lval* lval, vector<LIR*>& translation_vector){
	// let src = ptr
	Operand* src = lval_exp_lower(lir_func, lval->value.ArrayAccess.ptr, translation_vector);
	// let idx = index
	Operand* idx = exp_lower(lir_func, lval->value.ArrayAccess.index, translation_vector);
	// let lhs be a fresh var of type τ s . t . src :τ
	string lhs;
	if(lir_func->locals.find(src->value.Var.id) != lir_func->locals.end())
		lhs = create_fresh_var(lir_func, lir_func->locals[src->value.Var.id]->value.Ptr.ref);
	else if(lir->globals.find(src->value.Var.id) != lir->globals.end())
		lhs = create_fresh_var(lir_func, lir->globals[src->value.Var.id]->value.Ptr.ref);
	else
		cout << "Bad Bad" << endl;
	// emit Gep(lhs, src, idx)
	LirInst* gep = new LirInst(LirInst::Gep);
	gep->value.Gep.lhs = lhs;
	gep->value.Gep.src = src->value.Var.id;
	gep->value.Gep.idx = idx;
	translation_vector.push_back(gep);
	// lhs
	Operand* to_be_returned_op = new Operand(Operand::Var);
	to_be_returned_op->value.Var.id = lhs;
	return to_be_returned_op;
}

Operand* fieldaccess_lower(LIR_Function* lir_func, Lval* lval, vector<LIR*>& translation_vector){
	// let src = ptr
	Operand* src = lval_exp_lower(lir_func, lval->value.FieldAccess.ptr, translation_vector);
	// let lhs be a fresh var of type &τ s . t . src :& Structid , id [ fld ]:τ
	string struct_name;
	if(lir_func->locals.find(src->value.Var.id) != lir_func->locals.end())
		struct_name = lir_func->locals[src->value.Var.id]->value.Ptr.ref->value.Struct.name;
	else if(lir->globals.find(src->value.Var.id) != lir->globals.end())
		struct_name = lir->globals[src->value.Var.id]->value.Ptr.ref->value.Struct.name;
	else
		cout << "Bad Bad" << endl;
	Type* lhs_type = new Type(Type::Ptr);
	lhs_type->value.Ptr.ref = lir->structs[struct_name][lval->value.FieldAccess.field];
	string lhs = create_fresh_var(lir_func, lhs_type);
	// emit Gfp(lhs, src, fld)
	LirInst* gfp = new LirInst(LirInst::Gfp);
	gfp->value.Gfp.lhs = lhs;
	gfp->value.Gfp.src = src->value.Var.id;
	gfp->value.Gfp.field = lval->value.FieldAccess.field;
	translation_vector.push_back(gfp);
	// lhs
	Operand* to_be_returned_op = new Operand(Operand::Var);
	to_be_returned_op->value.Var.id = lhs;
	return to_be_returned_op;
}

/*
 * 2.5 Lowering Lvals as Expressions
 *
 * These functions lower an Lval as if it were an expression, returning a variable containing the final value.
 * Note that the argument can be an Id.
 */
Operand* lval_exp_lower(LIR_Function* lir_func, Lval* lval, vector<LIR*>& translation_vector){
	Operand* operand;
	if (lval->type == Lval::Id){
		operand = id_lower(lir_func, lval, translation_vector);
	} else {
		operand = not_id_lower(lir_func, lval, translation_vector);
	}
	return operand;
}

Operand* id_lower(LIR_Function* lir_func, Lval* lval, vector<LIR*>& translation_vector){
	Operand* var = new Operand(Operand::Var);
	var->value.Var.id = lval->value.Id.name;
	return var;
}

Operand* not_id_lower(LIR_Function* lir_func, Lval* lval, vector<LIR*>& translation_vector){
	// let src = [[lv]]^l
	Operand* src = lval_lower(lir_func, lval, translation_vector);
	// let lhs be a fresh var of type T s . t . src :&T
	string lhs;
	if(lir_func->locals.find(src->value.Var.id) != lir_func->locals.end())
		lhs = create_fresh_var(lir_func, lir_func->locals[src->value.Var.id]->value.Ptr.ref);
	else if(lir->globals.find(src->value.Var.id) != lir->globals.end())
		lhs = create_fresh_var(lir_func, lir->globals[src->value.Var.id]->value.Ptr.ref);
	else
		cout << "Bad Bad" << endl;
	// emit Load(lhs, src)
	LirInst* load = new LirInst(LirInst::Load);
	load->value.Load.lhs = lhs;
	load->value.Load.src = src->value.Var.id;
	translation_vector.push_back(load);
	// lhs
	Operand* to_be_returned_op = new Operand(Operand::Var);
	to_be_returned_op->value.Var.id = lhs;
	return to_be_returned_op;
}


/* helper function that takes a type and
 * (1) creates a new variable of that type, inserting it into the enclosing function's locals;
 * and (2) returns that variable to the caller. T 
 */
string create_fresh_var(LIR_Function* lir_func, Type* type){
	string func_name = lir_func->name;
	
	if(fresh_vars.find(func_name) == fresh_vars.end()){
		fresh_vars[func_name] = 1;
	}

	// Add the label to the locals of the lir_function
	string label = "_t" + std::to_string(fresh_vars[func_name]);
	lir_func->locals[label] = type;
	
	// Increase the counter
	fresh_vars[func_name]++;

	return label;
}

/*
 * To create fresh labels. Again we will use a helper function. To make the label names consistent 
 * they should be called lbln, where n is a counter that starts from 1 for each function and increases 
 * each time the helper function is called (e.g., lbl1, lbl2, etc). 
 */
string create_fresh_label(LIR_Function* lir_func){
	string func_name = lir_func->name;
	if(fresh_labels.find(func_name) == fresh_labels.end()){
		fresh_labels[func_name] = 1;
	}

	string label = "lbl" + std::to_string(fresh_labels[func_name]);
	fresh_labels[func_name]++;

	return label;
}

void BasicBlock::set_reachable(map<string, BasicBlock*>& body){
	// if this block is already reachable, return
	if(reachable){
		return;
	}
	// set this block as reachable
	reachable = true;
	// if this block has a terminal instruction
	if(term->type == Terminal::Branch){
		// set the true branch as reachable
		body[term->value.Branch.tt]->set_reachable(body);
		// set the false branch as reachable
		body[term->value.Branch.ff]->set_reachable(body);
	}
	else if(term->type == Terminal::Jump){
		// set the next block as reachable
		body[term->value.Jump.next_bb]->set_reachable(body);
	}
	else if(term->type == Terminal::CallDirect){
		// set the next block as reachable
		body[term->value.CallDirect.next_bb]->set_reachable(body);
	}
	else if(term->type == Terminal::CallIndirect){
		// set the next block as reachable
		body[term->value.CallIndirect.next_bb]->set_reachable(body);
	}
	else if(term->type == Terminal::Ret){
		num_ret++;
	}
}

void LIR_Program::toString(){
	//Printing the structs if theres more than 1
	if(structs.size() > 0){
		for (auto s : structs){
			cout << "Struct " << s.first << endl;
			for (auto f : s.second){
				cout << "  " << f.first << " : " << f.second->type_string() << endl;
			}
			cout << endl;
		}
	}

	cout << "Externs" << endl;
	for (auto e : externs){
		cout << "  " << e.first << " : " << e.second->type_string() << endl;
	}
	cout << endl;

	cout << "Globals" << endl;
	for(auto g: globals){
		cout << "  " << g.first << " : " << g.second->type_string() << endl;
	}

	for(auto f: functions){
		f.second->toString();
	}
	cout << endl;
};

void LIR_Function::toString(){
	cout << endl << "Function " << name << "(";
	auto it = params.begin();
	if(params.size() != 0){
		for(long unsigned int i = 0; i < params.size()-1; i++){
			cout << it->first << ":" << it->second->type_string() << ", ";
			it++;
		}
		cout << it->first << ":" << it->second->type_string();
	}
	cout << ") -> " << rettyp->type_string() << " {" << endl;

	// locals
	cout << "  Locals" << endl;
	for(auto e : locals){
		cout << "    " << e.first << " : " << e.second->type_string() << endl;
	}

	for(auto e : body){
		e.second->toString();
	}
	
	cout << "}" << endl;
	
	// bbs
};

void BasicBlock::toString(){
	if(reachable == false)
		return;
	cout << endl << "  " << label << ":" << endl;
	// vector<LirInst*> insts; //The translational vector
	for (LirInst* e : insts){
		cout << "    ";
		e->toString();
	}
	// Terminal* term
	cout << "    ";
	term->toString();
};

// | Alloc { lhs: VarId, num: Operand }
// | Arith { lhs: VarId, aop: ArithmeticOp, left: Operand, right: Operand }
// | CallExt { lhs: option<VarId>, callee: FuncId, args: vector<Operand> }
// | Cmp { lhs: VarId, aop: ComparisonOp, left: Operand, right: Operand }
// | Copy { lhs: VarId, op: Operand }
// | Gep { lhs: VarId, src: VarId, idx: Operand }
// | Gfp { lhs: VarId, src: VarId, field: string }
// | Load { lhs: VarId, src: VarId }
// | Store { dst: VarId, op: Operand }

// enum type{Alloc, Arith, CallExt, Cmp, Copy, Gep, Gfp, Load, Store} type;
void LirInst::toString(){
	if(type == LirInst::Alloc){
		cout << "Alloc(" << value.Alloc.lhs << ", ";
		value.Alloc.num->toString();
		cout << ")" << endl;
	}
	else if(type == LirInst::Arith){
		cout << "Arith(" << value.Arith.lhs << ", ";
		value.Arith.aop->toString();
		cout << ", ";
		value.Arith.left->toString();
		cout << ", ";
		value.Arith.right->toString();
		cout << ")" << endl;
	}
	else if(type == LirInst::CallExt){
		cout << "CallExt(";
		if(value.CallExt.lhs != ""){
			cout << value.CallExt.lhs << ", ";
		}
		cout << value.CallExt.callee << ", [";
		if(value.CallExt.args.size() != 0){
			for(long unsigned int i = 0; i < value.CallExt.args.size() - 1; i++){
				value.CallExt.args[i]->toString();
				cout << ", ";
			}
			value.CallExt.args[value.CallExt.args.size() - 1]->toString();
		}
		cout << "])" << endl;
	}
	else if(type == LirInst::Cmp){
		cout << "Cmp(" << value.Cmp.lhs << ", ";
		value.Cmp.aop->toString();
		cout << ", ";
		value.Cmp.left->toString();
		cout << ", ";
		value.Cmp.right->toString();
		cout << ")" << endl;
	}
	else if(type == LirInst::Copy){
		cout << "Copy(" << value.Copy.lhs << ", ";
		value.Copy.op->toString();
		cout << ")" << endl;
	}
	else if(type == LirInst::Gep){
		cout << "Gep(" << value.Gep.lhs << ", ";
		cout << value.Gep.src << ", ";
		value.Gep.idx->toString();
		cout << ")" << endl;
	}
	else if(type == LirInst::Gfp){
		cout << "Gfp(" << value.Gfp.lhs << ", ";
		cout << value.Gfp.src << ", ";
		cout << value.Gfp.field << ")" << endl;
	}
	else if(type == LirInst::Load){
		cout << "Load(" << value.Load.lhs << ", ";
		cout << value.Load.src << ")" << endl;
	}
	else if(type == LirInst::Store){
		cout << "Store(" << value.Store.dst << ", ";
		value.Store.op->toString();
		cout << ")" << endl;
	}
};
	// enum type{Branch, CallDirect, CallIndirect, Jump, Ret} type;
void Terminal::toString(){
	if(type == Terminal::Branch){
		cout << "Branch(";
		value.Branch.guard->toString();
		cout << ", " << value.Branch.tt << ", " << value.Branch.ff << ")" << endl;
	}
	else if(type == Terminal::CallDirect){
		cout << "CallDirect(";
		if(value.CallDirect.lhs != ""){
			cout << value.CallDirect.lhs << ", ";
		}
		cout << value.CallDirect.callee << ", [";
		if(value.CallDirect.args.size() != 0){
			for(long unsigned int i = 0; i < value.CallDirect.args.size() - 1; i++){
				value.CallDirect.args[i]->toString();
				cout << ", ";
			}
			value.CallDirect.args[value.CallDirect.args.size() - 1]->toString();
		}
		cout << "], " << value.CallDirect.next_bb << ")" << endl;
	}
	else if(type == Terminal::CallIndirect){
		cout << "CallIndirect(";
		if(value.CallIndirect.lhs != ""){
			cout << value.CallIndirect.lhs << ", ";
		}
		cout << value.CallIndirect.callee << ", [";
		if(value.CallIndirect.args.size() != 0){
			for(long unsigned int i = 0; i < value.CallIndirect.args.size() - 1; i++){
				value.CallIndirect.args[i]->toString();
				cout << ", ";
			}
			value.CallIndirect.args[value.CallIndirect.args.size() - 1]->toString();
		}
		cout << "], " << value.CallIndirect.next_bb << ")" << endl;
	}
	else if(type == Terminal::Jump){
		cout << "Jump(" << value.Jump.next_bb << ")" << endl;
	}
	else if(type == Terminal::Ret){
		cout << "Ret("; 
		if(value.Ret.op != NULL){
			value.Ret.op->toString();
		}
		cout << ")" << endl;
	}
};

	// enum type{Const, Var} type;
void Operand::toString(){
	if(type == Operand::Const){
		cout << value.Const.num;
	}
	else if(type == Operand::Var){
		cout << value.Var.id;
	}
};

//   enum type {Add, Sub, Mul, Div} type;
void ArithmeticOp::toString(){
	if(type == ArithmeticOp::Add){
		cout << "add";
	}
	else if(type == ArithmeticOp::Sub){
		cout << "sub";
	}
	else if(type == ArithmeticOp::Mul){
		cout << "mul";
	}
	else if(type == ArithmeticOp::Div){
		cout << "div";
	}
};

// enum type {Equal, NotEq, Lt, Lte, Gt, Gte} type;
void ComparisonOp::toString(){
	if(type == ComparisonOp::Equal){
		cout << "eq";
	}
	else if(type == ComparisonOp::NotEq){
		cout << "neq";
	}
	else if(type == ComparisonOp::Lt){
		cout << "lt";
	}
	else if(type == ComparisonOp::Lte){
		cout << "lte";
	}
	else if(type == ComparisonOp::Gt){
		cout << "gt";
	}
	else if(type == ComparisonOp::Gte){
		cout << "gte";
	}
};

void Label::toString(){
	cout << "gen was here. not good";
};