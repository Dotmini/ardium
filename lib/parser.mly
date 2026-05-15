%{
  open Ast
%}

%token <int> INT
%token <float> FLOAT
%token <string> STRING ID
%token FN LET VAR IF ELIF ELSE LOOP RETURN EXTERN PRINT EXPORT TEST IMPORT INTERRUPT
%token ASYNC AWAIT SPAWN VCLASS HCLASS ZCLASS
%token GLOBAL_KEY RESET ERR CONT CON CLASS STRUCT ENUM YED DECORATOR AS MUT
%token LPAREN RPAREN LBRACE RBRACE LBRACKET RBRACKET SEMI COMMA EQUAL BANG
%token PLUS MINUS TIMES DIV POW MOD EQ_EQ NOT_EQ LESS GREATER LE GE
%token DOT COLON ARROW AT DOTDOTDOT EOF NEWLINE

%left EQ_EQ NOT_EQ LESS GREATER LE GE
%left PLUS MINUS
%left TIMES DIV MOD
%right POW
%nonassoc UMINUS

%start <Ast.prog> prog
%%

prog:
  | items = top_levels; EOF { items }

top_levels:
  | /* empty */ { [] }
  | x = top_level; rest = top_levels { x :: rest }
  | NEWLINE; rest = top_levels { rest } (* Skip empty lines at top level *)

top_level:
  | attrs = decorators; FN; name = ID; LPAREN; args = func_args_def; RPAREN; option(COLON); LBRACE; body = stmts; RBRACE
    { Func({ name=name; args=args; body=body; is_export=List.mem "export" attrs; is_test=List.mem "test" attrs; is_interrupt=List.mem "interrupt" attrs; decorators=attrs }) }
  | attrs = decorators; CLASS; name = ID; p = option(parent_decl); option(COLON); LBRACE; body = stmts; RBRACE
    { ClassDef(name, p, body, attrs) }
  | STRUCT; name = ID; option(COLON); LBRACE; opt_newline; fields = struct_field_list; RBRACE
    { Struct({ name = name; fields = fields }) }
  | IMPORT; name = ID { Import(name) }
  | IMPORT; s = STRING { Import(s) }
  | ENUM; name = ID; LBRACE; variants = enum_variant_list; RBRACE
    { Enum(name, variants) }
  | GLOBAL_KEY; LPAREN; id = ID; RPAREN; EQUAL; LPAREN; el = separated_list(COMMA, global_field); RPAREN; eos
    { GlobalDef(id, el) }
  | attrs = decorators; VAR; name = ID; ty = option(type_ann); EQUAL; e = expr; eos { GlobalVar(name, ty, e, attrs) }
  | attrs = decorators; LET; option(MUT); name = ID; ty = option(type_ann); EQUAL; e = expr; eos { GlobalVar(name, ty, e, attrs) }
  (* | AT; GLOBAL_KEY; LPAREN; id = ID; RPAREN; LBRACE; body = stmts; RBRACE
    { GlobalHandler(id, body) } *)
  (* | s = stmt_base { TopLevelStmt(s) } *)
  | attrs = decorators; e = extern_decl { e attrs }

stmt:
  | attrs = decorators; FN; name = ID; LPAREN; args = func_args_def; RPAREN; option(COLON); LBRACE; body = stmts; RBRACE
    { FuncDef({ name=name; args=args; body=body; is_export=List.mem "export" attrs; is_test=List.mem "test" attrs; is_interrupt=List.mem "interrupt" attrs; decorators=attrs }) }
  | attrs = decorators; VAR; name = ID; ty = option(type_ann); EQUAL; e = expr; eos { Let(name, ty, e, attrs) }
  | attrs = decorators; LET; option(MUT); name = ID; ty = option(type_ann); EQUAL; e = expr; eos { Let(name, ty, e, attrs) }
  | SPAWN; LBRACE; body = stmts; RBRACE { Spawn(body) }
  | VCLASS; LBRACE; body = stmts; RBRACE { VClass(body) }
  | HCLASS; LBRACE; body = stmts; RBRACE { HClass(body) }
  | ZCLASS; LBRACE; body = stmts; RBRACE { ZClass(body) }
  | e = expr_basic; eos { Expr(e) }
  | s = stmt_base { s }

stmt_base:
  | e1 = expr_basic; EQUAL; e2 = expr; eos { Assign(e1, e2) }
  | IF; cond = expr; option(COLON); LBRACE; ts = stmts; RBRACE; es = option(if_tail)
      { let es_body = match es with Some b -> b | None -> [] in If(cond, ts, es_body) }
  | IF; LPAREN; cond = expr; RPAREN; option(COLON); LBRACE; ts = stmts; RBRACE; es = option(if_tail)
      { let es_body = match es with Some b -> b | None -> [] in If(cond, ts, es_body) }
  | LOOP; cond = expr; option(COLON); LBRACE; b = stmts; RBRACE { While(cond, b) }
  | LOOP; LPAREN; cond = expr; RPAREN; option(COLON); LBRACE; b = stmts; RBRACE { While(cond, b) }
  | RETURN; e = expr; eos { Return(e) }
  | GLOBAL_KEY; LPAREN; e = expr; RPAREN; action = global_action; eos { action e }
  | RESET; LPAREN; RPAREN; eos { Reset }
  | ERR; LPAREN; s = STRING; RPAREN; eos { Err(Some s) }
  | ERR; LPAREN; RPAREN; eos { Err(None) }

if_tail:
  | ELSE; option(COLON); LBRACE; body = stmts; RBRACE { body }
  | ELIF; cond = expr; option(COLON); LBRACE; ts = stmts; RBRACE; es = option(if_tail)
    { let es_body = match es with Some b -> b | None -> [] in [If(cond, ts, es_body)] }

global_action:
  | AS; name = ID { fun e -> GlobalMap(e, name) }
  | EQUAL; LPAREN; el = separated_list(COMMA, expr); RPAREN { fun e ->
      match e with
      | Var id -> GlobalDecl(id, el)
      | _ -> failwith "GLOBAL define requires an ID" }

expr:
  | e = expr_basic { e }
  | FN; LPAREN; args = separated_list(COMMA, ID); RPAREN; LBRACE; body = stmts; RBRACE { Lambda(args, body) }

expr_basic:
  | MINUS; e = expr %prec UMINUS { BinOp(Int(0), "-", e) }
  | i = INT { Int(i) }
  | f = FLOAT { Float(f) }
  | s = STRING { String(s) }
  | x = ID  { Var(x) }
  | GLOBAL_KEY { Global }
  | ASYNC; e = expr { Async(e) }
  | AWAIT; e = expr { Await(e) }
  | e = expr; DOT; m = ID; LPAREN; el = separated_list(COMMA, expr); RPAREN { MemberCall(e, m, el) }
  | e1 = expr; PLUS; e2 = expr { BinOp(e1, "+", e2) }
  | e1 = expr; MINUS; e2 = expr { BinOp(e1, "-", e2) }
  | e1 = expr; TIMES; e2 = expr { BinOp(e1, "*", e2) }
  | e1 = expr; DIV; e2 = expr   { BinOp(e1, "/", e2) }
  | e1 = expr; MOD; e2 = expr   { BinOp(e1, "%", e2) }
  | e1 = expr; EQ_EQ; e2 = expr { BinOp(e1, "==", e2) }
  | e1 = expr; NOT_EQ; e2 = expr { BinOp(e1, "!=", e2) }
  | e1 = expr; LESS; e2 = expr  { BinOp(e1, "<", e2) }
  | e1 = expr; GREATER; e2 = expr { BinOp(e1, ">", e2) }
  | e1 = expr; LE; e2 = expr    { BinOp(e1, "<=", e2) }
  | e1 = expr; GE; e2 = expr    { BinOp(e1, ">=", e2) }
  | BANG; e = expr { Call("not", [e]) }
  | LPAREN; e = expr; RPAREN { e }
  | fname = ID; LPAREN; args = separated_list(COMMA, expr); RPAREN { Call(fname, args) }
  | e = expr_basic; LBRACKET; idx = expr; RBRACKET { Index(e, idx) }
  | name = ID; LBRACE; fields = separated_list(COMMA, struct_init_field); RBRACE { StructInit(name, fields) }
  | e1 = expr; DOT; member = ID { MemberAccess(e1, member) }

func_args_def:
  | args = separated_list(COMMA, func_arg) { args }

func_arg:
  | name = ID; ty = option(type_ann) { (name, ty) }

type_ann:
  | COLON; ty = ID { ty }

struct_init_field:
  | name = ID; COLON; e = expr { (name, e) }

enum_variant_list:
  | /* empty */ { [] }
  | NEWLINE; rest = enum_variant_list { rest }
  | v = enum_variant; COMMA; rest = enum_variant_list { v :: rest }
  | v = enum_variant; opt_newline; { [v] }

struct_field_list:
  | /* empty */ { [] }
  | f = struct_field; opt_newline { [f] }
  | f = struct_field; COMMA; opt_newline; rest = struct_field_list { f :: rest }
  | f = struct_field; opt_newline; rest = struct_field_list { f :: rest }

opt_newline:
  | /* empty */ { () }
  | NEWLINE { () }

enum_variant:
  | name = ID { (name, None) }
  | name = ID; EQUAL; i = INT { (name, Some i) }

global_field:
  | e = expr { e }
  | id = ID; EQUAL; e = expr { Named(id, e) }

lident_list:
  | { ([], false) }
  | DOTDOTDOT { ([], true) }
  | id = ID { ([id], false) }
  | id = ID; COMMA; rest = lident_list { let ids, is_var = rest in (id :: ids, is_var) }

stmts:
  | /* empty */ { [] }
  | s = stmt; rest = stmts { s :: rest }
  | NEWLINE; rest = stmts { rest }

eos:
  | SEMI { () }
  | NEWLINE { () }

decorators:
  | /* empty */ { [] }
  | AT; d = decorator_attr_body; opt_newline; rest = decorators { d :: rest }

decorator_attr_body:
  | EXPORT { "export" }
  | TEST { "test" }
  | INTERRUPT { "interrupt" }
  | GLOBAL_KEY; LPAREN; separated_list(COMMA, expr); RPAREN { "global" }
  | id = ID { id }
  | id = ID; LPAREN; separated_list(COMMA, expr); RPAREN { id }

parent_decl:
  | LPAREN; p = expr; RPAREN { p }

struct_field:
  | name = ID; COLON; ty = ID { (name, ty) }

extern_decl:
  | EXTERN; FN; name = ID; LPAREN; args = extern_arg_list; RPAREN; ret = extern_ret_opt; eos
    { fun attrs -> 
      let arg_list, is_var = args in
      Extern(name, arg_list, ret, is_var, attrs) 
    }

extern_arg_list:
  | /* empty */ { ([], false) }
  | DOTDOTDOT   { ([], true) }
  | args = extern_args_core { (args, false) }
  | args = extern_args_core; COMMA; DOTDOTDOT { (args, true) }

extern_args_core:
  | arg = extern_arg_item { [arg] }
  | arg = extern_arg_item; COMMA; rest = extern_args_core { arg :: rest }


extern_arg_item:
  | name = ID { (name, "void*") } /* Default type if missing */
  | name = ID; COLON; ty = ID { (name, ty) }

extern_ret_opt:
  | /* empty */ { "void" }
  | ARROW; ty = ID { ty }
