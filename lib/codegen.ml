open Llvm
open Ast

(* --- Error Handling --- *)
exception Codegen_error of string * int * int

let error ?(line=0) ?(col=0) msg = raise (Codegen_error (msg, line, col))

let error_msg msg line col =
  Printf.sprintf "%s (at line %d, column %d)" msg line col

(* --- Loop Context for Break/Continue --- *)
type loop_context = {
  break_bb : llbasicblock option;
  continue_bb : llbasicblock option;
}

let empty_loop_ctx = { break_bb = None; continue_bb = None }

(* --- Context and Module Management --- *)
module Context = struct
  type t = {
    llvm_ctx : llcontext;
    the_module : llmodule;
    builder : llbuilder;
    named_values : (string, llvalue * lltype) Hashtbl.t;
    global_values : (string, llvalue * lltype) Hashtbl.t;
    structs : (string, Ast.struct_decl) Hashtbl.t;
    mutable current_loop : loop_context;
    mutable global_ptr : llvalue option;
    mutable global_struct : Ast.struct_decl option;
    mutable line : int;
    mutable col : int;
  }

  let create module_name =
    let llvm_ctx = global_context () in
    {
      llvm_ctx;
      the_module = create_module llvm_ctx module_name;
      builder = builder llvm_ctx;
      named_values = Hashtbl.create 50;
      global_values = Hashtbl.create 50;
      structs = Hashtbl.create 20;
      current_loop = empty_loop_ctx;
      global_ptr = None;
      global_struct = None;
      line = 0;
      col = 0;
    }

  let clear_named_values ctx =
    Hashtbl.clear ctx.named_values

  let add_named_value ctx name value ty =
    Hashtbl.replace ctx.named_values name (value, ty)

  let find_named_value ctx name =
    match Hashtbl.find_opt ctx.named_values name with
    | Some v -> Some v
    | None -> Hashtbl.find_opt ctx.global_values name

  let add_global_value ctx name value ty =
    Hashtbl.replace ctx.global_values name (value, ty)

  let add_struct ctx name decl =
    Hashtbl.replace ctx.structs name decl

  let find_struct ctx name =
    Hashtbl.find_opt ctx.structs name

  let set_loop_context ctx loop_ctx =
    ctx.current_loop <- loop_ctx

  let get_loop_context ctx = ctx.current_loop

  let set_global_map ctx ptr struct_decl =
    ctx.global_ptr <- Some ptr;
    ctx.global_struct <- Some struct_decl

  let get_global_map ctx = (ctx.global_ptr, ctx.global_struct)

  let set_location ctx line col =
    ctx.line <- line;
    ctx.col <- col
end

(* --- Type System --- *)
module Types = struct
  let i32_type ctx = i32_type ctx.Context.llvm_ctx
  let i64_type ctx = i64_type ctx.Context.llvm_ctx
  let double_type ctx = double_type ctx.Context.llvm_ctx
  let i8_type ctx = i8_type ctx.Context.llvm_ctx
  let i8_ptr_type ctx = pointer_type ctx.Context.llvm_ctx
  let void_type ctx = void_type ctx.Context.llvm_ctx
  let bool_type ctx = i1_type ctx.Context.llvm_ctx
  let void_ptr_type ctx = pointer_type ctx.Context.llvm_ctx

  let float_type ctx = float_type ctx.Context.llvm_ctx

  let is_numeric_type ty ctx =
    ty = i32_type ctx || ty = i64_type ctx || ty = double_type ctx || ty = float_type ctx

  let is_float_type ty ctx =
    ty = double_type ctx || ty = float_type ctx

  let is_int_type ty ctx =
    ty = i32_type ctx || ty = i64_type ctx

  let is_pointer_type ty =
    match classify_type ty with
    | TypeKind.Pointer -> true
    | _ -> false

  let type_to_string ty ctx =
    if ty = i32_type ctx then "i32"
    else if ty = i64_type ctx then "i64"
    else if ty = double_type ctx then "double"
    else if ty = float_type ctx then "float"
    else if ty = bool_type ctx then "bool"
    else if ty = void_type ctx then "void"
    else if is_pointer_type ty then "ptr"
    else "unknown"

  let type_from_string ctx type_name =
    match type_name with
    | "i32" | "int" -> i32_type ctx
    | "i64" | "long" -> i64_type ctx
    | "double" -> double_type ctx
    | "float" | "f32" -> float_type ctx
    | "bool" -> bool_type ctx
    | "void" -> void_type ctx
    | "ptr" | "pointer" | "any" -> i8_ptr_type ctx
    | "string" -> i8_ptr_type ctx
    | _ -> i64_type ctx (* Default to i64 for unknown types *)
end

(* --- Built-in Functions --- *)
module Builtins = struct
  let math_functions = ["sin"; "cos"; "tan"; "sqrt"; "pow"; "exp"; "log"; "floor"; "ceil"; "abs"; "fabs"]
  let io_functions = ["print"; "println"; "print_int"; "print_string"; "print_double"]

  let is_math_function name =
    List.mem name math_functions

  let is_io_function name =
    List.mem name io_functions

  let coreui_functions = [
    "agui"; "run_apple_gui"; "viewer"; "background"; "text"; 
    "add_button"; "add_image"; "add_textfield"; "get_input_value"
  ]

  let is_coreui_function name =
    List.mem name coreui_functions

  let get_or_declare_printf ctx =
    let printf_ty = var_arg_function_type (Types.i32_type ctx)
                      [| Types.i8_ptr_type ctx |] in
    match lookup_function "printf" ctx.Context.the_module with
    | Some f -> f
    | None -> declare_function "printf" printf_ty ctx.Context.the_module

  let get_or_declare_scanf ctx =
    let scanf_ty = var_arg_function_type (Types.i32_type ctx)
                      [| Types.i8_ptr_type ctx |] in
    match lookup_function "scanf" ctx.Context.the_module with
    | Some f -> f
    | None -> declare_function "scanf" scanf_ty ctx.Context.the_module

  let get_format_string ctx ty =
    let fmt_str =
      match classify_type ty with
      | TypeKind.Double -> "%.6f\n"
      | TypeKind.Integer -> 
          if ty = Types.i32_type ctx then "%d\n" 
          else if ty = Types.bool_type ctx then "%d\n"
          else "%lld\n"
      | TypeKind.Pointer -> "%s\n"
      | _ -> "%lld\n"
    in
    let str_val = const_stringz ctx.Context.llvm_ctx fmt_str in
    let g = define_global "fmt" str_val ctx.Context.the_module in
    set_global_constant true g;
    set_unnamed_addr true g;
    set_linkage Linkage.Internal g;
    let zero = const_int (Types.i32_type ctx) 0 in
    const_in_bounds_gep (type_of str_val) g [| zero; zero |]

  let get_input_format_string ctx ty =
    let fmt_str =
      if ty = Types.double_type ctx then "%lf"
      else if ty = Types.i32_type ctx then "%d"
      else if ty = Types.i64_type ctx then "%lld"
      else "%s"
    in
    let str_val = const_stringz ctx.Context.llvm_ctx fmt_str in
    let g = define_global "input_fmt" str_val ctx.Context.the_module in
    set_global_constant true g;
    set_unnamed_addr true g;
    set_linkage Linkage.Internal g;
    let zero = const_int (Types.i32_type ctx) 0 in
    const_in_bounds_gep (type_of str_val) g [| zero; zero |]

  (* String concatenation using sprintf *)
  let get_or_declare_sprintf ctx =
    let sprintf_ty = var_arg_function_type (Types.i32_type ctx)
                        [| Types.i8_ptr_type ctx; Types.i8_ptr_type ctx |] in
    match lookup_function "sprintf" ctx.Context.the_module with
    | Some f -> f
    | None -> declare_function "sprintf" sprintf_ty ctx.Context.the_module

  let get_or_declare_strlen ctx =
    let strlen_ty = function_type (Types.i64_type ctx) [| Types.i8_ptr_type ctx |] in
    match lookup_function "strlen" ctx.Context.the_module with
    | Some f -> f
    | None -> declare_function "strlen" strlen_ty ctx.Context.the_module

  let get_or_declare_malloc ctx =
    let malloc_ty = function_type (Types.i8_ptr_type ctx) [| Types.i64_type ctx |] in
    match lookup_function "malloc" ctx.Context.the_module with
    | Some f -> f
    | None -> declare_function "malloc" malloc_ty ctx.Context.the_module

  let get_or_declare_arc_retain ctx =
    let ty = function_type (Types.void_type ctx) [| Types.i8_ptr_type ctx |] in
    match lookup_function "__arc_retain" ctx.Context.the_module with
    | Some f -> f
    | None -> declare_function "__arc_retain" ty ctx.Context.the_module

  let get_or_declare_arc_release ctx =
    let ty = function_type (Types.void_type ctx) [| Types.i8_ptr_type ctx |] in
    match lookup_function "__arc_release" ctx.Context.the_module with
    | Some f -> f
    | None -> declare_function "__arc_release" ty ctx.Context.the_module

  (* --- Error Handling Intrinsics --- *)
  let get_or_declare_sys_throw ctx =
    let ty = function_type (Types.void_type ctx) [| Types.i8_ptr_type ctx |] in
    match lookup_function "__sys_throw" ctx.Context.the_module with
    | Some f -> f
    | None -> declare_function "__sys_throw" ty ctx.Context.the_module

  let get_or_declare_sys_get_error ctx =
    let ty = function_type (Types.i8_ptr_type ctx) [||] in
    match lookup_function "__sys_get_error" ctx.Context.the_module with
    | Some f -> f
    | None -> declare_function "__sys_get_error" ty ctx.Context.the_module

  let get_or_declare_sys_clear_error ctx =
    let ty = function_type (Types.void_type ctx) [||] in
    match lookup_function "__sys_clear_error" ctx.Context.the_module with
    | Some f -> f
    | None -> declare_function "__sys_clear_error" ty ctx.Context.the_module

  (* --- Threading Intrinsics --- *)
  let get_or_declare_pthread_create ctx =
    (* int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg); *)
    let ty = function_type (Types.i32_type ctx) 
        [| Types.i8_ptr_type ctx; Types.i8_ptr_type ctx; Types.i8_ptr_type ctx; Types.i8_ptr_type ctx |] in
    match lookup_function "__sys_pthread_create" ctx.Context.the_module with
    | Some f -> f
    | None -> declare_function "__sys_pthread_create" ty ctx.Context.the_module

  let get_or_declare_pthread_join ctx =
    (* int pthread_join(pthread_t thread, void **retval); *)
    let ty = function_type (Types.i32_type ctx) 
        [| Types.i64_type ctx; Types.i8_ptr_type ctx |] in
    match lookup_function "__sys_pthread_join" ctx.Context.the_module with
    | Some f -> f
    | None -> declare_function "__sys_pthread_join" ty ctx.Context.the_module

  (* --- File I/O Intrinsics --- *)
  let get_or_declare_sys_open ctx =
    let ty = function_type (Types.i32_type ctx) 
        [| Types.i8_ptr_type ctx; Types.i32_type ctx; Types.i32_type ctx |] in
    match lookup_function "__sys_open" ctx.Context.the_module with
    | Some f -> f | None -> declare_function "__sys_open" ty ctx.Context.the_module

  let get_or_declare_sys_close ctx =
    let ty = function_type (Types.i32_type ctx) [| Types.i32_type ctx |] in
    match lookup_function "__sys_close" ctx.Context.the_module with
    | Some f -> f | None -> declare_function "__sys_close" ty ctx.Context.the_module

  let get_or_declare_sys_read ctx =
    let ty = function_type (Types.i64_type ctx) 
        [| Types.i32_type ctx; Types.i8_ptr_type ctx; Types.i64_type ctx |] in
    match lookup_function "__sys_read" ctx.Context.the_module with
    | Some f -> f | None -> declare_function "__sys_read" ty ctx.Context.the_module

  let get_or_declare_sys_write ctx =
    let ty = function_type (Types.i64_type ctx) 
        [| Types.i32_type ctx; Types.i8_ptr_type ctx; Types.i64_type ctx |] in
    match lookup_function "__sys_write" ctx.Context.the_module with
    | Some f -> f | None -> declare_function "__sys_write" ty ctx.Context.the_module

  let get_or_declare_sys_lseek ctx =
    let ty = function_type (Types.i64_type ctx) 
        [| Types.i32_type ctx; Types.i64_type ctx; Types.i32_type ctx |] in
    match lookup_function "__sys_lseek" ctx.Context.the_module with
    | Some f -> f | None -> declare_function "__sys_lseek" ty ctx.Context.the_module

  (* --- Networking Intrinsics --- *)
  let get_or_declare_sys_socket ctx =
    let ty = function_type (Types.i32_type ctx) [||] in
    match lookup_function "__sys_socket" ctx.Context.the_module with
    | Some f -> f | None -> declare_function "__sys_socket" ty ctx.Context.the_module

  let get_or_declare_sys_bind ctx =
    let ty = function_type (Types.i32_type ctx) [| Types.i32_type ctx; Types.i32_type ctx |] in
    match lookup_function "__sys_bind" ctx.Context.the_module with
    | Some f -> f | None -> declare_function "__sys_bind" ty ctx.Context.the_module

  let get_or_declare_sys_listen ctx =
    let ty = function_type (Types.i32_type ctx) [| Types.i32_type ctx; Types.i32_type ctx |] in
    match lookup_function "__sys_listen" ctx.Context.the_module with
    | Some f -> f | None -> declare_function "__sys_listen" ty ctx.Context.the_module

  let get_or_declare_sys_accept ctx =
    let ty = function_type (Types.i32_type ctx) [| Types.i32_type ctx |] in
    match lookup_function "__sys_accept" ctx.Context.the_module with
    | Some f -> f | None -> declare_function "__sys_accept" ty ctx.Context.the_module

  (* --- Database Intrinsics --- *)
  let get_or_declare_sys_sqlite_open ctx =
    let ty = function_type (Types.i64_type ctx) [| Types.i8_ptr_type ctx |] in
    match lookup_function "__sys_sqlite_open" ctx.Context.the_module with
    | Some f -> f | None -> declare_function "__sys_sqlite_open" ty ctx.Context.the_module

  let get_or_declare_sys_sqlite_exec ctx =
    let ty = function_type (Types.i32_type ctx) [| Types.i64_type ctx; Types.i8_ptr_type ctx |] in
    match lookup_function "__sys_sqlite_exec" ctx.Context.the_module with
    | Some f -> f | None -> declare_function "__sys_sqlite_exec" ty ctx.Context.the_module

  let get_or_declare_sys_sqlite_close ctx =
    let ty = function_type (Types.void_type ctx) [| Types.i64_type ctx |] in
    match lookup_function "__sys_sqlite_close" ctx.Context.the_module with
    | Some f -> f | None -> declare_function "__sys_sqlite_close" ty ctx.Context.the_module

  let get_or_declare_strcmp ctx =
    let strcmp_ty = function_type (Types.i32_type ctx)
                      [| Types.i8_ptr_type ctx; Types.i8_ptr_type ctx |] in
    match lookup_function "strcmp" ctx.Context.the_module with
    | Some f -> f
    | None -> declare_function "strcmp" strcmp_ty ctx.Context.the_module

  (* --- CoreUI Intrinsics --- *)
  
  (* agui() -> void *)
  let get_or_declare_agui ctx =
    let ty = function_type (Types.void_type ctx) [||] in
    match lookup_function "CoreUI_Init" ctx.Context.the_module with
    | Some f -> f
    | None -> declare_function "CoreUI_Init" ty ctx.Context.the_module

  (* run_apple_gui() -> void *)
  let get_or_declare_run_gui ctx =
    let ty = function_type (Types.void_type ctx) [||] in
    match lookup_function "CoreUI_Run" ctx.Context.the_module with
    | Some f -> f
    | None -> declare_function "CoreUI_Run" ty ctx.Context.the_module

  (* viewer(title, x, y, w, h) -> void *)
  let get_or_declare_viewer ctx =
    let ty = function_type (Types.void_type ctx) 
      [| Types.i8_ptr_type ctx; Types.i32_type ctx; Types.i32_type ctx; Types.i32_type ctx; Types.i32_type ctx |] in
    match lookup_function "CoreUI_Window" ctx.Context.the_module with
    | Some f -> f
    | None -> declare_function "CoreUI_Window" ty ctx.Context.the_module

  (* --- Ardium Runtime Print Intrinsics (Type Safe) --- *)
  
  let get_or_declare_print_str ctx =
    let ty = function_type (Types.void_type ctx) [| Types.i8_ptr_type ctx |] in
    match lookup_function "ardium_print_str" ctx.Context.the_module with
    | Some f -> f | None -> declare_function "ardium_print_str" ty ctx.Context.the_module

  let get_or_declare_println_str ctx =
    let ty = function_type (Types.void_type ctx) [| Types.i8_ptr_type ctx |] in
    match lookup_function "ardium_println_str" ctx.Context.the_module with
    | Some f -> f | None -> declare_function "ardium_println_str" ty ctx.Context.the_module

  let get_or_declare_print_int ctx =
    let ty = function_type (Types.void_type ctx) [| Types.i64_type ctx |] in
    match lookup_function "ardium_print_int" ctx.Context.the_module with
    | Some f -> f | None -> declare_function "ardium_print_int" ty ctx.Context.the_module

  let get_or_declare_println_int ctx =
    let ty = function_type (Types.void_type ctx) [| Types.i64_type ctx |] in
    match lookup_function "ardium_println_int" ctx.Context.the_module with
    | Some f -> f | None -> declare_function "ardium_println_int" ty ctx.Context.the_module

  let get_or_declare_print_double ctx =
    let ty = function_type (Types.void_type ctx) [| Types.double_type ctx |] in
    match lookup_function "ardium_print_double" ctx.Context.the_module with
    | Some f -> f | None -> declare_function "ardium_print_double" ty ctx.Context.the_module

  let get_or_declare_println_double ctx =
    let ty = function_type (Types.void_type ctx) [| Types.double_type ctx |] in
    match lookup_function "ardium_println_double" ctx.Context.the_module with
    | Some f -> f | None -> declare_function "ardium_println_double" ty ctx.Context.the_module

  (* background() -> void (Sets default bg color) *)
  let get_or_declare_background ctx =
    let ty = function_type (Types.void_type ctx) [||] in
    match lookup_function "CoreUI_Background" ctx.Context.the_module with
    | Some f -> f
    | None -> declare_function "CoreUI_Background" ty ctx.Context.the_module

  (* text(content, size, bold) -> void *)
  let get_or_declare_text ctx =
    let ty = function_type (Types.void_type ctx) 
      [| Types.i8_ptr_type ctx; Types.i32_type ctx; Types.i32_type ctx |] in
    match lookup_function "CoreUI_Text" ctx.Context.the_module with
    | Some f -> f
    | None -> declare_function "CoreUI_Text" ty ctx.Context.the_module
    
  (* add_button(label, callback_ptr) -> void *)
  (* For simplicity, treating callback as i64 (func ptr) for now *)
  let get_or_declare_add_button ctx =
    let ty = function_type (Types.void_type ctx) 
      [| Types.i8_ptr_type ctx; Types.i64_type ctx |] in
    match lookup_function "CoreUI_Button" ctx.Context.the_module with
    | Some f -> f
    | None -> declare_function "CoreUI_Button" ty ctx.Context.the_module

  (* add_image(path) -> void *)
  let get_or_declare_add_image ctx =
    let ty = function_type (Types.void_type ctx) [| Types.i8_ptr_type ctx |] in
    match lookup_function "CoreUI_Image" ctx.Context.the_module with
    | Some f -> f
    | None -> declare_function "CoreUI_Image" ty ctx.Context.the_module

  (* add_textfield(placeholder, is_secure) -> ptr (i64) *)
  let get_or_declare_add_textfield ctx =
    let ty = function_type (Types.i64_type ctx) 
      [| Types.i8_ptr_type ctx; Types.i32_type ctx |] in
    match lookup_function "CoreUI_TextField" ctx.Context.the_module with
    | Some f -> f
    | None -> declare_function "CoreUI_TextField" ty ctx.Context.the_module

  (* get_input_value(handle) -> string (ptr) *)
  let get_or_declare_get_input_value ctx =
    let ty = function_type (Types.i8_ptr_type ctx) [| Types.i64_type ctx |] in
    match lookup_function "CoreUI_GetInputValue" ctx.Context.the_module with
    | Some f -> f
    | None -> declare_function "CoreUI_GetInputValue" ty ctx.Context.the_module

end

(* --- Helper Functions --- *)
let get_struct_field_ptr ctx struct_decl field_name base_ptr =
  let rec find_index fields idx =
    match fields with
    | [] -> None
    | (name, ty_name) :: rest ->
        if name = field_name then Some (idx, ty_name)
        else find_index rest (idx + 1)
  in
  match find_index struct_decl.Ast.fields 0 with
  | Some (idx, ty_name) ->
      let field_ptr_type = Types.type_from_string ctx ty_name in
      
      (* Define index constant *)
      let idx_val_64 = const_int (Types.i64_type ctx) idx in

      (* Cast base to pointer of field type *)
      let typed_ptr =
        if Types.is_int_type (type_of base_ptr) ctx then
          build_inttoptr base_ptr (pointer_type ctx.Context.llvm_ctx) "typed_ptr" ctx.Context.builder
        else
          build_bitcast base_ptr (pointer_type ctx.Context.llvm_ctx) "typed_ptr" ctx.Context.builder
      in
      let gep = build_in_bounds_gep (Types.i64_type ctx) typed_ptr [| idx_val_64 |] "field_ptr" ctx.Context.builder in
      (gep, field_ptr_type)
  | None -> error ~line:ctx.Context.line ~col:ctx.Context.col
              (Printf.sprintf "Field '%s' not found in struct '%s'" field_name struct_decl.Ast.name)

(* --- Type Conversion --- *)
module Convert = struct
  let to_bool ctx value =
    let ty = type_of value in
    if ty = Types.bool_type ctx then value
    else if ty = Types.double_type ctx then
      let zero = const_float (Types.double_type ctx) 0.0 in
      build_fcmp Fcmp.One value zero "tobool" ctx.Context.builder
    else if Types.is_pointer_type ty then
      let null_ptr = const_null ty in
      build_icmp Icmp.Ne value null_ptr "tobool" ctx.Context.builder
    else
      (* Compare against generic 0 *)
      let zero = const_int ty 0 in
      build_icmp Icmp.Ne value zero "tobool" ctx.Context.builder

  let to_i32 ctx value =
    let ty = type_of value in
    if ty = Types.i32_type ctx then value
    else if ty = Types.double_type ctx then
      build_fptosi value (Types.i32_type ctx) "ftoi" ctx.Context.builder
    else if ty = Types.i64_type ctx then
      build_trunc value (Types.i32_type ctx) "trunc" ctx.Context.builder
    else if ty = Types.bool_type ctx then
      build_zext value (Types.i32_type ctx) "btoi" ctx.Context.builder
    else
      error ~line:ctx.Context.line ~col:ctx.Context.col "Cannot convert to i32"

  let to_i64 ctx value =
    let ty = type_of value in
    if ty = Types.i64_type ctx then value
    else if ty = Types.i32_type ctx then
      build_sext value (Types.i64_type ctx) "sext" ctx.Context.builder
    else if ty = Types.double_type ctx then
      build_fptosi value (Types.i64_type ctx) "ftoi" ctx.Context.builder
    else if ty = Types.bool_type ctx then
      build_zext value (Types.i64_type ctx) "btoi" ctx.Context.builder
    else
      error ~line:ctx.Context.line ~col:ctx.Context.col "Cannot convert to i64"

  let to_double ctx value =
    let ty = type_of value in
    if ty = Types.double_type ctx then value
    else if Types.is_int_type ty ctx then
      build_sitofp value (Types.double_type ctx) "itof" ctx.Context.builder
    else if ty = Types.bool_type ctx then
      let as_int = build_zext value (Types.i32_type ctx) "btoi" ctx.Context.builder in
      build_sitofp as_int (Types.double_type ctx) "itof" ctx.Context.builder
    else
      error ~line:ctx.Context.line ~col:ctx.Context.col "Cannot convert to double"

  let promote_to_common_type ctx lhs rhs =
    let lty = type_of lhs in
    let rty = type_of rhs in

    if lty = rty then (lhs, rhs)
    else if lty = Types.double_type ctx then
      (lhs, to_double ctx rhs)
    else if rty = Types.double_type ctx then
      (to_double ctx lhs, rhs)
    else if lty = Types.i64_type ctx && rty = Types.i32_type ctx then
      (lhs, to_i64 ctx rhs)
    else if lty = Types.i32_type ctx && rty = Types.i64_type ctx then
      (to_i64 ctx lhs, rhs)
    else
      (lhs, rhs)

  let logical_or ctx lhs rhs =
    let l_bool = to_bool ctx lhs in
    let r_bool = to_bool ctx rhs in
    let res = build_or l_bool r_bool "ortmp" ctx.Context.builder in
    build_zext res (Types.i64_type ctx) "or_ext" ctx.Context.builder

  let logical_and ctx lhs rhs =
    let l_bool = to_bool ctx lhs in
    let r_bool = to_bool ctx rhs in
    let res = build_and l_bool r_bool "andtmp" ctx.Context.builder in
    build_zext res (Types.i64_type ctx) "and_ext" ctx.Context.builder
end

(* --- Expression Code Generation --- *)
module Expr = struct
  let rec codegen ctx = function
    | Int i -> const_int (Types.i64_type ctx) i
    | Float f -> const_float (Types.double_type ctx) f
    | String s ->
        let str_val = const_stringz ctx.Context.llvm_ctx s in
        let g = define_global "str_lit" str_val ctx.Context.the_module in
        set_global_constant true g;
        set_unnamed_addr true g;
        set_linkage Linkage.Internal g;
        let zero = const_int (Types.i32_type ctx) 0 in
        const_in_bounds_gep (type_of str_val) g [| zero; zero |]
    | Var name -> begin
        match Context.find_named_value ctx name with
        | Some (ptr, ty) ->
            build_load ty ptr name ctx.Context.builder
        | None ->
            error ~line:ctx.Context.line ~col:ctx.Context.col
              (Printf.sprintf "Undefined variable: '%s'" name)
      end
    | Global -> 
        error ~line:ctx.Context.line ~col:ctx.Context.col "Global keyword cannot be used as standalone expression yet"
    | MemberAccess (obj, member) -> begin
        match obj with
        | Var "Math" -> codegen_math_constant ctx member
        | Global -> 
            (* Handle GLOBAL.field *)
            let (ptr_opt, struct_opt) = Context.get_global_map ctx in
            begin match ptr_opt, struct_opt with
            | Some ptr, Some decl ->
                let (gep, ty) = get_struct_field_ptr ctx decl member ptr in
                build_load ty gep ("val_" ^ member) ctx.Context.builder
            | _ -> error ~line:ctx.Context.line ~col:ctx.Context.col "GLOBAL not initialized with 'GLOBAL(ptr) as Type'"
            end
        | _ -> 
           match member with
           | "PI" -> const_float (Types.double_type ctx) 3.14159265358979323846
           | "E" -> const_float (Types.double_type ctx) 2.71828182845904523536
           | _ -> error ~line:ctx.Context.line ~col:ctx.Context.col
                    (Printf.sprintf "Member access '%s' not implemented" member)
      end
    | BinOp (lhs, op, rhs) ->
        codegen_binop ctx lhs op rhs
    | Call (fname, args) ->
        codegen_call ctx fname args
    | Tuple _ -> error "Tuple not implemented"
    | Named _ -> error "Named arguments not implemented"
    | Async expr -> 
        let task_name = Printf.sprintf "__async_task_%d" (Hashtbl.length ctx.Context.named_values + 1000) in
        let fn_ty = function_type (Types.void_ptr_type ctx) [| Types.void_ptr_type ctx |] in
        let task_fn = declare_function task_name fn_ty ctx.Context.the_module in
        let bb = append_block ctx.Context.llvm_ctx "entry" task_fn in
        
        (* Save current insert block *)
        let current_bb = insertion_block ctx.Context.builder in
        
        position_at_end bb ctx.Context.builder;
        
        (* Generate the expression in the new function *)
        ignore (codegen ctx expr);
        ignore (build_ret (const_null (Types.void_ptr_type ctx)) ctx.Context.builder);
        
        (* Restore builder position *)
        position_at_end current_bb ctx.Context.builder;
        
        (* Call pthread_create *)
        let pthread_create = Builtins.get_or_declare_pthread_create ctx in
        let thread_ptr = build_alloca (Types.i64_type ctx) "thread_handle" ctx.Context.builder in
        let null_attr = const_null (Types.i8_ptr_type ctx) in
        let null_arg = const_null (Types.i8_ptr_type ctx) in
        let fn_cast = build_bitcast task_fn (Types.i8_ptr_type ctx) "fn_cast" ctx.Context.builder in
        
        let thread_ptr_cast = build_bitcast thread_ptr (Types.i8_ptr_type ctx) "tptr_cast" ctx.Context.builder in
        let pc_ty = function_type (Types.i32_type ctx) 
            [| Types.i8_ptr_type ctx; Types.i8_ptr_type ctx; Types.i8_ptr_type ctx; Types.i8_ptr_type ctx |] in
        ignore (build_call pc_ty pthread_create [| thread_ptr_cast; null_attr; fn_cast; null_arg |] "" ctx.Context.builder);
        
        build_load (Types.i64_type ctx) thread_ptr "thread_id" ctx.Context.builder

    | Await expr ->
        let handle = codegen ctx expr in
        let pthread_join = Builtins.get_or_declare_pthread_join ctx in
        let null_retval = const_null (pointer_type ctx.Context.llvm_ctx) in
        let pj_ty = function_type (Types.i32_type ctx) 
            [| Types.i64_type ctx; Types.i8_ptr_type ctx |] in
        ignore (build_call pj_ty pthread_join [| handle; null_retval |] "" ctx.Context.builder);
        const_int (Types.i64_type ctx) 0
    | MemberCall _ -> error "MemberCall not implemented"
    | Lambda _ -> error "Lambda not implemented"
    | StructInit _ -> error "StructInit not implemented"
    | Index (arr, idx) -> 
        codegen_array_access ctx arr idx

  and codegen_math_constant ctx member =
    match member with
    | "PI" -> const_float (Types.double_type ctx) 3.14159265358979323846
    | "E" -> const_float (Types.double_type ctx) 2.71828182845904523536
    | "SQRT2" -> const_float (Types.double_type ctx) 1.41421356237309504880
    | "LN2" -> const_float (Types.double_type ctx) 0.69314718055994530942
    | "LN10" -> const_float (Types.double_type ctx) 2.30258509299404568402
    | _ -> error ~line:ctx.Context.line ~col:ctx.Context.col
             (Printf.sprintf "Unknown Math constant: '%s'" member)

  and codegen_array_access ctx _arr _idx =
    error ~line:ctx.Context.line ~col:ctx.Context.col "Array access not fully implemented yet"

  and codegen_binop ctx lhs op rhs =
    (* Handle string concatenation *)
    let lhs_val = codegen ctx lhs in
    let rhs_val = codegen ctx rhs in
    let lhs_ty = type_of lhs_val in
    let rhs_ty = type_of rhs_val in

    (* String operations *)
    if Types.is_pointer_type lhs_ty && Types.is_pointer_type rhs_ty then begin
      match op with
      | "+" -> codegen_string_concat ctx lhs_val rhs_val
      | "==" | "!=" -> codegen_string_compare ctx lhs_val rhs_val op
      | _ -> error ~line:ctx.Context.line ~col:ctx.Context.col
                (Printf.sprintf "Unsupported operator '%s' for strings" op)
    end
    else if op = "or" || op = "||" then
      Convert.logical_or ctx lhs_val rhs_val
    else if op = "and" || op = "&&" then
      Convert.logical_and ctx lhs_val rhs_val
    else
      let l, r = Convert.promote_to_common_type ctx lhs_val rhs_val in
      let ty = type_of l in

      if Types.is_float_type ty ctx then
        codegen_float_binop ctx l r op
      else
        codegen_int_binop ctx l r op

  and codegen_string_concat ctx lhs rhs =
    let strlen_func = Builtins.get_or_declare_strlen ctx in
    let malloc_func = Builtins.get_or_declare_malloc ctx in
    let sprintf_func = Builtins.get_or_declare_sprintf ctx in

    let strlen_ty = function_type (Types.i64_type ctx) [| Types.i8_ptr_type ctx |] in
    let len1 = build_call strlen_ty strlen_func [| lhs |] "len1" ctx.Context.builder in
    let len2 = build_call strlen_ty strlen_func [| rhs |] "len2" ctx.Context.builder in
    let total_len = build_add len1 len2 "totallen" ctx.Context.builder in
    let total_len_plus = build_add total_len (const_int (Types.i64_type ctx) 1) "totallen_plus" ctx.Context.builder in

    let malloc_ty = function_type (Types.i8_ptr_type ctx) [| Types.i64_type ctx |] in
    let result_ptr = build_call malloc_ty malloc_func [| total_len_plus |] "concat_result" ctx.Context.builder in

    let fmt = const_stringz ctx.Context.llvm_ctx "%s%s" in
    let fmt_global = define_global "concat_fmt" fmt ctx.Context.the_module in
    set_global_constant true fmt_global;
    let zero = const_int (Types.i32_type ctx) 0 in
    let fmt_ptr = const_in_bounds_gep (type_of fmt) fmt_global [| zero; zero |] in

    let sprintf_ty = var_arg_function_type (Types.i32_type ctx)
                        [| Types.i8_ptr_type ctx; Types.i8_ptr_type ctx |] in
    ignore (build_call sprintf_ty sprintf_func [| result_ptr; fmt_ptr; lhs; rhs |] "" ctx.Context.builder);
    result_ptr

  and codegen_string_compare ctx lhs rhs op =
    let strcmp_func = Builtins.get_or_declare_strcmp ctx in
    let strcmp_ty = function_type (Types.i32_type ctx)
                      [| Types.i8_ptr_type ctx; Types.i8_ptr_type ctx |] in
    let cmp_result = build_call strcmp_ty strcmp_func [| lhs; rhs |] "strcmp" ctx.Context.builder in
    let zero = const_int (Types.i32_type ctx) 0 in

    let cmp = match op with
      | "==" -> build_icmp Icmp.Eq cmp_result zero "streq" ctx.Context.builder
      | "!=" -> build_icmp Icmp.Ne cmp_result zero "strne" ctx.Context.builder
      | _ -> error ~line:ctx.Context.line ~col:ctx.Context.col "Invalid string comparison"
    in
    build_zext cmp (Types.i64_type ctx) "str_cmp_ext" ctx.Context.builder

  and codegen_float_binop ctx l r op =
    match op with
    | "+" -> build_fadd l r "fadd" ctx.Context.builder
    | "-" -> build_fsub l r "fsub" ctx.Context.builder
    | "*" -> build_fmul l r "fmul" ctx.Context.builder
    | "/" -> build_fdiv l r "fdiv" ctx.Context.builder
    | "==" ->
        let cmp = build_fcmp Fcmp.Oeq l r "feq" ctx.Context.builder in
        build_zext cmp (Types.i64_type ctx) "feq_ext" ctx.Context.builder
    | "!=" ->
        let cmp = build_fcmp Fcmp.One l r "fne" ctx.Context.builder in
        build_zext cmp (Types.i64_type ctx) "fne_ext" ctx.Context.builder
    | "<" ->
        let cmp = build_fcmp Fcmp.Olt l r "flt" ctx.Context.builder in
        build_zext cmp (Types.i64_type ctx) "flt_ext" ctx.Context.builder
    | ">" ->
        let cmp = build_fcmp Fcmp.Ogt l r "fgt" ctx.Context.builder in
        build_zext cmp (Types.i64_type ctx) "fgt_ext" ctx.Context.builder
    | "<=" ->
        let cmp = build_fcmp Fcmp.Ole l r "fle" ctx.Context.builder in
        build_zext cmp (Types.i64_type ctx) "fle_ext" ctx.Context.builder
    | ">=" ->
        let cmp = build_fcmp Fcmp.Oge l r "fge" ctx.Context.builder in
        build_zext cmp (Types.i64_type ctx) "fge_ext" ctx.Context.builder
    | _ -> error ~line:ctx.Context.line ~col:ctx.Context.col
             (Printf.sprintf "Unsupported operator for float: '%s'" op)

  and codegen_int_binop ctx l r op =
    match op with
    | "+" -> build_add l r "add" ctx.Context.builder
    | "-" -> build_sub l r "sub" ctx.Context.builder
    | "*" -> build_mul l r "mul" ctx.Context.builder
    | "/" -> build_sdiv l r "div" ctx.Context.builder
    | "%" -> build_srem l r "rem" ctx.Context.builder
    | "==" ->
        let cmp = build_icmp Icmp.Eq l r "eq" ctx.Context.builder in
        build_zext cmp (Types.i64_type ctx) "eq_ext" ctx.Context.builder
    | "!=" ->
        let cmp = build_icmp Icmp.Ne l r "ne" ctx.Context.builder in
        build_zext cmp (Types.i64_type ctx) "ne_ext" ctx.Context.builder
    | "<" ->
        let cmp = build_icmp Icmp.Slt l r "lt" ctx.Context.builder in
        build_zext cmp (Types.i64_type ctx) "lt_ext" ctx.Context.builder
    | ">" ->
        let cmp = build_icmp Icmp.Sgt l r "gt" ctx.Context.builder in
        build_zext cmp (Types.i64_type ctx) "gt_ext" ctx.Context.builder
    | "<=" ->
        let cmp = build_icmp Icmp.Sle l r "le" ctx.Context.builder in
        build_zext cmp (Types.i64_type ctx) "le_ext" ctx.Context.builder
    | ">=" ->
        let cmp = build_icmp Icmp.Sge l r "ge" ctx.Context.builder in
        build_zext cmp (Types.i64_type ctx) "ge_ext" ctx.Context.builder
    | _ -> error ~line:ctx.Context.line ~col:ctx.Context.col
             (Printf.sprintf "Unknown operator: '%s'" op)

  and codegen_call ctx fname args =
    let args_val = List.map (codegen ctx) args |> Array.of_list in

    if fname = "input" || fname = "read" then
      codegen_input_call ctx args_val
    else if Builtins.is_io_function fname then
      codegen_io_call ctx fname args_val
    else
      codegen_function_call ctx fname args_val

  and codegen_coreui_call ctx fname args_val =
    match fname with
    | "agui" -> 
        let f = Builtins.get_or_declare_agui ctx in
        let f_ty = function_type (Types.void_type ctx) [||] in
        build_call f_ty f [||] "" ctx.Context.builder
    | "run_apple_gui" -> 
        let f = Builtins.get_or_declare_run_gui ctx in
        let f_ty = function_type (Types.void_type ctx) [||] in
        build_call f_ty f [||] "" ctx.Context.builder
    | "viewer" ->
        (* Args: title, x, y, w, h *)
        let f = Builtins.get_or_declare_viewer ctx in
        if Array.length args_val < 5 then error "viewer expects 5 args";
        (* Convert X,Y,W,H to i32 *)
        let title = args_val.(0) in
        let x = Convert.to_i32 ctx args_val.(1) in
        let y = Convert.to_i32 ctx args_val.(2) in
        let w = Convert.to_i32 ctx args_val.(3) in
        let h = Convert.to_i32 ctx args_val.(4) in
        let f_ty = function_type (Types.void_type ctx) 
          [| Types.i8_ptr_type ctx; Types.i32_type ctx; Types.i32_type ctx; Types.i32_type ctx; Types.i32_type ctx |] in
        build_call f_ty f [| title; x; y; w; h |] "" ctx.Context.builder

    | "background" ->
        let f = Builtins.get_or_declare_background ctx in
        let f_ty = function_type (Types.void_type ctx) [||] in
        build_call f_ty f [||] "" ctx.Context.builder

    | "text" ->
        let f = Builtins.get_or_declare_text ctx in
        let content = args_val.(0) in
        let size = Convert.to_i32 ctx args_val.(1) in
        let bold = Convert.to_i32 ctx args_val.(2) in
        let f_ty = function_type (Types.void_type ctx) 
          [| Types.i8_ptr_type ctx; Types.i32_type ctx; Types.i32_type ctx |] in
        build_call f_ty f [| content; size; bold |] "" ctx.Context.builder
    
    | "add_button" -> 
        let f = Builtins.get_or_declare_add_button ctx in
        let label = args_val.(0) in
        let cb = Convert.to_i64 ctx args_val.(1) in
        let f_ty = function_type (Types.void_type ctx) 
          [| Types.i8_ptr_type ctx; Types.i64_type ctx |] in
        build_call f_ty f [| label; cb |] "" ctx.Context.builder

    | "add_image" ->
        let f = Builtins.get_or_declare_add_image ctx in
        let f_ty = function_type (Types.void_type ctx) [| Types.i8_ptr_type ctx |] in
        build_call f_ty f [| args_val.(0) |] "" ctx.Context.builder

    | "add_textfield" ->
        let f = Builtins.get_or_declare_add_textfield ctx in
        let ph = args_val.(0) in
        let sec = Convert.to_i32 ctx args_val.(1) in
        let f_ty = function_type (Types.i64_type ctx) 
          [| Types.i8_ptr_type ctx; Types.i32_type ctx |] in
        build_call f_ty f [| ph; sec |] "tf_handle" ctx.Context.builder

    | "get_input_value" ->
        let f = Builtins.get_or_declare_get_input_value ctx in
        let h = Convert.to_i64 ctx args_val.(0) in
        let f_ty = function_type (Types.i8_ptr_type ctx) [| Types.i64_type ctx |] in
        build_call f_ty f [| h |] "input_val" ctx.Context.builder

    | _ -> error ("Unknown CoreUI function: " ^ fname)

  and codegen_input_call ctx _args_val =
    let scanf_func = Builtins.get_or_declare_scanf ctx in
    let result_ptr = build_alloca (Types.i64_type ctx) "input_tmp" ctx.Context.builder in
    let fmt = Builtins.get_input_format_string ctx (Types.i64_type ctx) in
    let scanf_ty = var_arg_function_type (Types.i32_type ctx) [| Types.i8_ptr_type ctx |] in
    ignore (build_call scanf_ty scanf_func [| fmt; result_ptr |] "" ctx.Context.builder);
    build_load (Types.i64_type ctx) result_ptr "input_val" ctx.Context.builder

  and codegen_io_call ctx fname args_val =
    if Array.length args_val = 0 then (
      (* For println with no args, print newline *)
      if fname = "println" then (
          let f = Builtins.get_or_declare_println_str ctx in
          let null_str = const_null (Types.i8_ptr_type ctx) in
          let f_ty = function_type (Types.void_type ctx) [| Types.i8_ptr_type ctx |] in
          ignore (build_call f_ty f [| null_str |] "" ctx.Context.builder)
      );
      const_int (Types.i64_type ctx) 0
    ) else begin
      let arg = args_val.(0) in
      let ty = type_of arg in
      let is_println = (fname = "println") in
      
      if ty = Types.i8_ptr_type ctx then (
          let f = if is_println then Builtins.get_or_declare_println_str ctx else Builtins.get_or_declare_print_str ctx in
          let f_ty = function_type (Types.void_type ctx) [| Types.i8_ptr_type ctx |] in
          ignore (build_call f_ty f [| arg |] "" ctx.Context.builder)
      ) else if ty = Types.double_type ctx then (
          let f = if is_println then Builtins.get_or_declare_println_double ctx else Builtins.get_or_declare_print_double ctx in
          let f_ty = function_type (Types.void_type ctx) [| Types.double_type ctx |] in
          ignore (build_call f_ty f [| arg |] "" ctx.Context.builder)
      ) else (
          (* Default to int64 *)
          let f = if is_println then Builtins.get_or_declare_println_int ctx else Builtins.get_or_declare_print_int ctx in
          let val_i64 = if ty = Types.i32_type ctx || ty = Types.bool_type ctx then build_zext arg (Types.i64_type ctx) "zext" ctx.Context.builder else arg in
          let f_ty = function_type (Types.void_type ctx) [| Types.i64_type ctx |] in
          ignore (build_call f_ty f [| val_i64 |] "" ctx.Context.builder)
      );

      const_int (Types.i64_type ctx) 0
    end

  and codegen_function_call ctx fname args_val =
    match lookup_function fname ctx.Context.the_module with
    | Some f ->
        let is_math = Builtins.is_math_function fname in

        let final_args =
          if is_math then
            Array.map (fun v ->
              if Types.is_int_type (type_of v) ctx then
                Convert.to_double ctx v
              else v
            ) args_val
          else args_val
        in

        let expected = Array.length (params f) in
        let actual = Array.length final_args in
        let is_var_arg = is_var_arg (element_type (type_of f)) in

        if not is_var_arg && expected <> actual then
          error ~line:ctx.Context.line ~col:ctx.Context.col
            (Printf.sprintf "Function '%s' expects %d arguments, got %d" fname expected actual);

        let ret_type = if is_math then Types.double_type ctx else Types.i64_type ctx in
        let arg_types = Array.map type_of final_args in
        let fn_type = function_type ret_type arg_types in

        build_call fn_type f final_args (fname ^ "_call") ctx.Context.builder

    | None ->
        error ~line:ctx.Context.line ~col:ctx.Context.col
          (Printf.sprintf "Undefined function: '%s'" fname)
end

(* --- Statement Code Generation --- *)
module Stmt = struct
  let rec codegen ctx func_val = function
    | Let (name, _ty_opt, expr, _decs) ->
        let init_val = Expr.codegen ctx expr in
        let var_type = type_of init_val in
        let alloca = build_alloca var_type name ctx.Context.builder in
        ignore (build_store init_val alloca ctx.Context.builder);
        
        (* Implicit ARC Retain for pointers *)
        if Types.is_pointer_type var_type then (
            let retain_fn = Builtins.get_or_declare_arc_retain ctx in
            let retain_ty = function_type (Types.void_type ctx) [| Types.i8_ptr_type ctx |] in
            ignore (build_call retain_ty retain_fn [| init_val |] "" ctx.Context.builder)
        );
        
        Context.add_named_value ctx name alloca var_type

    | Own (name, _ty_opt, expr, _decs) ->
        (* Ownership transfer - no retain called, but will be released at end of scope *)
        let init_val = Expr.codegen ctx expr in
        let var_type = type_of init_val in
        let alloca = build_alloca var_type name ctx.Context.builder in
        ignore (build_store init_val alloca ctx.Context.builder);
        Context.add_named_value ctx name alloca var_type

    | Borrow (name, _ty_opt, expr, _decs) ->
        (* Borrowing - no retain, no release *)
        let init_val = Expr.codegen ctx expr in
        let var_type = type_of init_val in
        let alloca = build_alloca var_type name ctx.Context.builder in
        ignore (build_store init_val alloca ctx.Context.builder);
        Context.add_named_value ctx name alloca var_type

    | TryCatch (try_block, err_name, catch_block) ->
        let clear_fn = Builtins.get_or_declare_sys_clear_error ctx in
        let clear_ty = function_type (Types.void_type ctx) [||] in
        ignore (build_call clear_ty clear_fn [||] "" ctx.Context.builder);

        let try_bb = append_block ctx.Context.llvm_ctx "try_block" func_val in
        let catch_bb = append_block ctx.Context.llvm_ctx "catch_block" func_val in
        let merge_bb = append_block ctx.Context.llvm_ctx "try_merge" func_val in

        ignore (build_br try_bb ctx.Context.builder);
        position_at_end try_bb ctx.Context.builder;
        
        List.iter (fun stmt -> ignore (codegen ctx func_val stmt)) try_block;

        let get_fn = Builtins.get_or_declare_sys_get_error ctx in
        let get_ty = function_type (Types.i8_ptr_type ctx) [||] in
        let err_ptr = build_call get_ty get_fn [||] "err_ptr" ctx.Context.builder in
        
        let null_ptr = const_null (Types.i8_ptr_type ctx) in
        let has_err = build_icmp Icmp.Ne err_ptr null_ptr "has_err" ctx.Context.builder in
        
        ignore (build_cond_br has_err catch_bb merge_bb ctx.Context.builder);

        position_at_end catch_bb ctx.Context.builder;
        let alloca = build_alloca (Types.i8_ptr_type ctx) err_name ctx.Context.builder in
        ignore (build_store err_ptr alloca ctx.Context.builder);
        Context.add_named_value ctx err_name alloca (Types.i8_ptr_type ctx);
        
        List.iter (fun stmt -> ignore (codegen ctx func_val stmt)) catch_block;
        ignore (build_call clear_ty clear_fn [||] "" ctx.Context.builder); (* Clear after catch *)
        ignore (build_br merge_bb ctx.Context.builder);

        position_at_end merge_bb ctx.Context.builder

    | Err err_opt ->
        let throw_fn = Builtins.get_or_declare_sys_throw ctx in
        let throw_ty = function_type (Types.void_type ctx) [| Types.i8_ptr_type ctx |] in
        let err_str = match err_opt with
          | Some s ->
              let str_val = const_stringz ctx.Context.llvm_ctx s in
              let g = define_global "err_str" str_val ctx.Context.the_module in
              set_global_constant true g;
              set_unnamed_addr true g;
              set_linkage Linkage.Internal g;
              let zero = const_int (Types.i32_type ctx) 0 in
              const_in_bounds_gep (type_of str_val) g [| zero; zero |]
          | None -> const_null (Types.i8_ptr_type ctx)
        in
        ignore (build_call throw_ty throw_fn [| err_str |] "" ctx.Context.builder)

    | Assign (lhs, rhs) -> begin
        match lhs with
        | Var name -> 
            let new_val = Expr.codegen ctx rhs in
            begin match Context.find_named_value ctx name with
            | Some (ptr, expected_ty) ->
                let new_val_ty = type_of new_val in
                let converted_val =
                  if new_val_ty = expected_ty then new_val
                  else if expected_ty = Types.double_type ctx then
                    Convert.to_double ctx new_val
                  else if expected_ty = Types.i64_type ctx then
                    Convert.to_i64 ctx new_val
                  else if expected_ty = Types.i32_type ctx then
                    Convert.to_i32 ctx new_val
                  else
                    error ~line:ctx.Context.line ~col:ctx.Context.col
                      (Printf.sprintf "Type mismatch in assignment to '%s'" name)
                in
                ignore (build_store converted_val ptr ctx.Context.builder)
            | None ->
                error ~line:ctx.Context.line ~col:ctx.Context.col
                  (Printf.sprintf "Undefined variable in assignment: '%s'" name)
            end
        | MemberAccess (Global, member) ->
            (* Handle GLOBAL.field = rhs *)
            let new_val = Expr.codegen ctx rhs in
            let (ptr_opt, struct_opt) = Context.get_global_map ctx in
            begin match ptr_opt, struct_opt with
            | Some ptr, Some decl ->
                let (gep, ty) = get_struct_field_ptr ctx decl member ptr in
                (* Convert if needed *)
                let converted_val = 
                  if (type_of new_val) = ty then new_val
                  else if ty = Types.double_type ctx then Convert.to_double ctx new_val
                  else if ty = Types.i64_type ctx then Convert.to_i64 ctx new_val
                  else new_val (* hope for the best *)
                in
                ignore (build_store converted_val gep ctx.Context.builder)
            | _ -> error ~line:ctx.Context.line ~col:ctx.Context.col "GLOBAL not initialized"
            end
        | _ -> error "Complex assignment LHS not implemented"
      end

    | Return expr ->
        let ret_val = Expr.codegen ctx expr in
        ignore (build_ret ret_val ctx.Context.builder)

    | Expr expr ->
        ignore (Expr.codegen ctx expr)

    | If (cond, then_stmts, else_stmts) ->
        codegen_if ctx func_val cond then_stmts else_stmts

    | While (cond, body) ->
        codegen_while ctx func_val cond body
    
    | GlobalMap (expr, struct_name) ->
        let ptr = Expr.codegen ctx expr in
        begin match Context.find_struct ctx struct_name with
        | Some decl -> Context.set_global_map ctx ptr decl
        | None -> error ~line:ctx.Context.line ~col:ctx.Context.col
                    (Printf.sprintf "Unknown struct '%s' in GLOBAL map" struct_name)
        end

    | Spawn body ->
        let task_name = Printf.sprintf "__spawn_task_%d" (Hashtbl.length ctx.Context.named_values + 2000) in
        let fn_ty = function_type (Types.void_ptr_type ctx) [| Types.void_ptr_type ctx |] in
        let task_fn = declare_function task_name fn_ty ctx.Context.the_module in
        let bb = append_block ctx.Context.llvm_ctx "entry" task_fn in
        
        let current_bb = insertion_block ctx.Context.builder in
        position_at_end bb ctx.Context.builder;
        
        List.iter (fun stmt -> ignore (codegen ctx task_fn stmt)) body;
        ignore (build_ret (const_null (Types.void_ptr_type ctx)) ctx.Context.builder);
        
        position_at_end current_bb ctx.Context.builder;
        
        let pthread_create = Builtins.get_or_declare_pthread_create ctx in
        let thread_ptr = build_alloca (Types.i64_type ctx) "thread_handle" ctx.Context.builder in
        let null_attr = const_null (Types.i8_ptr_type ctx) in
        let null_arg = const_null (Types.i8_ptr_type ctx) in
        let fn_cast = build_bitcast task_fn (Types.i8_ptr_type ctx) "fn_cast" ctx.Context.builder in
        let thread_ptr_cast = build_bitcast thread_ptr (Types.i8_ptr_type ctx) "tptr_cast" ctx.Context.builder in
        
        let pc_ty = function_type (Types.i32_type ctx) 
            [| Types.i8_ptr_type ctx; Types.i8_ptr_type ctx; Types.i8_ptr_type ctx; Types.i8_ptr_type ctx |] in
        ignore (build_call pc_ty pthread_create [| thread_ptr_cast; null_attr; fn_cast; null_arg |] "" ctx.Context.builder)

    | VClass body ->
        ignore (build_call (function_type (void_type ctx.Context.llvm_ctx) [| i32_type ctx.Context.llvm_ctx |])
                  (declare_function "titan_ui_begin_container" (function_type (void_type ctx.Context.llvm_ctx) [| i32_type ctx.Context.llvm_ctx |]) ctx.Context.the_module)
                  [| const_int (i32_type ctx.Context.llvm_ctx) 0 |] "" ctx.Context.builder);
        List.iter (codegen ctx func_val) body;
        ignore (build_call (function_type (void_type ctx.Context.llvm_ctx) [||])
                  (declare_function "titan_ui_end_container" (function_type (void_type ctx.Context.llvm_ctx) [||]) ctx.Context.the_module)
                  [||] "" ctx.Context.builder)

    | HClass body ->
        ignore (build_call (function_type (void_type ctx.Context.llvm_ctx) [| i32_type ctx.Context.llvm_ctx |])
                  (declare_function "titan_ui_begin_container" (function_type (void_type ctx.Context.llvm_ctx) [| i32_type ctx.Context.llvm_ctx |]) ctx.Context.the_module)
                  [| const_int (i32_type ctx.Context.llvm_ctx) 1 |] "" ctx.Context.builder);
        List.iter (codegen ctx func_val) body;
        ignore (build_call (function_type (void_type ctx.Context.llvm_ctx) [||])
                  (declare_function "titan_ui_end_container" (function_type (void_type ctx.Context.llvm_ctx) [||]) ctx.Context.the_module)
                  [||] "" ctx.Context.builder)

    | ZClass body ->
        ignore (build_call (function_type (void_type ctx.Context.llvm_ctx) [| i32_type ctx.Context.llvm_ctx |])
                  (declare_function "titan_ui_begin_container" (function_type (void_type ctx.Context.llvm_ctx) [| i32_type ctx.Context.llvm_ctx |]) ctx.Context.the_module)
                  [| const_int (i32_type ctx.Context.llvm_ctx) 2 |] "" ctx.Context.builder);
        List.iter (codegen ctx func_val) body;
        ignore (build_call (function_type (void_type ctx.Context.llvm_ctx) [||])
                  (declare_function "titan_ui_end_container" (function_type (void_type ctx.Context.llvm_ctx) [||]) ctx.Context.the_module)
                  [||] "" ctx.Context.builder)

    | Reset -> error "Reset not implemented"
    | Err msg -> error (match msg with Some s -> "Err: " ^ s | None -> "Err")
    | GlobalDecl _ -> error "GlobalDecl stmt not implemented"
    | Decorator _ -> error "Decorator stmt not implemented"
    | ClassDecl _ -> error "ClassDecl stmt not implemented"
    | FuncDef _ -> error "Nested FuncDef not implemented"


  and codegen_if ctx func_val cond then_stmts else_stmts =
    let cond_val = Expr.codegen ctx cond in
    let cond_bool = Convert.to_bool ctx cond_val in

    let start_bb = insertion_block ctx.Context.builder in
    let the_func = block_parent start_bb in

    let then_bb = append_block ctx.Context.llvm_ctx "then" the_func in
    let else_bb = append_block ctx.Context.llvm_ctx "else" the_func in
    let merge_bb = append_block ctx.Context.llvm_ctx "merge" the_func in

    ignore (build_cond_br cond_bool then_bb else_bb ctx.Context.builder);

    position_at_end then_bb ctx.Context.builder;
    List.iter (codegen ctx func_val) then_stmts;
    let then_has_terminator =
      match block_terminator (insertion_block ctx.Context.builder) with
      | Some _ -> true
      | None -> false
    in
    if not then_has_terminator then
      ignore (build_br merge_bb ctx.Context.builder);

    position_at_end else_bb ctx.Context.builder;
    List.iter (codegen ctx func_val) else_stmts;
    let else_has_terminator =
      match block_terminator (insertion_block ctx.Context.builder) with
      | Some _ -> true
      | None -> false
    in
    if not else_has_terminator then
      ignore (build_br merge_bb ctx.Context.builder);

    position_at_end merge_bb ctx.Context.builder

  and codegen_while ctx func_val cond body =
    let start_bb = insertion_block ctx.Context.builder in
    let the_func = block_parent start_bb in

    let cond_bb = append_block ctx.Context.llvm_ctx "while_cond" the_func in
    let loop_bb = append_block ctx.Context.llvm_ctx "while_body" the_func in
    let after_bb = append_block ctx.Context.llvm_ctx "while_end" the_func in

    (* Save old loop context *)
    let old_loop_ctx = Context.get_loop_context ctx in
    Context.set_loop_context ctx { break_bb = Some after_bb; continue_bb = Some cond_bb };

    ignore (build_br cond_bb ctx.Context.builder);

    position_at_end cond_bb ctx.Context.builder;
    let cond_val = Expr.codegen ctx cond in
    let cond_bool = Convert.to_bool ctx cond_val in
    ignore (build_cond_br cond_bool loop_bb after_bb ctx.Context.builder);

    position_at_end loop_bb ctx.Context.builder;
    List.iter (codegen ctx func_val) body;
    let has_terminator =
      match block_terminator (insertion_block ctx.Context.builder) with
      | Some _ -> true
      | None -> false
    in
    if not has_terminator then
      ignore (build_br cond_bb ctx.Context.builder);

    position_at_end after_bb ctx.Context.builder;

    (* Restore old loop context *)
    Context.set_loop_context ctx old_loop_ctx
end

(* --- Function Prototype --- *)
module Proto = struct
  let codegen ctx name args ret_type is_extern =
    let is_math = Builtins.is_math_function name in

    (* Determine return type *)
    let llvm_ret_type =
      match ret_type with
      | Some "void" -> Types.void_type ctx
      | Some "int" | Some "i32" -> Types.i32_type ctx
      | Some "i64" -> Types.i64_type ctx
      | Some "double" | Some "float" -> Types.double_type ctx
      | Some "bool" -> Types.bool_type ctx
      | Some "ptr" | Some "string" -> Types.i8_ptr_type ctx
      | None ->
          if is_math then Types.double_type ctx
          else Types.i64_type ctx
      | Some _ -> Types.i64_type ctx (* Default fallback *)
    in

    (* Determine argument type *)
    let arg_types_list = 
        List.map (fun (_, ty_opt) -> 
            match ty_opt with
            | Some ty_name -> Types.type_from_string ctx ty_name
            | None -> if is_math then Types.double_type ctx else Types.i64_type ctx
        ) args 
    in
    let arg_types = Array.of_list arg_types_list in
    let ft = function_type llvm_ret_type arg_types in

    match lookup_function name ctx.Context.the_module with
    | None ->
        let f = declare_function name ft ctx.Context.the_module in

        if not is_extern then begin
          let param_arr = params f in
          List.iteri (fun i (arg_name, _) ->
            set_value_name arg_name param_arr.(i)
          ) args
        end;
        f

    | Some f ->
        if block_begin f <> At_end f && not is_extern then
          error ~line:ctx.Context.line ~col:ctx.Context.col
            (Printf.sprintf "Redefinition of function: '%s'" name);
        f
end

(* --- Optimization --- *)
module Optimize = struct
  let optimize_module _the_module =
    (* Optimizations disabled due to missing library linkage *)
    ()
end

(* --- Top Level Code Generation --- *)
let codegen_toplevel ctx = function
  | Ast.Extern (name, args, ret_type, _is_var, _decs) ->
      (* Convert args (string * string) to (string * string option) for Proto *)
      let args_fmt = List.map (fun (n, t) -> 
          if String.length t = 0 then (n, None) 
          else (n, Some t)
      ) args in
      ignore (Proto.codegen ctx name args_fmt (Some ret_type) true)

  | Ast.Struct decl ->
      Context.add_struct ctx decl.name decl

  | Ast.Func func ->
      Context.clear_named_values ctx;
      let the_function = Proto.codegen ctx func.name func.args None false in

      let entry_bb = append_block ctx.Context.llvm_ctx "entry" the_function in
      position_at_end entry_bb ctx.Context.builder;

      let is_math = Builtins.is_math_function func.name in
      let arg_type = if is_math then Types.double_type ctx else Types.i64_type ctx in
      let param_arr = params the_function in

      List.iteri (fun i (arg_name, _) ->
        let arg_val = param_arr.(i) in
        let alloca = build_alloca arg_type arg_name ctx.Context.builder in
        ignore (build_store arg_val alloca ctx.Context.builder);
        Context.add_named_value ctx arg_name alloca arg_type
      ) func.args;

      List.iter (Stmt.codegen ctx the_function) func.body;

      (* Add default return if needed *)
      begin match block_terminator (insertion_block ctx.Context.builder) with
      | Some _ -> ()
      | None ->
          let ret_type = return_type (element_type (type_of the_function)) in
          if ret_type = Types.void_type ctx then
            ignore (build_ret_void ctx.Context.builder)
          else if is_math then
            ignore (build_ret (const_float (Types.double_type ctx) 0.0) ctx.Context.builder)
          else
            ignore (build_ret (const_int (Types.i64_type ctx) 0) ctx.Context.builder)
      end;

      begin match Llvm_analysis.verify_function the_function with
      | true -> ()
      | false ->
          Printf.eprintf "Function verification failed for: %s\n" func.name;
          dump_value the_function;
          delete_function the_function;
          error ~line:ctx.Context.line ~col:ctx.Context.col
            (Printf.sprintf "Invalid function generated: '%s'" func.name)
      end

  | Ast.GlobalVar (name, _ty, expr, _decs) ->
      let init_val = Expr.codegen ctx expr in
      let var_type = type_of init_val in
      let global_var = define_global name init_val ctx.Context.the_module in
      set_linkage Linkage.Internal global_var;
      Context.add_global_value ctx name global_var var_type

  | Ast.Import _ -> () (* Imports handled in main *)
  | _ -> () (* Stubs for ClassDef, Struct, Enum, etc *)


(* --- Main Entry Point --- *)
let codegen_program ?triple ?(output_file="output.o") ?(optimize=false) module_name (top_levels : Ast.prog) =
  let ctx = Context.create module_name in

  (* Pre-scan for structs to ensure they are available before function bodies are generated *)
  List.iter (fun tl -> match tl with
    | Ast.Struct decl -> Context.add_struct ctx decl.name decl
    | _ -> ()
  ) top_levels;

  let _ = (triple : string option) in      (* Constrain to string option *)
  let _ = output_file in (* Mark as used *)

  try
    List.iter (codegen_toplevel ctx) top_levels;

    (* Run optimization if requested *)
    if optimize then
      Optimize.optimize_module ctx.Context.the_module;

    (* Write bitcode to output file *)
    if not (Llvm_bitwriter.write_bitcode_file ctx.Context.the_module output_file) then
      Printf.eprintf "⚠️ Warning: Failed to write bitcode to %s\n" output_file;

    (* If triple is provided, could configure target machine here *)
    
    Ok ctx.Context.the_module
  with
  | Codegen_error (msg, line, col) ->
      Error (error_msg msg line col)
  | Failure msg ->
      Error ("Code generation failure: " ^ msg)
  | e ->
      Error ("Unexpected error: " ^ Printexc.to_string e)

(* --- JIT Execution --- *)
let run_jit the_module =
  ignore (Llvm_all_backends.initialize ());
  ignore (Llvm_executionengine.initialize ());
  let _ee = Llvm_executionengine.create the_module in
  Printf.printf "JIT started (execution logic stubbed).\n";
  (* 
  let main_func_opt = lookup_function "main" the_module in
  match main_func_opt with
  | Some f ->
      (* run_function seems to be missing in this LLVM binding version *)
      (* let result = Llvm_executionengine.run_function f [||] ee in *)
      (* let int_res = Llvm_executionengine.GenericValue.as_int result in *)
      (* Printf.printf "Program exited with code: %d\n" int_res; *)
      ()
  | None ->
      Printf.eprintf "Error: 'main' function not found for JIT execution.\n"
  *)
  ()