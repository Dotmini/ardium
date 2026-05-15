(* lib/config.ml - Project Configuration Manager *)

type project_config = {
  name: string;
  version: string;
  entry_point: string;
  links: string list;
}

let default_config name = {
  name = name;
  version = "2.3.4";
  entry_point = "src/main.ar";
  links = [];
}

(* Simple TOML parser (subset) just for MVP *)
let parse_toml content =
  let lines = String.split_on_char '\n' content in
  let name = ref "untitled" in
  let version = ref "2.3.4" in
  let links = ref [] in
  List.iter (fun line ->
    let line = String.trim line in
    if String.starts_with ~prefix:"name" line then
      match String.split_on_char '"' line with
      | _ :: n :: _ -> name := n
      | _ -> ()
    else if String.starts_with ~prefix:"version" line then
      match String.split_on_char '"' line with
      | _ :: v :: _ -> version := v
      | _ -> ()
    else if String.starts_with ~prefix:"links" line then
      (* Basic parsing for links = ["a", "b"] *)
      try
        let start_idx = String.index line '[' in
        let end_idx = String.index line ']' in
        let content = String.sub line (start_idx + 1) (end_idx - start_idx - 1) in
        let libs = String.split_on_char ',' content 
                   |> List.map String.trim 
                   |> List.map (fun s -> String.sub s 1 (String.length s - 2)) (* remove quotes *)
        in
        links := libs
      with _ -> ()
  ) lines;
  { name = !name; version = !version; entry_point = "src/main.ar"; links = !links }

let load_config path =
  if Sys.file_exists path then
    let ic = open_in path in
    let content = really_input_string ic (in_channel_length ic) in
    close_in ic;
    Some (parse_toml content)
  else
    None
