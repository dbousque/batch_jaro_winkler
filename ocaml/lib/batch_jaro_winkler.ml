

module Encoding = struct
  type t = ASCII | UTF8 | UTF16 | UTF32 | CHAR_WIDTH_1 | CHAR_WIDTH_2 | CHAR_WIDTH_4

  let to_string = function
    | ASCII -> "ascii"
    | UTF8 -> "utf8"
    | UTF16 -> "utf16"
    | UTF32 -> "utf32"
    | CHAR_WIDTH_1 -> "char_width_1"
    | CHAR_WIDTH_2 -> "char_width_2"
    | CHAR_WIDTH_4 -> "char_width_4"
end

type runtime_model = { exportable_model: string ; runtime_model_ptr: string ; released: bool ref }

external caml_c_build_exportable_model: string -> int -> (string * float option) list -> string = "caml_c_build_exportable_model"
external caml_c_build_runtime_model: string -> string = "caml_c_build_runtime_model"
external caml_c_free_runtime_model: string -> unit = "caml_c_free_runtime_model"
external caml_c_jaro_winkler_distance: string -> float -> (float option * float option * int option) -> runtime_model -> string -> (string * float) list = "caml_c_jaro_winkler_distance"

let build_exportable_model ~encoding ?nb_runtime_threads:(nb_runtime_threads=1) candidates =
  if nb_runtime_threads < 1 then
    raise (Invalid_argument "nb_runtime_threads must be > 0")
  else
    caml_c_build_exportable_model (Encoding.to_string encoding) nb_runtime_threads candidates

(* Shouldn't be called more than once, but we use 'released' to be safe *)
let finalize_runtime_model { runtime_model_ptr ; released ; _ } =
  match !released with
    | false -> (
      released := true ;
      caml_c_free_runtime_model runtime_model_ptr
    )
    | _ -> ()

let build_runtime_model exportable_model =
  let runtime_model_ptr = caml_c_build_runtime_model exportable_model in
  (* We keep a reference to the exportable model because we will need it at runtime *)
  let runtime_model = { exportable_model ; runtime_model_ptr ; released = ref false } in
  Gc.finalise finalize_runtime_model runtime_model ;
  runtime_model

let jaro_winkler_distance ~encoding ?min_score:(min_score=(-1.0)) ?weight:(weight=0.1) ?threshold:(threshold=0.7) ?n_best_results:(n_best_results=None) runtime_model input =
  if min_score <> -1.0 && (min_score < 0.0 || min_score > 1.0) then
    raise (Invalid_argument "min_score must be >= 0.0 and <= 1.0")
  else if weight < 0.0 || weight > 0.25 then
    raise (Invalid_argument "weight must be >= 0.0 and <= 0.25")
  else if threshold < 0.0 || threshold > 1.0 then
    raise (Invalid_argument "threshold must be >= 0.0 and <= 1.0")
  else
    match n_best_results with
    | Some 0 -> []
    | Some x when x < 0 -> raise (Invalid_argument "n_best_results must be >= 0")
    | _ -> (
      (* We pass the entire runtime model with the exportable model to make sure it is not garbage collected *)
      caml_c_jaro_winkler_distance (Encoding.to_string encoding) min_score ((Some weight), (Some threshold), n_best_results) runtime_model input
    )

let jaro_distance ~encoding ?min_score:(min_score=(-1.0)) ?n_best_results:(n_best_results=None) runtime_model input =
  if min_score <> -1.0 && (min_score < 0.0 || min_score > 1.0) then
    raise (Invalid_argument "min_score must be >= 0.0 and <= 1.0")
  else
    match n_best_results with
    | Some 0 -> []
    | Some x when x < 0 -> raise (Invalid_argument "n_best_results must be >= 0")
    | _ -> (
      (* We pass the entire runtime model with the exportable model to make sure it is not garbage collected *)
      caml_c_jaro_winkler_distance (Encoding.to_string encoding) min_score (None, None, n_best_results) runtime_model input
    )
