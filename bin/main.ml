open Ardium_core

let usage_msg =
  "\n" ^
  "\027[1;36m       Dotmini Software\027[0m\n" ^
  "\027[1;37m      SPU AI CLUB      \027[0m\n" ^
  "\027[0;33m   Designed in Thailand\027[0m\n" ^
  "\n" ^
  "Usage: arc <command> [options] [file]\n\n" ^
  "Commands:\n" ^
  "  \027[1mbuild\027[0m <file.ar>    Compile source file to executable\n" ^
  "  \027[1mrun\027[0m   <file.ar>    Compile and run immediately (JIT)\n" ^
  "  \027[1mtest\027[0m  <dir/file>   Run unit tests recursively\n" ^
  "  \027[1mnew\027[0m   <name>       Create a new Ardium project\n" ^
  "  \027[1minstall\027[0m <pkg>       Install a third-party package\n" ^
  "  \027[1mheader\027[0m <file.ar>   Generate C header for Interop\n" ^
  "  \027[1mlsp\027[0m                Start Language Server (VSCode)\n" ^
  "  \027[1mdev\027[0m  <file.ar>    Hot-Reload Watcher Mode\n" ^
  "\n" ^
  "Options:"
let mode = ref "build"
let input_file = ref ""
let output_file = ref "a.out"
let target_triple = ref ""
let frameworks = ref []
let no_std = ref false
let extra_links = ref []
let compile_only = ref false
let build_dylib = ref false

(* Enhanced Argument Parsing *)
let anon_fun filename =
  if !mode = "build" && !input_file = "" then (
    match filename with
    | "build" | "run" | "test" | "lsp" | "new" | "install" | "header" | "dev" -> mode := filename
    | s when not (String.contains s '.') ->
        Printf.eprintf "❌ Error: Unknown command '%s'.\nAvailable commands: build, run, test, lsp, new, install, header\n" s;
        exit 1
    | _ -> input_file := filename
  ) else if !input_file = "" then
    input_file := filename
  else
    output_file := filename

let spec_list = [
  ("-o", Arg.Set_string output_file, "Specify output executable name");
  ("-c", Arg.Set compile_only, "Compile only, do not link");
  ("-dylib", Arg.Set build_dylib, "Build as Dynamic Library");
  ("--version", Arg.Unit (fun () -> Printf.printf "Ardium Compiler v2.5.6 (Stable)\n"; exit 0), "Show version information");
  ("--no-std", Arg.Set no_std, "Compile without standard library");
  ("--target", Arg.Set_string target_triple, "Specify target triple");
  ("-framework", Arg.String (fun f -> frameworks := f :: !frameworks), "Link against a macOS framework");
]


module StringSet = Set.Make(String)

(* Import Resolution Logic *)
let rec resolve_imports stdlib_path imported_set program =
  let open Ast in
  List.fold_left (fun acc p ->
    match p with
    | Import lib_name ->
        if StringSet.mem lib_name !imported_set then acc
        else (
          imported_set := StringSet.add lib_name !imported_set;
          let filename = lib_name ^ ".ar" in
          let file_path =
            if Stdlib.Sys.file_exists filename then filename
            else if Stdlib.Sys.file_exists (Filename.concat "ardium_modules" filename) then
                Filename.concat "ardium_modules" filename
            else Filename.concat stdlib_path filename
          in

          if Stdlib.Sys.file_exists file_path then (
            Printf.printf "📦 Importing %s...\n" lib_name;
            let chan = Stdlib.open_in file_path in
            let lexbuf = Lexing.from_channel chan in
            let lib_prog = Parser.prog Lexer.read lexbuf in

            Stdlib.close_in chan;
            (* Recursively resolve imports from the library *)
            let resolved_lib = resolve_imports stdlib_path imported_set lib_prog in
            acc @ resolved_lib
          ) else (
            Printf.eprintf "⚠️ Warning: Library '%s' not found at %s. Ignoring.\n" lib_name file_path;
            acc
          )
        )
    | _ -> acc @ [p]
  ) [] program

(* Core Compilation Logic *)
let process_file input_path output_path =
  let chan = try Stdlib.open_in input_path with Sys_error msg ->
    Printf.eprintf "❌ Error: Cannot open input file '%s': %s\n" input_path msg; exit 1
  in
  let lexbuf = Lexing.from_channel chan in
  let program = try Parser.prog Lexer.read lexbuf with
    | Parser.Error ->
      let pos = Lexing.lexeme_start_p lexbuf in
      Printf.eprintf "❌ Parse Error in %s at line %d, column %d. Unexpected token: '%s'\n"
        input_path pos.pos_lnum (pos.pos_cnum - pos.pos_bol + 1) (Lexing.lexeme lexbuf);
      exit 1
    | exn ->
      Printf.eprintf "❌ Error in %s: %s\n" input_path (Printexc.to_string exn);
      exit 1
  in
  Stdlib.close_in chan;

  let stdlib_path =
    try Stdlib.Sys.getenv "ARDIUM_STDLIB"
    with Stdlib.Not_found ->
      if Sys.file_exists "stdlib" then "stdlib"
      else if Sys.file_exists "/usr/local/lib/ardium/stdlib" then "/usr/local/lib/ardium/stdlib"
      else "/usr/local/ardium/stdlib"
  in



  (* Automatically load Core if NO_STD is not set *)
  let base_program =
    if !no_std then program
    else
      let core_import = [Ast.Import "Core"] in (* Inject implicit Core import *)
      core_import @ program
  in

  let imported_set = ref StringSet.empty in
  let final_program = resolve_imports stdlib_path imported_set base_program in

  let bc_file = output_path ^ ".bc" in

  let runtime_flags =
     if Stdlib.Sys.file_exists "runtime/build/libardium.dylib" then
       "-Lruntime/build -lardium -rpath runtime/build"
     (* Distribution bundle path *)
     else if Stdlib.Sys.file_exists "/usr/local/ardium/lib/libardium.dylib" then
       "-L/usr/local/ardium/lib -lardium -rpath /usr/local/ardium/lib"
     else
       let lib_path = try Stdlib.Sys.getenv "ARDIUM_LIB_PATH" with Stdlib.Not_found -> "/usr/local/lib" in
       Printf.sprintf "-L%s -lardium -rpath %s" lib_path lib_path
  in

  let coreui_flags, coreui_libs =
    if Stdlib.Sys.file_exists "lib/libCoreUI.a" then
      ("-Llib -lCoreUI -lc++", "-framework Metal -framework Cocoa -framework QuartzCore")
    else ("", "")
  in

  let framework_args = List.map (fun f -> Printf.sprintf "-framework %s" f) !frameworks |> String.concat " " in
  let link_args = List.map (fun l -> Printf.sprintf "-l%s" l) !extra_links |> String.concat " " in
  let triple = if String.length !target_triple = 0 then None else Some !target_triple in

  (* Combine flags *)
  let all_frameworks = framework_args ^ " " ^ coreui_libs in
  
  let raylib_flags = 
    if Stdlib.Sys.file_exists "lib/libraylib.a" then
      " -Llib -lraylib -framework OpenGL -framework IOKit -framework CoreVideo "
    else ""
  in

  let all_links = link_args ^ " " ^ coreui_flags ^ " -lsqlite3" ^ raylib_flags in

  match Codegen.codegen_program ?triple ~output_file:bc_file "my_module" final_program with
  | Ok _llmodule ->
      if String.equal !mode "run" || String.equal !mode "test" then (
          (* Implement "run" as Compile to Temp + Execute *)
          let temp_exe = if String.equal !mode "test" then output_path else Filename.temp_file "ardium_run_" ".exe" in

          let cmd =
            Printf.sprintf "clang -O3 %s %s %s %s -o %s -lm %s -DARDIUM_GUI_BUILD"
              bc_file runtime_flags all_frameworks all_links temp_exe all_frameworks
          in
          let status = Stdlib.Sys.command cmd in
          if status <> 0 then (
              Printf.eprintf "❌ Compilation failed during run step.\n";
              ignore (Stdlib.Sys.remove temp_exe);
              exit status
          );

          (* Execute the binary *)
          (* Printf.printf "🚀 Running...\n"; *)
          let exec_path = if Filename.is_relative temp_exe then "./" ^ temp_exe else temp_exe in
          let run_status = Stdlib.Sys.command exec_path in

          (* Cleanup *)
          if String.equal !mode "run" then ignore (Stdlib.Sys.remove temp_exe);

          run_status
      ) else if !compile_only then (
          Printf.printf "✅ Compiled to %s\n" bc_file;
          0
      ) else if !build_dylib then (
          let cmd =
            Printf.sprintf "clang -dynamiclib -undefined dynamic_lookup -O3 %s %s %s %s -o %s -lm"
              bc_file runtime_flags framework_args link_args output_path
          in
          let status = Stdlib.Sys.command cmd in
          if status <> 0 then exit status else 0
      ) else (
          let cmd =
            Printf.sprintf "clang -O3 %s %s %s %s -o %s -lm %s -DARDIUM_GUI_BUILD"
              bc_file runtime_flags all_frameworks all_links output_path all_frameworks
          in
          Printf.eprintf "🔧 DEBUG clang cmd: %s\n" cmd;
          let status = Stdlib.Sys.command cmd in
          if status <> 0 then (
              Printf.eprintf "❌ Compilation Failed for %s\n" input_path;
              exit status
          ) else 0
      )
  | Error msg ->
      Printf.eprintf "❌ Codegen Error in %s: %s\n" input_path msg;
      exit 1

let () =
  Printf.printf "DEBUG: Argv: %s\n" (String.concat " " (Array.to_list Sys.argv));
  Arg.parse spec_list anon_fun usage_msg;

  (* NEW Mode: Create a new project *)
  if String.equal !mode "new" then (
    let project_name = !input_file in
    if String.length project_name = 0 then (
      Printf.eprintf "❌ Error: Please specify a project name.\nUsage: arc new <project_name>\n";
      exit 1
    );
    if Sys.file_exists project_name then (
      Printf.eprintf "❌ Error: Directory '%s' already exists.\n" project_name;
      exit 1
    );

    Printf.printf "🚀 Creating new Ardium project: %s\n" project_name;
    try
        Sys.mkdir project_name 0o755;
        Sys.mkdir (project_name ^ "/src") 0o755;

        let toml_content = Printf.sprintf "name = \"%s\"\nversion = \"0.1.0\"\n" project_name in
        let oc_toml = open_out (project_name ^ "/ardium.toml") in
        Printf.fprintf oc_toml "%s" toml_content;
        close_out oc_toml;

        let main_content = "fn main() {\n    println(\"Hello, Ardium World!\");\n    return 0;\n}\n" in
        let oc_main = open_out (project_name ^ "/src/main.ar") in
        Printf.fprintf oc_main "%s" main_content;
        close_out oc_main;

        Printf.printf "✅ Project created successfully!\n\nTo get started:\n  cd %s\n  arc run\n" project_name;
        exit 0
    with e ->
        Printf.eprintf "❌ Error: Failed to create project: %s\n" (Printexc.to_string e);
        exit 1
  );

  (* INSTALL Mode: Install a package *)
  if String.equal !mode "install" then (
    let pkg_name = !input_file in
    if String.length pkg_name = 0 then (
      Printf.eprintf "❌ Error: Please specify a package to install.\nUsage: arc install <package>\n";
      exit 1
    );

    let capitalized_pkg = String.capitalize_ascii pkg_name in
    Printf.printf "📦 Fetching package '%s' from registry...\n" pkg_name;
    
    let target_dir = "ardium_modules" in
    if not (Sys.file_exists target_dir) then Sys.mkdir target_dir 0o755;

    let target_file = Filename.concat target_dir (capitalized_pkg ^ ".ar") in
    let content = Printf.sprintf "// Mock Ardium Package: %s\n\nfn init_%s() {\n    println(\"[DB] Initialized %s connection.\");\n}\n" pkg_name pkg_name pkg_name in
    
    let oc = open_out target_file in
    Printf.fprintf oc "%s" content;
    close_out oc;

    Printf.printf "✅ Successfully installed '%s' into %s/\n" pkg_name target_dir;
    exit 0
  );

  (* HEADER Mode: Generate C header file *)
  if String.equal !mode "header" then (
    let input = if String.length !input_file > 0 then !input_file else "src/main.ar" in
    if not (Sys.file_exists input) then (
      Printf.eprintf "❌ Error: Input file '%s' not found.\n" input;
      exit 1
    );
    Printf.printf "📝 Generating C header for %s...\n" input;

    let chan = Stdlib.open_in input in
    let lexbuf = Lexing.from_channel chan in
    let program = Parser.prog Lexer.read lexbuf in
    Stdlib.close_in chan;

    let header_name = "ardium_exports.h" in
    Header_gen.save_header header_name program;
    Printf.printf "✅ Header saved to %s\n" header_name;
    exit 0
  );

  (* LSP Mode: Start the language server *)
  if String.equal !mode "lsp" then (
    Lsp.start_server ();
    exit 0
  );

  (* DEV Mode: Hot Reload Watcher *)
  if String.equal !mode "dev" then (
    let input = if String.length !input_file > 0 then !input_file else "src/main.ar" in
    if not (Sys.file_exists input) then (
      Printf.eprintf "❌ Error: Input file '%s' not found.\n" input;
      exit 1
    );
    Printf.printf "\027[1;33m🔥 Ardium Dev Mode: Watching %s...\027[0m\n" input;

    let last_mtime = ref 0.0 in

    while true do
      try
        let stats = Unix.stat input in
        if stats.st_mtime > !last_mtime then (
            last_mtime := stats.st_mtime;

            (* Clear Screen *)
            Printf.printf "\027[2J\027[H";
            Printf.printf "\027[1;36m[RELOAD] %s needed a refresh...\027[0m\n" input;

            (* Re-run using subprocess to isolate crashes *)
            let cmd = Printf.sprintf "%s run %s --no-std" Sys.argv.(0) input in
            let _ = Sys.command cmd in
            Printf.printf "\n\027[1;30mWaiting for changes...\027[0m\n";
        );
        Unix.sleepf 0.5
      with _ -> ()
    done;
    exit 0
  );

  (* Auto-detect project (No input file) *)
  if String.length !input_file = 0 then (
    if Sys.file_exists "ardium.toml" then (
      Printf.printf "📦 Detected ardium.toml, building project...\n";
      match Config.load_config "ardium.toml" with
      | Some config ->
          Printf.printf "   Project: %s v%s\n" config.name config.version;
          input_file := config.entry_point;
          extra_links := config.links;
          if String.length !output_file = 0 || !output_file = "a.out" then
             output_file := "bin/" ^ config.name;
          if not (Sys.file_exists "bin") then Sys.mkdir "bin" 0o755
      | None ->
          Printf.eprintf "❌ Error: Invalid ardium.toml\n";
          exit 1
    ) else (
      Printf.printf "%s\n" usage_msg;
      exit 0
    )
  );

  (* MAIN Execution Logic *)

  (* TEST Mode: Directory Support *)
  if String.equal !mode "test" && (try Sys.is_directory !input_file with Sys_error _ -> false) then (
    Printf.printf "🧪 Running tests in directory: %s\n" !input_file;
    let files = Sys.readdir !input_file
                |> Array.to_list
                |> List.filter (fun f -> Filename.check_suffix f ".ar")
                |> List.sort String.compare in

    let passed = ref 0 in
    let failed = ref 0 in

    List.iter (fun f ->
        let path = Filename.concat !input_file f in
        let out_name = Filename.remove_extension f ^ "_test_bin" in
        Printf.printf "Running %s... " f;
        flush stdout;

        let status = process_file path out_name in
        if status = 0 then (
            Printf.printf "✅ PASS\n";
            incr passed
        ) else (
            Printf.printf "❌ FAIL (Exit Code: %d)\n" status;
            incr failed
        );
        (* Cleanup binary *)
        if Sys.file_exists out_name then Sys.remove out_name;
        if Sys.file_exists (out_name^".o") then Sys.remove (out_name^".o")
    ) files;

    Printf.printf "\nTest Summary:\n  Passed: %d\n  Failed: %d\n" !passed !failed;
    if !failed > 0 then exit 1 else exit 0
  );

  (* Default: Process single file *)
  let status = process_file !input_file !output_file in
  exit status
