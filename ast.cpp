#include <iostream>
#include <string>
#include <vector>
#include "ast.hpp"
using namespace std;

int indent = 0;

bool DEBUG = false;

string get_struct_name(Exp* a){
    if(a->type == Exp::ArrayAccess){
        return get_struct_name(a->value.ArrayAccess.ptr);
    }
    else if(a->type == Exp::FieldAccess){
        return get_struct_name(a->value.FieldAccess.ptr);
    }
    else if(a->type == Exp::Id){
        return a->value.Id.name;
    }
    else if(a->type == Exp::UnOp){
        return get_struct_name(a->value.UnOp.operand);
    }
    else if(a->type == Exp::Call){
        return get_struct_name(a->value.Call.callee);
    }
    else{
        return "";
    }
}

string get_struct_name(Lval* a){
    if(a->type == Lval::ArrayAccess){
        return get_struct_name(a->value.ArrayAccess.ptr);
    }
    else if(a->type == Lval::FieldAccess){
        return get_struct_name(a->value.FieldAccess.ptr);
    }
    else if(a->type == Lval::Deref){
        return get_struct_name(a->value.Deref.lval);
    }
    else if(a->type == Lval::Id){
        return a->value.Id.name;
    }
    else{
        return "";
    }
}

string Type::type_string(){
  string ret_str;
  switch(type){
    case Int:
      return "Int";
    case Struct:
      return "Struct(" + value.Struct.name + ")";
    case Fn:
    //   (int, &int) -> int
      ret_str = "Fn([";
      if(value.Fn.prms.size() != 0){
        for(long unsigned int i = 0; i < value.Fn.prms.size()-1; i++){
          ret_str += value.Fn.prms[i]->type_string();
          if(i != value.Fn.prms.size() - 1)
            ret_str += ", ";
        }
        ret_str += value.Fn.prms[value.Fn.prms.size()-1]->type_string();
      }
      ret_str += "], ";
      if(value.Fn.ret != NULL)
        ret_str += value.Fn.ret->type_string();
      else
        ret_str += "_";
      ret_str += ")";
      return ret_str;
    case Ptr:
      return "Ptr(" + value.Ptr.ref->type_string() + ")";
    case Any:
      return "_";
  }
  return "ERROR";
}

void indent_print(string s, bool newline = true){
  for(int i = 0; i < indent; i++){
    cout << "  ";
  }
  cout << s;
  if(newline){
    cout << endl;
  }
}

void single_print(string name, vector<AST*> ast, bool short_print = false, bool period = true){
  if (DEBUG) cout << "(single print)" << endl;
  // 0, 1, and 2+ cases
  if (ast.size() == 0){
    if(period){
      indent_print(name + " = [],");
    }
    else{
      indent_print(name + " = []");
    }
  }
  else if(ast.size() == 1 && !short_print){
    indent_print(name + " = [");
    indent++;
    ast[0]->toString();
    cout << endl;
    indent--;
    if(period){
      indent_print("],");
    }
    else{
      indent_print("]");
    }
  }
  else if(ast.size() == 1 && short_print){
    indent_print(name + " = [", false);
    ast[0]->toString();
    if(period){
      cout << "]," << endl;
    }
    else{
      cout << "]" << endl;
    }
  }
  else if(short_print){
    indent_print(name + " = [");
    indent++;
    for(long unsigned int i = 0; i < ast.size()-1; i++){
      indent_print("", false);
      ast[i]->toString();
      cout << "," << endl;
    }
    indent_print("", false);
    ast[ast.size()-1]->toString();
    cout << endl;
    indent--;
    if(period){
      indent_print("],");
    }
    else{
      indent_print("]");
    }
  }
  else{
    indent_print(name + " = [");
    indent++;
    for(long unsigned int i = 0; i < ast.size()-1; i++){
      ast[i]->toString();
      cout << "," << endl;
    }
    ast[ast.size()-1]->toString();
    cout << endl;
    indent--;
    if(period){
      indent_print("],");
    }
    else{
      indent_print("]");
    }
  }
}

// Fn(prms = [Int, Int], ret = Int)
void Type::toString(){
  if (DEBUG) cout << "(Type)" << endl;
  switch(type){
    case Int:
      cout << "Int";
      break;
    case Struct:
      cout << "Struct(" << value.Struct.name << ")";
      break;
    case Fn:
      cout << "Fn(prms = [";
      if(value.Fn.prms.size() != 0){
        for(long unsigned int i = 0; i < value.Fn.prms.size() - 1; i++){
          value.Fn.prms[i]->toString();
          cout << ", ";
        }
        value.Fn.prms[value.Fn.prms.size()-1]->toString();
      }
      cout << "], ret = ";
      if(value.Fn.ret == NULL){
        cout << "_";
      }
      else{
        value.Fn.ret->toString();
      }
      cout << ")";
      break;
    case Ptr:
      cout << "Ptr(";
      value.Ptr.ref->toString();
      cout << ")";
      break;
    case Any:
      cout << "Any";
      break;
  }
}

void Decl::toString(){
  if (DEBUG) cout << "(Decl)" << endl;
  cout << "Decl(" << name << ", ";
  type->toString();
  cout << ")";
}

// Struct(
//   name = list,
//   fields = [
//     Decl(value, Int),
//     Decl(next, Ptr(Struct(list)))
//   ]
// )
void Struct::toString(){
  if (DEBUG) cout << "(Struct)" << endl;
  indent_print("Struct(");
  indent++;
  indent_print("name = " + name + ",");
  vector<AST*> temp(fields.begin(), fields.end());
  single_print("fields", temp, true, false);
  indent--;
  indent_print(")", false);
}

void UnaryOp::toString(){
  if (DEBUG) cout << "(UnaryOp)" << endl;
  switch(type){
    case Neg:
      cout << "Neg";
      break;
    case Deref:
      cout << "Deref";
      break;
  }
}

void BinaryOp::toString(){
  if (DEBUG) cout << "(BinaryOp)" << endl;
  switch(type){
    case Add:
      cout << "Add";
      break;
    case Sub:
      cout << "Sub";
      break;
    case Mul:
      cout << "Mul";
      break;
    case Div:
      cout << "Div";
      break;
    case Equal:
      cout << "Equal";
      break;
    case NotEq:
      cout << "NotEq";
      break;
    case Lt:
      cout << "Lt";
      break;
    case Lte:
      cout << "Lte";
      break;
    case Gt:
      cout << "Gt";
      break;
    case Gte:
      cout << "Gte";
      break;
  }
}

// stmts = [
//   Assign(
//     lhs = FieldAccess(
//       ptr = Id(x),
//       field = value
//     ),
//     rhs = Neg(BinOp(
//       op = Add,
//       left = Num(1),
//       right = Num(5)
//     ))
//   ),
//   Return(
//     Num(1)
//   )
// ]
void Exp::toString(){
  if (DEBUG) cout << "(Exp)" << endl;
  switch(type){
    case Num:
      cout << "Num(" << value.Num.n << ")";
      break;
    case Id:
      cout << "Id(" << value.Id.name << ")";
      break;
    case Nil:
      cout << "Nil";
      break;
    case UnOp:
      value.UnOp.op->toString();
      cout << "(";
      value.UnOp.operand->toString();
      cout << ")";
      break;
    case BinOp:
      cout << "BinOp(" << endl;
      indent++;
      indent_print("op = ", false);
      value.BinOp.op->toString();
      cout << "," << endl;
      indent_print("left = ", false);
      value.BinOp.left->toString();
      cout << "," << endl;
      indent_print("right = ", false);
      value.BinOp.right->toString();
      cout << endl;
      indent--;
      indent_print(")", false);
      break;
    case ArrayAccess:
      cout << "ArrayAccess(" << endl;
      indent++;
      indent_print("ptr = ", false);
      value.ArrayAccess.ptr->toString();
      cout << "," << endl;
      indent_print("index = ", false);
      value.ArrayAccess.index->toString();
      cout << endl;
      indent--;
      indent_print(")", false);
      break;
    case FieldAccess:
      cout << "FieldAccess(" << endl;
      indent++;
      indent_print("ptr = ", false);
      value.FieldAccess.ptr->toString();
      cout << "," << endl;
      indent_print("field = " + value.FieldAccess.field);
      indent--;
      indent_print(")", false);
      break;
    case Call:
      cout << "Call(" << endl;
      indent++;
      indent_print("callee = ", false);
      value.Call.callee->toString();
      cout << "," << endl;
      vector<AST*> temp(value.Call.args.begin(), value.Call.args.end());
      single_print("args", temp, true, false);
      indent--;
      indent_print(")", false);
      break;
  }
}

void Rhs::toString(){
  if (DEBUG) cout << "(Rhs)" << endl;
  switch(type){
    case RhsExp:
      value.RhsExp.exp->toString();
      break;
    case New:
      cout << "New(";
      value.New.type->toString();
      cout << ", ";
      value.New.amount->toString();
      cout << ")";
      break;
  }
}

  // enum {Id, Deref, ArrayAccess, FieldAccess} type;
void Lval::toString(){
  if (DEBUG) cout << "(Lval)" << endl;
  switch(type){
    case Id:
      cout << "Id(" << value.Id.name << ")";
      break;
    case Deref:
      cout << "Deref(";
      value.Deref.lval->toString();
      cout << ")";
      break;
    case ArrayAccess:
      cout << "ArrayAccess(" << endl;
      indent++;
      indent_print("ptr = ", false);
      value.ArrayAccess.ptr->toString();
      cout << "," << endl;
      indent_print("index = ", false);
      value.ArrayAccess.index->toString();
      cout << endl;
      indent--;
      indent_print(")", false);
      break;
    case FieldAccess:
      cout << "FieldAccess(" << endl;
      indent++;
      indent_print("ptr = ", false);
      value.FieldAccess.ptr->toString();
      cout << "," << endl;
      indent_print("field = " + value.FieldAccess.field);
      indent--;
      indent_print(")", false);
      break;
  }
}

void Stmt::toString(){
  if (DEBUG) cout << "(Stmt)" << endl;
  vector<AST*> temp;
  switch(type){
    case Break:
      indent_print("Break", false);
      break;
    case Continue:
      indent_print("Continue", false);
      break;
    case Return:
      indent_print("Return(", false);
      if(value.Return.exp == NULL){
        cout << "_)";
      }
      else{
        cout << endl;
        indent++;
        indent_print("", false);
        value.Return.exp->toString();
        cout << endl;
        indent--;
        indent_print(")", false);
      }
      break;
    case Assign:
      indent_print("Assign(");
      indent++;
      indent_print("lhs = ", false);
      value.Assign.lhs->toString();
      cout << "," << endl;
      indent_print("rhs = ", false);
      value.Assign.rhs->toString();
      cout << endl;
      indent--;
      indent_print(")", false);
      break;
    case Call:
      indent_print("Call(");
      indent++;
      indent_print("callee = ", false);
      value.Call.callee->toString();
      cout << "," << endl;
      temp = vector<AST*>(value.Call.args.begin(), value.Call.args.end());
      single_print("args", temp, true, false);
      indent--;
      indent_print(")", false);
      break;
    case If:
      indent_print("If(");
      indent++;
      indent_print("guard = ", false);
      value.If.guard->toString();
      cout << "," << endl;
      temp = vector<AST*>(value.If.tt.begin(), value.If.tt.end());
      single_print("tt", temp, false, true);
      temp = vector<AST*>(value.If.ff.begin(), value.If.ff.end());
      single_print("ff", temp, false, false);
      indent--;
      indent_print(")", false);
      break;
    case While:
      indent_print("While(");
      indent++;
      indent_print("guard = ", false);
      value.While.guard->toString();
      cout << "," << endl;
      temp = vector<AST*>(value.While.body.begin(), value.While.body.end());
      single_print("body", temp, false, false);
      indent--;
      indent_print(")", false);
      break;
  }
}


// Function(
//   name = main,
//   params = [],
//   rettyp = Int,
//   locals = [],
//   stmts = [
//     Return(
//       Num(42)
//     )
//   ]
// ),
void Function::toString(){
  if (DEBUG) cout << "(Function)" << endl;
  indent_print("Function(");
  indent++;
  indent_print("name = " + name + ",");
  
  // params 0, 1, and 2+ cases
  vector<AST*> temp(params.begin(), params.end());
  single_print("params", temp, true, true);

  // TODO: Not sure if the NULL check works
  if(rettyp == NULL){
    indent_print("rettyp = _,");
  }
  else{
    indent_print("rettyp = ", false);
    rettyp->toString();
    cout << "," << endl;
  }

  // locals 0, 1, and 2+ cases
  if(locals.size() == 0){
    indent_print("locals = [],");
  }
  else if(locals.size() == 1){
    indent_print("locals = [(", false);
    locals[0].first->toString();
    cout << ", ";
    if(locals[0].second == NULL){
      cout << "_";
    }
    else{
      locals[0].second->toString();
    }
    cout << ")]," << endl;
  }
  else{
    indent_print("locals = [");
    indent++;
    for(long unsigned int i = 0; i < locals.size()-1; i++){
      indent_print("(", false);
      indent++;
      locals[i].first->toString();
      cout << ", ";
      if(locals[i].second == NULL){
        cout << "_";
      }
      else{
        locals[i].second->toString();
      }
      cout << ")," << endl;
      indent--;
    }
    indent_print("(", false);
    indent++;
    locals[locals.size()-1].first->toString();
    cout << ", ";
    if(locals[locals.size()-1].second == NULL){
      cout << "_";
    }
    else{
      locals[locals.size()-1].second->toString();
    }
    cout << ")" << endl;
    indent--;
    indent--;
    indent_print("],");
  }

  // stmts 0, 1, and 2+ cases
  vector<AST*> temp2(stmts.begin(), stmts.end());
  single_print("stmts", temp2, false, false);

  indent--;
  indent_print(")", false);
}

void Program::toString(){
  if (DEBUG) cout << "(Program)" << endl;
  indent_print("Program(");
  indent++;
  // globals 0, 1, and 2+ cases
  vector<AST*> temp(globals.begin(), globals.end());
  single_print("globals", temp, true, true);

  // structs 0, 1, and 2+ cases
  vector<AST*> temp2(structs.begin(), structs.end());
  single_print("structs", temp2);

  // externs 0, 1, and 2+ cases
  vector<AST*> temp3(externs.begin(), externs.end());
  single_print("externs", temp3, true, true);

  // functions 0, 1, and 2+ cases
  vector<AST*> temp4(functions.begin(), functions.end());
  single_print("functions", temp4, false, false);

  indent--;
  indent_print(")");
}