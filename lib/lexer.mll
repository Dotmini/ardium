{
  open Parser
}

let white = [' ' '\t']+
let newline = '\r' | '\n' | "\r\n"
let id = ['a'-'z' 'A'-'Z' '_'] ['a'-'z' 'A'-'Z' '0'-'9' '_']*
let digit = ['0'-'9']
let float_num = digit+ '.' digit+

rule read = parse
  | white          { read lexbuf }
  | newline        { Lexing.new_line lexbuf; NEWLINE }
  | "//" [^ '\n']* { read lexbuf }

  (* --- Keywords (ต้องอยู่ก่อน ID เสมอ) --- *)
  | "import" { IMPORT }
  | "fn"     { FN }
  | "func"   { FN }
  | "var"    { VAR }
  | "let"    { LET }
  | "async"  { ASYNC }
  | "await"  { AWAIT }
  | "mut"    { MUT }
  | "if"     { IF }
  | "elif"   { ELIF }
  | "else"   { ELSE }
  | "while"  { LOOP }
  | "return" { RETURN }
  | "test"   { TEST }
  | "extern" { EXTERN }
  | "GLOBAL" { GLOBAL_KEY }
  | "RESET"  { RESET }
  | "ERR"    { ERR }
  | "CONT"   { CONT }
  | "CON"    { CON }
  | "class"  { CLASS }
  | "struct" { STRUCT }
  | "enum"   { ENUM }
  | "YED"    { YED }
  | "as"     { AS }
  | "dev"    { DECORATOR }
  | "export" { EXPORT }
  | "spawn"  { SPAWN }
  | "VClass" { VCLASS }
  | "HClass" { HCLASS }
  | "ZClass" { ZCLASS }
  | "interrupt" { INTERRUPT }
  | "@" { AT }

  (* --- Symbols & Operators --- *)
  | "("      { LPAREN }
  | ")"      { RPAREN }
  | "{"      { LBRACE }
  | "}"      { RBRACE }
  | "["      { LBRACKET }
  | "]"      { RBRACKET }
  | ";"      { SEMI }
  | ","      { COMMA }
  | "="      { EQUAL }
  | "=="     { EQ_EQ }
  | "!="     { NOT_EQ }
  | "!"      { BANG }
  | "<"      { LESS }
  | ">"      { GREATER }
  | "<="     { LE }
  | ">="     { GE }
  | "+"      { PLUS }
  | "-"      { MINUS }
  | "*"      { TIMES }
  | "/"      { DIV }
  | "%"      { MOD }
  | "**"     { POW }
  | "."      { DOT }
  | ":"      { COLON }
  | "->"     { ARROW }
  | "@"      { AT }
  | "..."    { DOTDOTDOT }

  (* --- Literals --- *)
  | float_num as f { FLOAT (float_of_string f) }
  | "0x" ['0'-'9' 'a'-'f' 'A'-'F']+ as h { INT (int_of_string h) }
  | digit+ as i    { INT (int_of_string i) }
  | '"'            { read_string (Buffer.create 17) lexbuf }

  (* --- Identifier --- *)
  | id as s  {
      match s with
      | _ -> ID s
    }

  | eof      { EOF }
  | _        { failwith (Printf.sprintf "Unexpected char: %c" (Lexing.lexeme_char lexbuf 0)) }

and read_string buf = parse
  | '"'       { STRING (Buffer.contents buf) }
  | '\\' '/'  { Buffer.add_char buf '/'; read_string buf lexbuf }
  | '\\' '\\' { Buffer.add_char buf '\\'; read_string buf lexbuf }
  | '\\' 'n'  { Buffer.add_char buf '\n'; read_string buf lexbuf }
  | '\\' 't'  { Buffer.add_char buf '\t'; read_string buf lexbuf }
  | [^ '"' '\\']+ { Buffer.add_string buf (Lexing.lexeme lexbuf); read_string buf lexbuf }
  | _         { failwith "Illegal string character" }
  | eof       { failwith "String is not terminated" }
