

module Encoding : sig
  type t = ASCII | UTF8 | UTF16 | UTF32 | CHAR_WIDTH_1 | CHAR_WIDTH_2 | CHAR_WIDTH_4
end

type runtime_model

val build_exportable_model : encoding:Encoding.t -> ?nb_runtime_threads:int -> (string * float option) list -> string

val build_runtime_model : string -> runtime_model

val jaro_winkler_distance : encoding:Encoding.t -> ?min_score:float -> ?weight:float -> ?threshold:float -> ?n_best_results:int option -> runtime_model -> string -> (string * float) list

val jaro_distance : encoding:Encoding.t -> ?min_score:float -> ?n_best_results:int option -> runtime_model -> string -> (string * float) list
