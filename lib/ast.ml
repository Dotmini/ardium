type struct_decl = {
  name: string;
  fields: (string * string) list; (* name * type *)
}

(* Recursive Types: ต้องผูก expr, stmt, func_data ไว้ด้วยกัน *)
type expr =
  | Int of int
  | Float of float
  | String of string
  | Var of string
  | Global
  | MemberAccess of expr * string         (* obj.prop *)
  | BinOp of expr * string * expr
  | Call of string * expr list
  | Tuple of expr list
  | Named of string * expr                (* arg=value *)
  | Async of expr
  | Await of expr
  | MemberCall of expr * string * expr list
  | Lambda of string list * stmt list
  | StructInit of string * (string * expr) list (* NEW: User { name: "Dot" } *)
  | Index of expr * expr                  (* NEW: arr[0] *)

and stmt =
  | Let of string * string option * expr * string list (* NEW: name, type?, expr, decorators *)
  | Assign of expr * expr
  | GlobalDecl of string * expr list
  | Decorator of string * stmt list
  | ClassDecl of string * expr option * stmt list * string list
  | While of expr * stmt list
  | If of expr * stmt list * stmt list
  | Reset
  | Err of string option
  | Return of expr
  | Expr of expr
  | GlobalMap of expr * string
  | Spawn of stmt list
  | VClass of stmt list
  | HClass of stmt list
  | ZClass of stmt list
  | FuncDef of func_data

and func_data = {
  name: string;
  args: (string * string option) list; (* NEW: รองรับ (x: int) *)
  body: stmt list;
  is_export: bool;
  is_test: bool;
  is_interrupt: bool;
  decorators: string list
}

and top_level =
  | Func of func_data
  | Import of string
  | Extern of string * (string * string) list * string * bool * string list (* name, args, ret_type, is_variadic, decorators *)
  | GlobalDef of string * expr list
  | GlobalVar of string * string option * expr * string list (* NEW: type option *)
  | TopLevelStmt of stmt
  | ClassDef of string * expr option * stmt list * string list
  | Struct of struct_decl
  | Enum of string * (string * int option) list
  | GlobalHandler of string * stmt list

type prog = top_level list