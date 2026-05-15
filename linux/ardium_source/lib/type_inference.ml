(* lib/type_inference.ml *)
(* Static type inference system for Ardium *)

open Ast

type inferred_type =
  | TInt
  | TFloat
  | TString
  | TPointer
  | TBool
  | TVoid
  | TUnknown

type type_env = (string, inferred_type) Hashtbl.t

let create_env () : type_env = Hashtbl.create 32

let type_to_string = function
  | TInt -> "int"
  | TFloat -> "float"
  | TString -> "string"
  | TPointer -> "pointer"
  | TBool -> "bool"
  | TVoid -> "void"
  | TUnknown -> "unknown"

(* Unify two types - choose the more specific one *)
let unify t1 t2 =
  match t1, t2 with
  | TUnknown, t | t, TUnknown -> t
  | TInt, TFloat | TFloat, TInt -> TFloat  (* Int + Float = Float *)
  | t1, t2 when t1 = t2 -> t1
  | _ -> TUnknown  (* Type conflict, fall back to unknown *)

(* Infer type from expression *)
let rec infer_expr (env: type_env) (expr: expr) : inferred_type =
  match expr with
  | Int _ -> TInt
  | Float _ -> TFloat
  | String _ -> TString
  | Global -> TPointer
  
  | Var name ->
      (match Hashtbl.find_opt env name with
       | Some ty -> ty
       | None -> TUnknown)
  
  | MemberAccess (_, _) -> TPointer
  
  | BinOp (left, op, right) ->
      let lt = infer_expr env left in
      let rt = infer_expr env right in
      (match op with
       | "+" | "-" | "*" | "/" ->
           (* Arithmetic: if either is float, result is float *)
           if lt = TFloat || rt = TFloat then TFloat
           else if lt = TInt && rt = TInt then TInt
           else TUnknown
       | "==" | "!=" | "<" | ">" ->
           (* Comparison: always returns bool *)
           TBool
       | _ -> TUnknown)
  
  | Call (fname, _args) ->
      (* Infer return type based on function name *)
      (match fname with
       | "malloc" -> TPointer
       | "peek" -> TInt
       | "readcyclecounter" -> TInt
       | "sin" | "cos" | "sqrt" | "pow" -> TFloat
       | "not" -> TBool
       | "printf" | "print" | "println" -> TInt
       | _ -> TUnknown)
  
  | Tuple _ -> TPointer
  | Named (_, e) -> infer_expr env e
  | Async e -> infer_expr env e (* Future: TPromise *)
  | Await e -> infer_expr env e (* Future: Unwrap TPromise *)
  | MemberCall (_, _, _) -> TUnknown
  | Lambda (_, _) -> TPointer
  | Index (_, _) -> TUnknown
  | StructInit (_, _) -> TUnknown

(* Infer types from statement and update environment *)
let rec infer_stmt (env: type_env) (stmt: stmt) : unit =
  match stmt with
  | Let (name, _, expr, _) ->
      let ty = infer_expr env expr in
      Hashtbl.replace env name ty
  
  | Assign (target, expr) ->
      let expr_ty = infer_expr env expr in
      (match target with
       | Var name ->
           (* Update variable type if it becomes more specific *)
           let current_ty = Hashtbl.find_opt env name |> Option.value ~default:TUnknown in
           let unified = unify current_ty expr_ty in
           Hashtbl.replace env name unified
       | _ -> ())
  
  | If (cond, then_stmts, else_stmts) ->
      ignore (infer_expr env cond);
      List.iter (infer_stmt env) then_stmts;
      List.iter (infer_stmt env) else_stmts
  
  | While (cond, body) ->
      ignore (infer_expr env cond);
      (* Iterate multiple times to handle loop-carried dependencies *)
      for _ = 1 to 3 do
        List.iter (infer_stmt env) body
      done
  
  | Return expr ->
      ignore (infer_expr env expr)
  
  | Expr expr ->
      ignore (infer_expr env expr)
  
  | GlobalDecl (_, exprs) ->
      List.iter (fun e -> ignore (infer_expr env e)) exprs
  
  | Decorator (_, stmts) ->
      List.iter (infer_stmt env) stmts
  
  | GlobalMap (expr, _) ->
      ignore (infer_expr env expr)
  
  | FuncDef { body; _ } ->
      List.iter (infer_stmt env) body
  | Spawn body | VClass body | HClass body | ZClass body ->
      List.iter (infer_stmt env) body
  | Reset | Err _ | ClassDecl _ -> ()

(* Infer types for a function *)
let infer_function (func: top_level) : type_env =
  match func with
  | Func { args; body; _ } ->
      let env = create_env () in
      (* Initialize arguments as unknown (will be inferred from usage) *)
      List.iter (fun (arg, _) -> Hashtbl.add env arg TUnknown) args;
      (* Infer from body *)
      List.iter (infer_stmt env) body;
      env
  | _ -> create_env ()

(* Infer types for entire program *)
let infer_program (prog: prog) : type_env =
  let global_env = create_env () in
  
  (* First pass: collect global variables *)
  List.iter (function
    | GlobalVar (name, _, expr, _) ->
        let ty = infer_expr global_env expr in
        Hashtbl.replace global_env name ty
    | GlobalDef (name, _) ->
        Hashtbl.replace global_env name TPointer
    | _ -> ()
  ) prog;
  
  (* Second pass: infer function bodies *)
  List.iter (function
    | Func { body; _ } ->
        List.iter (infer_stmt global_env) body
    | TopLevelStmt stmt ->
        infer_stmt global_env stmt
    | _ -> ()
  ) prog;
  
  global_env

(* Debug: print type environment *)
let print_env (env: type_env) : unit =
  Printf.printf "Type Environment:\n";
  Hashtbl.iter (fun name ty ->
    Printf.printf "  %s: %s\n" name (type_to_string ty)
  ) env
