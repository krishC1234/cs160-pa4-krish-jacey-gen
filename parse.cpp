/**
 * An implementation of an LL(1) grammar in C++.
 * This code represents the following grammar,
 * and it is implemented using the program call
 * stack, rather than an explicit stack variable.
 *
 * S ::= xSx | Qy
 * Q ::= aQ | b
 */

/*
Tokens:
Num
Id
Int
Struct
Nil
Break
Continue
Return
If
Else
While
New
Let
Extern
Fn
Address
Colon
Semicolon
Comma
Underscore
Arrow
Plus
Dash
Star
Slash
Equal
NotEq
Lt
Lte
Gt
Gte
Dot
Gets
OpenParen
CloseParen
OpenBracket
CloseBracket
OpenBrace
CloseBrace
*/

#include <iostream>
#include <stdexcept>
#include <utility>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <typeinfo>
#include <cstring>

#include "parse.hpp"
#include "ast.hpp"
#include "lower.hpp"

using namespace std;

string* tokens;

bool DEBUG_MODE = false;
int token_index = 0;

int main(int argc, char* argv[]) {
    
    // Read in the file
    if(argc != 4){
        cout << "Usage: ./codegen <file_lir> <file_toks> <file_ast>" << endl;
        return 1;
    }
    string filename = argv[2];

    ifstream file(filename);
    if(!file.is_open()){
        cout << "Error: could not open file" << endl;
        return 1;
    }
    // convert whole file to string
    stringstream buffer;
    buffer << file.rdbuf(); // learned from https://stackoverflow.com/questions/2602013/read-whole-ascii-file-into-c-stdstring
    string input = buffer.str();

    //cout << input << endl;

    // Begin parsing the file

    tokens = new string[1000000];
    long unsigned int pos = 0;
    int index = 0;
    while(pos < input.size()){
        pos = input.find("\n");
        tokens[index] = input.substr(0,pos);
        input.erase(0,pos+1); // To ignore the \n
        index++;
    }
    tokens[index] = "\0";

    // string* temp = tokens;
    // while(*temp != "\0"){
    //     cout << "token: " << *temp << endl;
    //     temp++;
    // }
    // cout << endl;
    try{
        Program* prog  = program();
        // prog->toString();
        // cout << endl;
        LIR_Program* lir = lower(prog);
        // validate(prog);
        lir->toString();
        lir->codeGenString();
    }
    catch(const runtime_error& e){
        token_index--;
        cout << "parse error at token " << token_index << endl;
    }

    return 0;
}

/*
type ::= `&`* type_ad
*/

Type* type(){
    if(DEBUG_MODE) cout << "type: " << *tokens << endl;
    if(*tokens == "Address"){ 
        consume("Address");
        Type* t = new Type;
        t->type = Type::Ptr;
        t->value.Ptr.ref = type();
        return t;
    }
    Type* t = type_ad();
    return t;
}

/*
type_ad ::= `int`
          | id
          | `(` type_op
*/

Type* type_ad(){
    if(DEBUG_MODE) cout << "type_ad: " << *tokens << endl;
    Type* t;
    if(*tokens == "Int"){
        t = new Type;
        t->type = Type::Int;
        consume("Int");
    }
    else if(tokens->substr(0,3) == "Id("){
        t = new Type;
        t->type = Type::Struct;
        string id = consume_id();
        t->value.Struct.name = id;
    }
    else if(*tokens == "OpenParen"){
        consume("OpenParen");
        t = type_op();
    }
    else{
        token_index++;
        throw runtime_error{"Unexpected type_ad"};
    }
    return t;
}

/*
type_op ::= `)` type_ar
          | type type_fp
*/
Type* type_op(){
    if(DEBUG_MODE) cout << "type_op: " << *tokens << endl;
    Type* t = new Type;
    t->type = Type::Fn;
    //Case where there are no parameters and one return
    if(*tokens == "CloseParen"){
        consume("CloseParen");
        t->value.Fn.ret = type_ar();
    }
    //Case where 1+ params and one return
    else{
        t->value.Fn.prms.push_back(type());
        Type* temp = type_fp();
        for(long unsigned int i = 0; i < temp->value.Fn.prms.size(); i++){
            t->value.Fn.prms.push_back(temp->value.Fn.prms[i]);
        }
        t->value.Fn.ret = temp->value.Fn.ret;
    }
    return t;
}

/*
type_fp ::= `)` type_ar?
          | (`,` type)+ `)` type_ar
*/

Type* type_fp(){
    if(DEBUG_MODE) cout << "type_fp: " << *tokens << endl;
    Type* t = new Type;
    t->type = Type::Fn;
    if(*tokens == "CloseParen"){
        consume("CloseParen");
        t->value.Fn.ret = type_ar();
    }
    else{
        while(*tokens == "Comma"){
            consume("Comma");
            t->value.Fn.prms.push_back(type());
        }
        consume("CloseParen");
        t->value.Fn.ret = type_ar();
    }
    return t;
}

// type_ar ::= `->` rettyp

Type* type_ar(){
    if(DEBUG_MODE) cout << "type_ar: " << *tokens << endl;
    consume("Arrow");
    Type* t = rettyp();
    return t;
}

// funtype ::= `(` (type (`,` type)*)? `)` `->` rettyp

Type* funtype(){
    consume("OpenParen");
    Type* t = new Type;
    t->type = Type::Fn;
    if(*tokens != "CloseParen"){
        t->value.Fn.prms.push_back(type());
        while(*tokens == "Comma"){
            consume("Comma");
            t->value.Fn.prms.push_back(type());
        }
        consume("CloseParen");
        consume("Arrow");
        t->value.Fn.ret = rettyp();
    }
    else{
        consume("CloseParen");
        consume("Arrow");
        t->value.Fn.ret = rettyp();
    }
    return t;
}

/*
 rettyp ::= type 
          | `_`
*/

Type* rettyp(){
    if(*tokens == "Underscore"){
        consume("Underscore");
        return NULL;
    }
    return type();
}

// program ::= toplevel+
// toplevel ::= glob      # global variable declaration
//            | typedef   # struct type definition
//            | extern    # external function declaration
//            | fundef    # function definition
Program* program() {
    if(DEBUG_MODE) cout << "program: " << *tokens << endl;

    Program* p = new Program;

    while(*tokens != "\0"){ 
        if(*tokens == "Let"){
            //Call glob function 
            vector<Decl*> d = glob();
            //We need to merge both vectors (p and d)
            for(long unsigned int i = 0; i < d.size(); i++){
                p->globals.push_back(d[i]);
                // Adding the declarations to the global
            }
        }
        else if(*tokens == "Struct"){
            //Call typedef function
            Struct* s = typdef();  
            p->structs.push_back(s);
        }
        else if(*tokens == "Extern"){
            //Call extern function
            Decl* d = extrn();
            p->externs.push_back(d);
        }
        else if(*tokens == "Fn"){
            //Call fundef function
            Function* f = fundef();
            p->functions.push_back(f);
        }
        else{
            token_index++;
            throw runtime_error{"Expected toplevel"};
        }
    }
    return p;
}

/*
# global variable declaration.
glob ::= `let` decls `;`
*/
vector<Decl*> glob(){
    if(DEBUG_MODE) cout << "glob: " << *tokens << endl;
    consume("Let");
    vector<Decl*> temps = decls();
    consume("Semicolon");
    return temps;
}

/*
# variable / field declaration(s).
decls ::= decl (`,` decl)*
*/

vector<Decl*> decls(){
    if(DEBUG_MODE) cout << "decls: " << *tokens << endl;
    vector<Decl*> d;
    d.push_back(decl());
    while(*tokens == "Comma"){
        consume("Comma");
        d.push_back(decl());
    }
    return d;
}

/*
 decl ::= id `:` type
*/
Decl* decl(){
    if(DEBUG_MODE) cout << "decl: " << *tokens << endl;
    Decl* d = new Decl;
    string id = consume_id();
    d->name = id;
    consume("Colon");
    d->type = type();
    return d;
}

// typdef ::= `struct` id `{` decls `}`
Struct* typdef(){
    if(DEBUG_MODE) cout << "typdef: " << *tokens << endl;
    Struct* s = new Struct;
    
    consume("Struct");
    string id = consume_id();
    s->name = id;
    consume("OpenBrace");
    s->fields = decls(); //returns vector<Decl*>
    consume("CloseBrace");
    return s;
}

// extern ::= `extern` id `:` funtype `;`

Decl* extrn(){
    if(DEBUG_MODE) cout << "extrn: " << *tokens << endl;
    Decl* d = new Decl;
    consume("Extern");
    string id =consume_id();
    d->name = id;
    consume("Colon");
    d->type  = funtype();
    consume("Semicolon");
    return d;
}

// fundef ::= `fn` id `(` decls? `)` `->` rettyp `{` let* stmt+ `}`

// typedef struct Function{
//   FuncId name;
//   vector<Decl*> params;
//   Type* rettyp = NULL; //optional
//   vector<pair<Decl*, Exp*>> locals; //Exp is optional
//   vector<Stmt*> stmts;
// } Function;

Function* fundef(){
    if(DEBUG_MODE) cout << "fundef: " << *tokens << endl;
    Function* f = new Function;
    
    
    consume("Fn");
    string id = consume_id();
    f->name = id;

    consume("OpenParen");

    if(*tokens != "CloseParen"){
        f->params = decls();
    }
    consume("CloseParen");
    consume("Arrow");
    f->rettyp = rettyp();
    consume("OpenBrace");

    // let*
    while(*tokens == "Let"){
        vector<pair<Decl*, Exp*>> temp = let();
        for(long unsigned int i = 0; i < temp.size(); i++){
            f->locals.push_back(temp[i]);
        }
    }
    // stmt+
    f->stmts.push_back(stmt()); // ensure at least 1 stmt
    while(*tokens != "CloseBrace"){
        f->stmts.push_back(stmt());
    }
    consume("CloseBrace");
    return f;
}

// local variable declaration / initialization.
// let ::= `let` decl (`=` exp)? (`,` decl (`=` exp)?)* `;`

vector<pair<Decl*, Exp*>> let(){
    if(DEBUG_MODE) cout << "let: " << *tokens << endl;
    vector<pair<Decl*, Exp*>> d;
    consume("Let");
    pair<Decl*, Exp*> temp = {decl(), NULL};
    //If statement if there is 1 equals sign so 1 expression
    if(*tokens == "Gets"){
        consume("Gets");
        temp.second = exp();
        d.push_back(temp);
    }
    else{
        temp.second = NULL;
        d.push_back(temp);
    }
    //While loop is to handle all the comma cases
    while(*tokens == "Comma"){
        consume("Comma");
        temp = {decl(), NULL};
        if(*tokens == "Gets"){
            consume("Gets");
            temp.second = exp();
            d.push_back(temp);
        }
        else{
            temp.second = NULL;
            d.push_back(temp);
        }
    }
    consume("Semicolon");
    return d;
}

/*
stmt ::= cond               # conditional
       | loop               # loop
       | assign_or_call `;` # assignment or function call
       | `break` `;`        # break out of a loop
       | `continue` `;`     # continue to next iteration of loop
       | `return` exp? `;`  # return from function
*/

Stmt* stmt(){
    if(DEBUG_MODE) cout << "stmt: " << *tokens << endl;
    Stmt* s;
    if(*tokens == "If"){
        s = cond();
    }
    else if(*tokens == "While"){
        s = loop();
    }
    else if(tokens->substr(0,3) == "Id(" || *tokens == "Star"){
        s = assign_or_call();
        consume("Semicolon");
    }
    else if(*tokens == "Break"){
        s = new Stmt;
        s->type = Stmt::Break;
        consume("Break");
        consume("Semicolon");
    }
    else if(*tokens == "Continue"){
        s = new Stmt;
        s->type = Stmt::Continue;
        consume("Continue");
        consume("Semicolon");
    }
    else if(*tokens == "Return"){
        s = new Stmt;
        s->type = Stmt::Return;
        consume("Return");
        if(*tokens != "Semicolon"){
            s->value.Return.exp = exp();
        }
        consume("Semicolon");
    }
    else{
        token_index++;
        throw runtime_error{"Expected stmt"};
    }
    return s;
}

// cond ::= `if` exp block (`else` block)?
Stmt* cond(){
    if(DEBUG_MODE) cout << "cond: " << *tokens << endl;
    Stmt* s = new Stmt;
    s->type = Stmt::If;

    consume("If");
    
    s->value.If.guard = exp();
    s->value.If.tt = block();
    if(*tokens == "Else"){
        consume("Else");
        s->value.If.ff = block();
    }
    return s;
}

// loop ::= `while` exp block 
Stmt* loop(){
    if(DEBUG_MODE) cout << "loop: " << *tokens << endl;
    Stmt* s = new Stmt;
    s->type = Stmt::While;

    consume("While");
    s->value.While.guard = exp();
    s->value.While.body = block();
    return s;
}

// block ::= `{` stmt* `}`

vector<Stmt*> block(){
    if(DEBUG_MODE) cout << "block: " << *tokens << endl;
    vector<Stmt*> b;
    consume("OpenBrace");
    while(*tokens != "CloseBrace"){
        b.push_back(stmt());
    }
    consume("CloseBrace");
    
    return b;
}

// assign_or_call ::= lval gets_or_args
//   gets_or_args ::= `=` rhs | `(` args? `)`
//            rhs ::= exp | `new` type exp?

Stmt* assign_or_call(){
    if(DEBUG_MODE) cout << "assign_or_call: " << *tokens << endl;
    Stmt* s = new Stmt;
    
    Lval* l = lval();

    // Here we assign the right hand sign
    if(*tokens == "Gets"){
        s->type = Stmt::Assign;
        consume("Gets");
        s->value.Assign.lhs = l;
        s->value.Assign.rhs = rhs();
    }
    //Here we call a function
    else if(*tokens == "OpenParen"){
        s->type = Stmt::Call;
        consume("OpenParen");
        s->value.Call.callee = l;
        if(*tokens != "CloseParen"){
            s->value.Call.args = args();
        }
        consume("CloseParen");
    }
    else{
        token_index++;
        throw runtime_error{"Expected gets_or_args"};
    }
    return s;
}


// rhs ::= exp 
//         | `new` type exp?
Rhs* rhs(){
    if(DEBUG_MODE) cout << "rhs: " << *tokens << endl;
    Rhs* r = new Rhs;

    if (*tokens == "New"){
        r->type = Rhs::New;
        consume("New");
        r->value.New.type = type();
        if(*tokens != "Semicolon")
            r->value.New.amount = exp();
        else{
            Exp* e = new Exp;
            e->type = Exp::Num;
            e->value.Num.n = 1;
            r->value.New.amount = e;
        }
    } 
    // exp
    else {
        r->type = Rhs::RhsExp;
        r->value.RhsExp.exp = exp();
    }

    return r;
}


//   lval ::= `*`* id access*

Lval* lval(){
    if(DEBUG_MODE) cout << "lval: " << *tokens << endl;
    if(*tokens == "Star"){
        Lval* l = new Lval;
        consume("Star");
        l->type = Lval::Deref;
        l->value.Deref.lval = lval();
        return l;
    }
    string id = consume_id();
    Lval* l = new Lval;
    l->type = Lval::Id;
    l->value.Id.name = id;
    while(*tokens == "OpenBracket" || *tokens == "Dot"){
        l = access(l);
    }
    return l;
}


// access ::= `[` exp `]` | `.` id
Lval* access(Lval* l_temp){
    if(DEBUG_MODE) cout << "access: " << *tokens << endl;
    //Last step of the recursion where we pass up the access
    Lval* l = new Lval;
    //Array access like arr[0][2]
    if(*tokens == "OpenBracket"){
        consume("OpenBracket");
        l->type = Lval::ArrayAccess;
        l->value.ArrayAccess.index = exp();
        l->value.ArrayAccess.ptr = l_temp;
        consume("CloseBracket");
    }
    //Field access like arr.id
    else if(*tokens == "Dot"){
        consume("Dot");
        l->type = Lval::FieldAccess;
        string id = consume_id();
        l->value.FieldAccess.field = id;
        l->value.FieldAccess.ptr = l_temp;
    }
    return l;
}


// args ::= exp (`,` exp)*
vector<Exp*> args(){
    if(DEBUG_MODE) cout << "args: " << *tokens << endl;
    vector<Exp*> b;

    b.push_back(exp());

    while(*tokens == "Comma"){
        consume("Comma");
        b.push_back(exp());
    }
    return b;
}

/*
# expression; all binary operators and exp_ac are left-associative, all unary
# operators are right-associative. exp_ac binds tighter than `*`, e.g.,
# `*id[2]` means `*(id[2])`; to get `(*id)[2]` we need to write `id[0][2]`.
   exp ::= exp_p4 (binop_p3 exp_p4)*
*/

// exp ::= exp_p4 (binop_p3 exp_p4)*
Exp* exp(){
    if(DEBUG_MODE) cout << "exp: " << *tokens << endl;
    Exp* e = exp_p4();
    while (*tokens == "Equal"
        || *tokens == "NotEq"
        || *tokens == "Lt"
        || *tokens == "Lte"
        || *tokens == "Gt"
        || *tokens == "Gte"
    ){
        Exp* temp = new Exp;
        temp->type = Exp::BinOp;
        temp->value.BinOp.left = e;
        temp->value.BinOp.op = binop_p3();
        temp->value.BinOp.right = exp_p4();
        e = temp;
    }
    return e;
}


// exp_p4 ::= exp_p3 (binop_p2 exp_p3)*

Exp* exp_p4(){
    if(DEBUG_MODE) cout << "exp_p4: " << *tokens << endl;
    Exp* e = exp_p3();
    while (*tokens == "Plus" || *tokens=="Dash"){
        Exp* temp = new Exp;
        temp->type = Exp::BinOp;
        temp->value.BinOp.left = e;
        temp->value.BinOp.op = binop_p2();
        temp->value.BinOp.right = exp_p3();
        e = temp;
    }
    return e;
}

// exp_p3 ::= exp_p2 (binop_p1 exp_p2)*

Exp* exp_p3(){
    if(DEBUG_MODE) cout << "exp_p3: " << *tokens << endl;
    Exp* e = exp_p2();
    while(*tokens == "Star" || *tokens == "Slash"){
        Exp* temp = new Exp;
        temp->type = Exp::BinOp;
        temp->value.BinOp.left = e;
        temp->value.BinOp.op = binop_p1();
        temp->value.BinOp.right = exp_p2();
        e = temp;
    }
    return e;
}


// exp_p2 ::= unop* exp_p1
Exp* exp_p2(){
    if(DEBUG_MODE) cout << "exp_p2: " << *tokens << endl;
    Exp* e = new Exp;
    if(*tokens=="Star" || *tokens=="Dash"){
        e = new Exp;
        e->type = Exp::UnOp;
        e->value.UnOp.op = unop();
        e->value.UnOp.operand = exp_p2();
        return e;
    }
    e = exp_p1();
    return e;
}


// exp_p1 ::= num | `nil` | `(` exp `)` | id exp_ac*
Exp* exp_p1(){
    if(DEBUG_MODE) cout << "exp_p1: " << *tokens << endl;
    Exp* e;
    if(*tokens == "Nil"){
        consume("Nil");
        e = new Exp;
        e->type = Exp::Nil;
    }
    else if(*tokens == "OpenParen"){
        consume("OpenParen");
        e = exp();
        consume("CloseParen");
    }
    else if(tokens->substr(0, 4) == "Num("){
        int32_t num = consume_num();
        e = new Exp;
        e->type = Exp::Num;
        e->value.Num.n = num;
    }
    else if(tokens->substr(0, 3) == "Id(" && tokens->substr(tokens->length() - 1, 1) == ")"){
        string id = consume_id();
        e = new Exp;
        e->type = Exp::Id;
        e->value.Id.name = id;

        while(*tokens == "OpenBracket" || *tokens == "Dot" || *tokens == "OpenParen"){
            e = exp_ac(e);
        }   
    }
    else{
        token_index++;
        throw runtime_error{"Expected exp_p1"};
    }
    return e;
}

// exp_ac ::= `[` exp `]` | `.` id | `(` args? `)`
Exp* exp_ac(Exp* e_temp){
    if(DEBUG_MODE) cout << "exp_ac: " << *tokens << endl;
    Exp* e = new Exp;
    if(*tokens == "OpenBracket"){
        consume("OpenBracket");
        e->type = Exp::ArrayAccess;
        e->value.ArrayAccess.index = exp();
        e->value.ArrayAccess.ptr = e_temp;
        consume("CloseBracket");
    } 
    else if(*tokens == "Dot"){
        consume("Dot");
        e->type = Exp::FieldAccess;
        string id = consume_id();
        e->value.FieldAccess.field = id;
        e->value.FieldAccess.ptr = e_temp;
    } 
    else if(*tokens == "OpenParen"){
        consume("OpenParen");
        e->type = Exp::Call;
        e->value.Call.callee = e_temp;
        if(*tokens != "CloseParen"){
            e->value.Call.args = args();
        }
        consume("CloseParen");
    }
    else { 
        token_index++;
        throw runtime_error{"Expected exp_ac"};
    }
    return e;
}

// unop ::= `*` | `-`
UnaryOp* unop(){
    if(DEBUG_MODE) cout << "unop: " << *tokens << endl;
    UnaryOp* u = new UnaryOp;
    if (*tokens=="Star"){
        u->type = UnaryOp::Deref;   
        consume("Star");
    } 
    else if (*tokens == "Dash"){ 
        u->type = UnaryOp::Neg;
        consume("Dash");
    }
    else {
        token_index++;
        throw runtime_error{"Expected unop"};
    }
    return u;
}

// # binary operators (from highest to lowest precedence).
// binop_p1 ::= `*` | `/`
// binop_p2 ::= `+` | `-`
// binop_p3 ::= `==` | `!=` | `<` | `<=` | `>` | `>=`

// binop_p1 ::= `*` | `/`
BinaryOp* binop_p1 (){
    if(DEBUG_MODE) cout << "binop_p1: " << *tokens << endl;
    BinaryOp* b1 = new BinaryOp;
    // `*`
    if (*tokens == "Star"){ 
        b1->type = BinaryOp::Mul;
        consume("Star");
    } 
    // `/`
    else if(*tokens == "Slash"){ 
        b1->type = BinaryOp::Div;
        consume("Slash");
    }
    else {
        token_index++;
        throw runtime_error{"Expected binop_p1"};
    }
    return b1;
}

// binop_p2 ::= `+` | `-`
BinaryOp* binop_p2(){
    if(DEBUG_MODE) cout << "binop_p2: " << *tokens << endl;
    BinaryOp* b2 = new BinaryOp;
    // `+`
    if (*tokens=="Plus"){ 
        b2->type = BinaryOp::Add;
        consume("Plus");
    } 
    // `-`
    else if (*tokens == "Dash"){ 
        b2->type = BinaryOp::Sub;
        consume("Dash");
    }
    else {
        token_index++;
        throw runtime_error{"Expected binop_p2"};
    }
    return b2;
}

// binop_p3 ::= `==` | `!=` | `<` | `<=` | `>` | `>=`
BinaryOp* binop_p3(){
    if(DEBUG_MODE) cout << "binop_p3: " << *tokens << endl;
    BinaryOp* b3 = new BinaryOp;
    // `==`
    if (*tokens == "Equal"){ 
        b3->type = BinaryOp::Equal;
        consume("Equal");
    }
    // `!=`
    else if (*tokens == "NotEq"){ 
        b3->type = BinaryOp::NotEq;
        consume("NotEq");
    } 
    // `<`
    else if (*tokens == "Lt"){ 
        b3->type = BinaryOp::Lt;
        consume("Lt");
    } 
    // `<=`
    else if (*tokens == "Lte"){ 
        b3->type = BinaryOp::Lte;
        consume("Lte");
    } 
    // `>`
    else if (*tokens == "Gt"){ 
        b3->type = BinaryOp::Gt;
        consume("Gt");
    } 
    // `>=`
    else if (*tokens == "Gte"){ 
        b3->type = BinaryOp::Gte;
        consume("Gte");
    } 
    else {
        token_index++;
        throw runtime_error{"Expected binop_p3"};
    }
    return b3;
}

/**
 * Try to consume a character.
 *
 * Throw a runtime error if the expected charater does not match
 * the consumed character.
 *
 * Returns input + 1.
 */
void consume(string expected) {
    token_index++;
    if(DEBUG_MODE) cout << token_index << ": consume: " << expected << ", token: " << *tokens;
    if(*tokens != expected){
        throw runtime_error{"consume Expected " + expected + " got " + *tokens};
    }
    tokens++;
    if(DEBUG_MODE) cout << ", next token: " << *tokens << endl;
    return;
}

string consume_id(){ // tokens = reference to tokens array
    token_index++;
    if(DEBUG_MODE) cout << token_index << ": consume_id: " << *tokens << endl;
    // Handles the case where user inputted wrong id format in token
    if(tokens->substr(0,3) != "Id("|| tokens->substr(tokens->length() - 1, 1) != ")"){
        throw runtime_error{"Expected ID got " + *tokens};
    }
    string id_name = tokens->substr(3, tokens->length() - 4);
    if(DEBUG_MODE) cout << "id_name: " << id_name << endl;
    tokens++;
    return id_name;
}

int32_t consume_num(){ // tokens = reference to tokens array
    token_index++;
    if(DEBUG_MODE) cout << token_index << ": consume_num: " << *tokens << endl;
    // Handles the case where user inputted wrong num format in token
    if(tokens->substr(0,4) != "Num(" || tokens->substr(tokens->length() - 1, 1) != ")"){
        throw runtime_error{"Expected Num got " + *tokens};
    }
    string num_name = tokens->substr(4, tokens->length() - 5);
    tokens++;
    return (int32_t)stoi(num_name);
}