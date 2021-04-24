

let run_jaro ?min_score:(min_score=None) n_best_results nb_runtime_threads candidates inp =
  let min_score = match min_score with
    | None -> -(1.0)
    | Some x -> x
  in
  let encoding = Batch_jaro_winkler.Encoding.UTF8 in
  let exportable_model = Batch_jaro_winkler.build_exportable_model ~encoding ~nb_runtime_threads candidates in
  let runtime_model = Batch_jaro_winkler.build_runtime_model exportable_model in
  let results = Batch_jaro_winkler.jaro_winkler_distance ~encoding ~min_score ~n_best_results runtime_model inp in
  List.sort (fun (cand1, _) (cand2, _) -> String.compare cand1 cand2) results

let default_candidates = [
  ("hélloz", None) ;
  ("中国", None) ;
  ("lolz", None) ;
  ("hii", None)
]

let test_candidates_results = [
  ("hii", 0.5) ;
  ("hélloz", 1.0) ;
  ("lolz", 0.75) ;
  ("中国", 0.0)
]

let tests = [
  ("no_candidates", fun nb_runtime_threads -> (
    let candidates = [] in
    let res = run_jaro None nb_runtime_threads candidates "hi" in
    assert (res = [])
  )) ; ("no_candidates_empty_input", fun nb_runtime_threads -> (
    let candidates = [] in
    let res = run_jaro None nb_runtime_threads candidates "" in
    assert (res = [])
  )) ; ("one_empty_candidate", fun nb_runtime_threads -> (
    let candidates = [("", None)] in
    let res = run_jaro None nb_runtime_threads candidates "hi" in
    assert (res = [("", 0.0)])
  )) ; ("one_empty_candidate_and_input", fun nb_runtime_threads -> (
    let candidates = [("", None)] in
    let res = run_jaro None nb_runtime_threads candidates "" in
    assert (res = [("", 0.0)])
  )) ; ("one_perfect_match", fun nb_runtime_threads -> (
    let candidates = [("hélloz", None)] in
    let res = run_jaro None nb_runtime_threads candidates "hélloz" in
    assert (res = [("hélloz", 1.0)])
  )) ; ("multiple_matches", fun nb_runtime_threads -> (
    let candidates = default_candidates in
    let res = run_jaro None nb_runtime_threads candidates "hélloz" in
    assert (res = test_candidates_results)
  )) ; ("min_scores_all_ok", fun nb_runtime_threads -> (
    let candidates =  [
      ("hélloz", Some 0.9) ;
      ("中国", Some 0.0) ;
      ("lolz", Some 0.7) ;
      ("hii", Some 0.4)
    ] in
    let res = run_jaro None nb_runtime_threads candidates "hélloz" in
    assert (res = test_candidates_results)
  )) ; ("min_scores_all_ok_exact", fun nb_runtime_threads -> (
    let candidates =  [
      ("hélloz", Some 1.0) ;
      ("中国", Some 0.0) ;
      ("lolz", Some 0.75) ;
      ("hii", Some 0.5)
    ] in
    let res = run_jaro None nb_runtime_threads candidates "hélloz" in
    assert (res = test_candidates_results)
  )) ; ("min_scores_some_filtered", fun nb_runtime_threads -> (
    let candidates =  [
      ("hélloz", Some 1.0) ;
      ("中国", Some 0.0) ;
      ("lolz", Some 0.750001) ;
      ("hii", Some 0.500001)
    ] in
    let res = run_jaro None nb_runtime_threads candidates "hélloz" in
    assert (res = [("hélloz", 1.0) ; ("中国", 0.0)])
  )) ; ("min_scores_all_filtered", fun nb_runtime_threads -> (
    let candidates =  [
      ("中国", Some 0.000001) ;
      ("lolz", Some 0.750001) ;
      ("hii", Some 0.500001)
    ] in
    let res = run_jaro None nb_runtime_threads candidates "hélloz" in
    assert (res = [])
  )) ; ("global_min_score_all_ok", fun nb_runtime_threads -> (
    let candidates = default_candidates in
    let res = run_jaro ~min_score:(Some 0.0) None nb_runtime_threads candidates "hélloz" in
    assert (res = test_candidates_results)
  )) ; ("global_min_score_some_filtered", fun nb_runtime_threads -> (
    let candidates = default_candidates in
    let res = run_jaro ~min_score:(Some 0.5) None nb_runtime_threads candidates "hélloz" in
    assert (res = [("hii", 0.5) ; ("hélloz", 1.0) ; ("lolz", 0.75)])
  )) ; ("global_min_score_some_filtered2", fun nb_runtime_threads -> (
    let candidates = default_candidates in
    let res = run_jaro ~min_score:(Some 0.500001) None nb_runtime_threads candidates "hélloz" in
    assert (res = [("hélloz", 1.0) ; ("lolz", 0.75)])
  )) ; ("global_min_score_all_filtered", fun nb_runtime_threads -> (
    let candidates = [
      ("中国", None) ;
      ("lolz", None) ;
      ("hii", None)
    ] in
    let res = run_jaro ~min_score:(Some 0.8) None nb_runtime_threads candidates "hélloz" in
    assert (res = [])
  )) ; ("global_min_score_ovveride_min_scores", fun nb_runtime_threads -> (
    let candidates = [
      ("hélloz", Some 1.0) ;
      ("中国", Some 0.0) ;
      ("lolz", Some 0.750001) ;
      ("hii", Some 0.500001)
    ] in
    let res = run_jaro ~min_score:(Some 0.75) None nb_runtime_threads candidates "hélloz" in
    assert (res = [("hélloz", 1.0) ; ("lolz", 0.75)])
  )) ; ("n_best_results_zero", fun nb_runtime_threads -> (
    let candidates = default_candidates in
    let res = run_jaro (Some 0) nb_runtime_threads candidates "hélloz" in
    assert (res = [])
  )) ; ("n_best_results_too_big", fun nb_runtime_threads -> (
    let candidates = default_candidates in
    let res = run_jaro (Some 5) nb_runtime_threads candidates "hélloz" in
    assert (res = test_candidates_results)
  )) ; ("n_best_results_all", fun nb_runtime_threads -> (
    let candidates = default_candidates in
    let res = run_jaro (Some 4) nb_runtime_threads candidates "hélloz" in
    assert (res = test_candidates_results)
  )) ; ("n_best_results_some_filtered", fun nb_runtime_threads -> (
    let candidates = default_candidates in
    let res = run_jaro (Some 3) nb_runtime_threads candidates "hélloz" in
    assert (res = [("hii", 0.5) ; ("hélloz", 1.0) ; ("lolz", 0.75)])
  )) ; ("n_best_results_some_filtered2", fun nb_runtime_threads -> (
    let candidates = default_candidates in
    let res = run_jaro (Some 2) nb_runtime_threads candidates "hélloz" in
    assert (res = [("hélloz", 1.0) ; ("lolz", 0.75)])
  )) ; ("n_best_results_respect_min_scores", fun nb_runtime_threads -> (
    let candidates = [
      ("hélloz", Some 1.0) ;
      ("中国", Some 0.0) ;
      ("lolz", Some 0.750001) ;
      ("hii", Some 0.5)
    ] in
    let res = run_jaro (Some 2) nb_runtime_threads candidates "hélloz" in
    assert (res = [("hii", 0.5) ; ("hélloz", 1.0)])
  )) ; ("n_best_results_respect_min_scores2", fun nb_runtime_threads -> (
    let candidates = [
      ("hélloz", Some 1.0) ;
      ("中国", Some 0.0) ;
      ("lolz", Some 0.75) ;
      ("hii", Some 0.5)
    ] in
    let res = run_jaro (Some 2) nb_runtime_threads candidates "hélloz" in
    assert (res = [("hélloz", 1.0) ; ("lolz", 0.75)])
  )) ; ("n_best_results_respect_min_score", fun nb_runtime_threads -> (
    let candidates = [
      ("hélloz", Some 1.0) ;
      ("中国", Some 0.0) ;
      ("lolz", Some 0.750001) ;
      ("hii", Some 0.5)
    ] in
    let res = run_jaro ~min_score:(Some 0.7501) (Some 2) nb_runtime_threads candidates "hélloz" in
    assert (res = [("hélloz", 1.0)])
  )) ; ("n_best_results_respect_min_score2", fun nb_runtime_threads -> (
    let candidates = [
      ("hélloz", Some 1.0) ;
      ("中国", Some 0.0) ;
      ("lolz", Some 0.750001) ;
      ("hii", Some 0.5)
    ] in
    let res = run_jaro ~min_score:(Some 0.75) (Some 2) nb_runtime_threads candidates "hélloz" in
    assert (res = [("hélloz", 1.0) ; ("lolz", 0.75)])
  )) ; ("n_best_results_respect_min_score3", fun nb_runtime_threads -> (
    let candidates = [
      ("hélloz", Some 1.0) ;
      ("中国", Some 0.0) ;
      ("lolz", Some 0.750001) ;
      ("hii", Some 0.5)
    ] in
    let res = run_jaro ~min_score:(Some 0.0) (Some 2) nb_runtime_threads candidates "hélloz" in
    assert (res = [("hélloz", 1.0) ; ("lolz", 0.75)])
  )) ; ("long_candidate", fun nb_runtime_threads -> (
    let long_candidate = String.make (256 * 128) 'b' in
    let normal_candidate = String.make 11 'a' in
    let candidates = [
      (normal_candidate, None) ;
      (long_candidate, None)
    ] in
    let res = run_jaro ~min_score:(Some 0.9) None nb_runtime_threads candidates normal_candidate in
    assert (res = [(normal_candidate, 1.0)])
  )) ; ("long_candidate2", fun nb_runtime_threads -> (
    let long_candidate = String.make (256 * 128) 'b' in
    let normal_candidate = String.make 11 'a' in
    let candidates = [
      (normal_candidate, None) ;
      (long_candidate, None)
    ] in
    let res = run_jaro ~min_score:(Some 0.9) None nb_runtime_threads candidates long_candidate in
    assert (res = [(long_candidate, 1.0)])
  ))
]

let run () =
  let nb_runtime_threads = [1; 2; 3; 4; 5; 6] in
  List.iter (fun (fname, func) -> List.iter (fun nb_runtime_t -> Printf.printf "- %s %d\n" fname nb_runtime_t ; func nb_runtime_t) nb_runtime_threads) tests ;
  print_endline "All ok"
