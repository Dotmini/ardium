open Llvm

let () =
  let ctx = global_context () in
  let m = create_module ctx "test" in
  let b = builder ctx in
  let i32_t = i32_type ctx in
  let ft = function_type i32_t [||] in
  let f = define_function "main" ft m in
  let bb = append_block ctx "entry" f in
  position_at_end bb b;
  Printf.printf "Success!\n"
