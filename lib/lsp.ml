(* lib/lsp.ml - Production Language Server Protocol for Ardium *)

(* ============================================================
   JSON-RPC Protocol Helpers
   ============================================================ *)

let send_json json =
  let len = String.length json in
  Printf.printf "Content-Length: %d\r\n\r\n%s%!" len json

let escape_json_string s =
  let buf = Buffer.create (String.length s * 2) in
  String.iter (fun c ->
    match c with
    | '"' -> Buffer.add_string buf "\\\""
    | '\\' -> Buffer.add_string buf "\\\\"
    | '\n' -> Buffer.add_string buf "\\n"
    | '\r' -> Buffer.add_string buf "\\r"
    | '\t' -> Buffer.add_string buf "\\t"
    | c -> Buffer.add_char buf c
  ) s;
  Buffer.contents buf

(* ============================================================
   Document Management
   ============================================================ *)

(* In-memory store of open documents: uri -> content *)
let documents : (string, string) Hashtbl.t = Hashtbl.create 16

let get_document uri = Hashtbl.find_opt documents uri
let set_document uri content = Hashtbl.replace documents uri content
let remove_document uri = Hashtbl.remove documents uri

(* ============================================================
   Diagnostics Generation
   ============================================================ *)

type diagnostic = {
  line: int;
  col_start: int;
  col_end: int;
  severity: int; (* 1=Error, 2=Warning, 3=Info, 4=Hint *)
  message: string;
}

let diagnostic_to_json d =
  Printf.sprintf
    "{\"range\": {\"start\": {\"line\": %d, \"character\": %d}, \"end\": {\"line\": %d, \"character\": %d}}, \"severity\": %d, \"source\": \"ardium\", \"message\": \"%s\"}"
    d.line d.col_start d.line d.col_end d.severity (escape_json_string d.message)

let parse_and_diagnose content =
  let lexbuf = Lexing.from_string content in
  try
    let _ = Parser.prog Lexer.read lexbuf in
    [] (* No errors *)
  with
  | Parser.Error ->
      let pos = lexbuf.Lexing.lex_curr_p in
      let line = pos.Lexing.pos_lnum - 1 in (* LSP is 0-indexed *)
      let col = pos.Lexing.pos_cnum - pos.Lexing.pos_bol in
      [{ line; col_start = col; col_end = col + 1; severity = 1; message = "Syntax error: unexpected token" }]
  | Failure msg ->
      let pos = lexbuf.Lexing.lex_curr_p in
      let line = pos.Lexing.pos_lnum - 1 in
      let col = pos.Lexing.pos_cnum - pos.Lexing.pos_bol in
      [{ line; col_start = col; col_end = col + 10; severity = 1; message = msg }]
  | e ->
      [{ line = 0; col_start = 0; col_end = 1; severity = 1; message = Printexc.to_string e }]

let publish_diagnostics uri diagnostics =
  let diag_json = diagnostics |> List.map diagnostic_to_json |> String.concat ", " in
  let json = Printf.sprintf
    "{\"jsonrpc\": \"2.0\", \"method\": \"textDocument/publishDiagnostics\", \"params\": {\"uri\": \"%s\", \"diagnostics\": [%s]}}"
    uri diag_json
  in
  send_json json

(* ============================================================
   Symbol Table for Go-to-Definition
   ============================================================ *)

type symbol_location = {
  sym_uri: string;
  sym_line: int;
  sym_col: int;
}

let symbol_table : (string, symbol_location) Hashtbl.t = Hashtbl.create 64

let build_symbol_table uri content =
  Hashtbl.clear symbol_table;
  let lines = String.split_on_char '\n' content in
  List.iteri (fun line_num line ->
    (* Simple regex-like scan for fn NAME( *)
    let len = String.length line in
    let rec scan i =
      if i + 3 < len then
        if String.sub line i 3 = "fn " then
          let name_start = i + 3 in
          let rec find_end j =
            if j >= len then j
            else match line.[j] with
              | '(' | ' ' | '\t' -> j
              | _ -> find_end (j + 1)
          in
          let name_end = find_end name_start in
          if name_end > name_start then begin
            let name = String.sub line name_start (name_end - name_start) in
            Hashtbl.replace symbol_table name { sym_uri = uri; sym_line = line_num; sym_col = name_start }
          end;
          scan (name_end + 1)
        else
          scan (i + 1)
      else ()
    in
    scan 0
  ) lines

let find_word_at content line col =
  let lines = String.split_on_char '\n' content in
  if line >= 0 && line < List.length lines then
    let ln = List.nth lines line in
    let len = String.length ln in
    if col >= 0 && col < len then
      (* Find word boundaries *)
      let is_ident c = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c = '_' in
      let rec find_start i = if i <= 0 || not (is_ident ln.[i-1]) then i else find_start (i-1) in
      let rec find_end i = if i >= len || not (is_ident ln.[i]) then i else find_end (i+1) in
      let s = find_start col in
      let e = find_end col in
      if e > s then Some (String.sub ln s (e - s)) else None
    else None
  else None

(* ============================================================
   JSON Parsing Helpers (Simple extraction)
   ============================================================ *)

let extract_string_field json field =
  let pattern = Printf.sprintf "\"%s\":\"" field in
  try
    let start = Str.search_forward (Str.regexp_string pattern) json 0 in
    let value_start = start + String.length pattern in
    let value_end = String.index_from json value_start '"' in
    Some (String.sub json value_start (value_end - value_start))
  with Not_found -> None

let extract_int_field json field =
  let pattern = Printf.sprintf "\"%s\":" field in
  try
    let start = Str.search_forward (Str.regexp_string pattern) json 0 in
    let value_start = start + String.length pattern in
    let rec find_num_end i =
      if i >= String.length json then i
      else match json.[i] with
        | '0'..'9' -> find_num_end (i+1)
        | _ -> i
    in
    let value_end = find_num_end value_start in
    if value_end > value_start then
      Some (int_of_string (String.sub json value_start (value_end - value_start)))
    else None
  with Not_found | Failure _ -> None

let extract_text_content json =
  (* Find text field and extract the value *)
  extract_string_field json "text"

(* ============================================================
   Request Handlers
   ============================================================ *)

let handle_initialize id =
  let capabilities = {|{
    "textDocumentSync": 1,
    "definitionProvider": true,
    "hoverProvider": true,
    "completionProvider": {"triggerCharacters": [".", "("]},
    "diagnosticProvider": {"interFileDependencies": false, "workspaceDiagnostics": false}
  }|} in
  let result = Printf.sprintf
    "{\"jsonrpc\": \"2.0\", \"id\": %d, \"result\": {\"capabilities\": %s, \"serverInfo\": {\"name\": \"ardium-lsp\", \"version\": \"1.0.0\"}}}"
    id capabilities
  in
  send_json result

let handle_initialized () =
  (* Client confirms initialization - nothing to do *)
  ()

let handle_shutdown id =
  send_json (Printf.sprintf "{\"jsonrpc\": \"2.0\", \"id\": %d, \"result\": null}" id)

let handle_did_open params_json =
  match extract_string_field params_json "uri" with
  | Some uri ->
      (match extract_text_content params_json with
       | Some text ->
           set_document uri text;
           build_symbol_table uri text;
           let diags = parse_and_diagnose text in
           publish_diagnostics uri diags
       | None -> ())
  | None -> ()

let handle_did_change params_json =
  match extract_string_field params_json "uri" with
  | Some uri ->
      (* For incremental sync, we'd need to apply changes. For full sync, find the new text. *)
      (match extract_text_content params_json with
       | Some text ->
           set_document uri text;
           build_symbol_table uri text;
           let diags = parse_and_diagnose text in
           publish_diagnostics uri diags
       | None -> ())
  | None -> ()

let handle_did_close params_json =
  match extract_string_field params_json "uri" with
  | Some uri -> remove_document uri
  | None -> ()

let handle_definition id params_json =
  match extract_string_field params_json "uri" with
  | Some uri ->
      (match get_document uri with
       | Some content ->
           let line = extract_int_field params_json "line" |> Option.value ~default:0 in
           let col = extract_int_field params_json "character" |> Option.value ~default:0 in
           (match find_word_at content line col with
            | Some word ->
                (match Hashtbl.find_opt symbol_table word with
                 | Some loc ->
                     let result = Printf.sprintf
                       "{\"jsonrpc\": \"2.0\", \"id\": %d, \"result\": {\"uri\": \"%s\", \"range\": {\"start\": {\"line\": %d, \"character\": %d}, \"end\": {\"line\": %d, \"character\": %d}}}}"
                       id loc.sym_uri loc.sym_line loc.sym_col loc.sym_line (loc.sym_col + String.length word)
                     in
                     send_json result
                 | None ->
                     send_json (Printf.sprintf "{\"jsonrpc\": \"2.0\", \"id\": %d, \"result\": null}" id))
            | None ->
                send_json (Printf.sprintf "{\"jsonrpc\": \"2.0\", \"id\": %d, \"result\": null}" id))
       | None ->
           send_json (Printf.sprintf "{\"jsonrpc\": \"2.0\", \"id\": %d, \"result\": null}" id))
  | None ->
      send_json (Printf.sprintf "{\"jsonrpc\": \"2.0\", \"id\": %d, \"result\": null}" id)

let handle_hover id params_json =
  match extract_string_field params_json "uri" with
  | Some uri ->
      (match get_document uri with
       | Some content ->
           let line = extract_int_field params_json "line" |> Option.value ~default:0 in
           let col = extract_int_field params_json "character" |> Option.value ~default:0 in
           (match find_word_at content line col with
            | Some word ->
                (match Hashtbl.find_opt symbol_table word with
                 | Some _ ->
                     let hover_content = Printf.sprintf "```ardium\\nfn %s(...)\\n```\\n\\n*Ardium function*" word in
                     let result = Printf.sprintf
                       "{\"jsonrpc\": \"2.0\", \"id\": %d, \"result\": {\"contents\": {\"kind\": \"markdown\", \"value\": \"%s\"}}}"
                       id (escape_json_string hover_content)
                     in
                     send_json result
                 | None ->
                     send_json (Printf.sprintf "{\"jsonrpc\": \"2.0\", \"id\": %d, \"result\": null}" id))
            | None ->
                send_json (Printf.sprintf "{\"jsonrpc\": \"2.0\", \"id\": %d, \"result\": null}" id))
       | None ->
           send_json (Printf.sprintf "{\"jsonrpc\": \"2.0\", \"id\": %d, \"result\": null}" id))
  | None ->
      send_json (Printf.sprintf "{\"jsonrpc\": \"2.0\", \"id\": %d, \"result\": null}" id)

(* ============================================================
   Main Request Router
   ============================================================ *)

let handle_request method_name id params_json =
  match method_name with
  | "initialize" -> handle_initialize id
  | "initialized" -> handle_initialized ()
  | "shutdown" -> handle_shutdown id
  | "textDocument/didOpen" -> handle_did_open params_json
  | "textDocument/didChange" -> handle_did_change params_json
  | "textDocument/didClose" -> handle_did_close params_json
  | "textDocument/definition" -> handle_definition id params_json
  | "textDocument/hover" -> handle_hover id params_json
  | _ -> () (* Ignore unknown methods *)

(* ============================================================
   Server Main Loop
   ============================================================ *)

let start_server () =
  Printf.eprintf "🚀 Ardium LSP Server v1.0.0 Started\n%!";
  let rec loop () =
    try
      let line = input_line stdin in
      if String.length line >= 16 && String.sub line 0 16 = "Content-Length: " then begin
        let len = int_of_string (String.trim (String.sub line 16 (String.length line - 16))) in
        ignore (input_line stdin); (* skip empty line *)
        let body = Bytes.create len in
        really_input stdin body 0 len;
        let body_str = Bytes.to_string body in
        
        (* Extract method and id from JSON *)
        let method_name = extract_string_field body_str "method" |> Option.value ~default:"unknown" in
        let id = extract_int_field body_str "id" |> Option.value ~default:0 in
        
        handle_request method_name id body_str;
        loop ()
      end else
        loop ()
    with
    | End_of_file -> Printf.eprintf "LSP: Connection closed\n%!"
    | e -> Printf.eprintf "LSP Error: %s\n%!" (Printexc.to_string e); loop ()
  in
  loop ()
