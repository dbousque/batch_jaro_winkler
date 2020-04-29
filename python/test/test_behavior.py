

import pytest

import batch_jaro_winkler as bjw

def run_jaro(candidates, inp, as_bytes, nb_runtime_threads, min_score=None, n_best_results=None):
  if as_bytes:
    with_min_scores = len(candidates) > 0 and type(candidates[0]) is dict
    if with_min_scores:
      for candidate in candidates:
        candidate['candidate'] = candidate['candidate'].encode('utf-16-le')
    else:
      candidates = list(map(lambda x: x.encode('utf-16-le'), candidates))
    exportable_model = bjw.build_exportable_model_bytes(2, candidates, nb_runtime_threads=nb_runtime_threads)
  else:
    exportable_model = bjw.build_exportable_model(candidates, nb_runtime_threads=nb_runtime_threads)
  runtime_model = bjw.build_runtime_model(exportable_model)
  if as_bytes:
    inp = inp.encode('utf-16-le')
    res = bjw.jaro_distance_bytes(2, runtime_model, inp, min_score=min_score, n_best_results=n_best_results)
    for i_res, r in enumerate(res):
      res[i_res] = (r[0].decode('utf-16-le'), r[1])
  else:
    res = bjw.jaro_distance(runtime_model, inp, min_score=min_score, n_best_results=n_best_results)
  # Sort to make results order deterministic
  res.sort(key=lambda r: r[0])
  return res

as_bytes = [False, True]
nb_runtime_threads = [1, 2, 3, 4, 5, 6]

@pytest.mark.parametrize('as_bytes', as_bytes)
@pytest.mark.parametrize('nb_runtime_threads', nb_runtime_threads)
def test_no_candidates(as_bytes, nb_runtime_threads):
  candidates = []
  assert run_jaro(candidates, 'hi', as_bytes, nb_runtime_threads) == []

@pytest.mark.parametrize('as_bytes', as_bytes)
@pytest.mark.parametrize('nb_runtime_threads', nb_runtime_threads)
def test_no_candidates_empty_input(as_bytes, nb_runtime_threads):
  candidates = []
  assert run_jaro(candidates, '', as_bytes, nb_runtime_threads) == []

@pytest.mark.parametrize('as_bytes', as_bytes)
@pytest.mark.parametrize('nb_runtime_threads', nb_runtime_threads)
def test_one_empty_candidate(as_bytes, nb_runtime_threads):
  candidates = ['']
  assert run_jaro(candidates, 'hi', as_bytes, nb_runtime_threads) == [('', 0.0)]

@pytest.mark.parametrize('as_bytes', as_bytes)
@pytest.mark.parametrize('nb_runtime_threads', nb_runtime_threads)
def test_one_empty_candidate_and_input(as_bytes, nb_runtime_threads):
  candidates = ['']
  assert run_jaro(candidates, '', as_bytes, nb_runtime_threads) == [('', 0.0)]

@pytest.mark.parametrize('as_bytes', as_bytes)
@pytest.mark.parametrize('nb_runtime_threads', nb_runtime_threads)
def test_one_perfect_match(as_bytes, nb_runtime_threads):
  candidates = ['hélloz']
  assert run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads) == [('hélloz', 1.0)]

test_candidates_results = [
  ('hii', 0.5),
  ('hélloz', 1.0),
  ('lolz', 0.75),
  ('中国', 0.0)
]

@pytest.mark.parametrize('as_bytes', as_bytes)
@pytest.mark.parametrize('nb_runtime_threads', nb_runtime_threads)
def test_multiple_matches(as_bytes, nb_runtime_threads):
  candidates = ['hélloz', '中国', 'lolz', 'hii']
  res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads)
  assert res == test_candidates_results

@pytest.mark.parametrize('as_bytes', as_bytes)
@pytest.mark.parametrize('nb_runtime_threads', nb_runtime_threads)
def test_min_scores_all_ok(as_bytes, nb_runtime_threads):
  candidates = [
    { 'candidate': 'hélloz', 'min_score': 0.9 },
    { 'candidate': '中国', 'min_score': 0.0 },
    { 'candidate': 'lolz', 'min_score': 0.7 },
    { 'candidate': 'hii', 'min_score': 0.4 }
  ]
  res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads)
  assert res == test_candidates_results

@pytest.mark.parametrize('as_bytes', as_bytes)
@pytest.mark.parametrize('nb_runtime_threads', nb_runtime_threads)
def test_min_scores_all_ok_exact(as_bytes, nb_runtime_threads):
  candidates = [
    { 'candidate': 'hélloz', 'min_score': 1.0 },
    { 'candidate': '中国', 'min_score': 0.0 },
    { 'candidate': 'lolz', 'min_score': 0.75 },
    { 'candidate': 'hii', 'min_score': 0.5 }
  ]
  res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads)
  assert res == test_candidates_results

@pytest.mark.parametrize('as_bytes', as_bytes)
@pytest.mark.parametrize('nb_runtime_threads', nb_runtime_threads)
def test_min_scores_some_filtered(as_bytes, nb_runtime_threads):
  candidates = [
    { 'candidate': 'hélloz', 'min_score': 1.0 },
    { 'candidate': '中国', 'min_score': 0.0 },
    { 'candidate': 'lolz', 'min_score': 0.750001 },
    { 'candidate': 'hii', 'min_score': 0.500001 }
  ]
  res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads)
  assert res == [
    ('hélloz', 1.0),
    ('中国', 0.0)
  ]

@pytest.mark.parametrize('as_bytes', as_bytes)
@pytest.mark.parametrize('nb_runtime_threads', nb_runtime_threads)
def test_min_scores_all_filtered(as_bytes, nb_runtime_threads):
  candidates = [
    { 'candidate': '中国', 'min_score': 0.000001 },
    { 'candidate': 'lolz', 'min_score': 0.750001 },
    { 'candidate': 'hii', 'min_score': 0.500001 }
  ]
  res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads)
  assert res == []

@pytest.mark.parametrize('as_bytes', as_bytes)
@pytest.mark.parametrize('nb_runtime_threads', nb_runtime_threads)
def test_global_min_score_all_ok(as_bytes, nb_runtime_threads):
  candidates = ['hélloz', '中国', 'lolz', 'hii']
  res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, min_score=0.0)
  assert res == test_candidates_results

@pytest.mark.parametrize('as_bytes', as_bytes)
@pytest.mark.parametrize('nb_runtime_threads', nb_runtime_threads)
def test_global_min_score_some_filtered(as_bytes, nb_runtime_threads):
  candidates = ['hélloz', '中国', 'lolz', 'hii']
  res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, min_score=0.5)
  assert res == [
    ('hii', 0.5),
    ('hélloz', 1.0),
    ('lolz', 0.75)
  ]

@pytest.mark.parametrize('as_bytes', as_bytes)
@pytest.mark.parametrize('nb_runtime_threads', nb_runtime_threads)
def test_global_min_score_some_filtered2(as_bytes, nb_runtime_threads):
  candidates = ['hélloz', '中国', 'lolz', 'hii']
  res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, min_score=0.500001)
  assert res == [
    ('hélloz', 1.0),
    ('lolz', 0.75)
  ]

@pytest.mark.parametrize('as_bytes', as_bytes)
@pytest.mark.parametrize('nb_runtime_threads', nb_runtime_threads)
def test_global_min_score_all_filtered(as_bytes, nb_runtime_threads):
  candidates = ['中国', 'lolz', 'hii']
  res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, min_score=0.8)
  assert res == []

@pytest.mark.parametrize('as_bytes', as_bytes)
@pytest.mark.parametrize('nb_runtime_threads', nb_runtime_threads)
def test_global_min_score_ovveride_min_scores(as_bytes, nb_runtime_threads):
  candidates = [
    { 'candidate': 'hélloz', 'min_score': 1.0 },
    { 'candidate': '中国', 'min_score': 0.0 },
    { 'candidate': 'lolz', 'min_score': 0.750001 },
    { 'candidate': 'hii', 'min_score': 0.500001 }
  ]
  res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, min_score=0.75)
  assert res == [
    ('hélloz', 1.0),
    ('lolz', 0.75)
  ]

@pytest.mark.parametrize('as_bytes', as_bytes)
@pytest.mark.parametrize('nb_runtime_threads', nb_runtime_threads)
def test_n_best_results_zero(as_bytes, nb_runtime_threads):
  candidates = ['hélloz', '中国', 'lolz', 'hii']
  res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, n_best_results=0)
  assert res == []

@pytest.mark.parametrize('as_bytes', as_bytes)
@pytest.mark.parametrize('nb_runtime_threads', nb_runtime_threads)
def test_n_best_results_too_big(as_bytes, nb_runtime_threads):
  candidates = ['hélloz', '中国', 'lolz', 'hii']
  res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, n_best_results=5)
  assert res == test_candidates_results

@pytest.mark.parametrize('as_bytes', as_bytes)
@pytest.mark.parametrize('nb_runtime_threads', nb_runtime_threads)
def test_n_best_results_all(as_bytes, nb_runtime_threads):
  candidates = ['hélloz', '中国', 'lolz', 'hii']
  res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, n_best_results=4)
  assert res == test_candidates_results

@pytest.mark.parametrize('as_bytes', as_bytes)
@pytest.mark.parametrize('nb_runtime_threads', nb_runtime_threads)
def test_n_best_results_some_filtered(as_bytes, nb_runtime_threads):
  candidates = ['hélloz', '中国', 'lolz', 'hii']
  res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, n_best_results=3)
  assert res == [
    ('hii', 0.5),
    ('hélloz', 1.0),
    ('lolz', 0.75)
  ]

@pytest.mark.parametrize('as_bytes', as_bytes)
@pytest.mark.parametrize('nb_runtime_threads', nb_runtime_threads)
def test_n_best_results_some_filtered2(as_bytes, nb_runtime_threads):
  candidates = ['hélloz', '中国', 'lolz', 'hii']
  res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, n_best_results=2)
  assert res == [
    ('hélloz', 1.0),
    ('lolz', 0.75)
  ]

@pytest.mark.parametrize('as_bytes', as_bytes)
@pytest.mark.parametrize('nb_runtime_threads', nb_runtime_threads)
def test_n_best_results_respect_min_scores(as_bytes, nb_runtime_threads):
  candidates = [
    { 'candidate': 'hélloz', 'min_score': 1.0 },
    { 'candidate': '中国', 'min_score': 0.0 },
    { 'candidate': 'lolz', 'min_score': 0.750001 },
    { 'candidate': 'hii', 'min_score': 0.5 }
  ]
  res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, n_best_results=2)
  assert res == [
    ('hii', 0.5),
    ('hélloz', 1.0)
  ]

@pytest.mark.parametrize('as_bytes', as_bytes)
@pytest.mark.parametrize('nb_runtime_threads', nb_runtime_threads)
def test_n_best_results_respect_min_scores2(as_bytes, nb_runtime_threads):
  candidates = [
    { 'candidate': 'hélloz', 'min_score': 1.0 },
    { 'candidate': '中国', 'min_score': 0.0 },
    { 'candidate': 'lolz', 'min_score': 0.75 },
    { 'candidate': 'hii', 'min_score': 0.5 }
  ]
  res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, n_best_results=2)
  assert res == [
    ('hélloz', 1.0),
    ('lolz', 0.75)
  ]

@pytest.mark.parametrize('as_bytes', as_bytes)
@pytest.mark.parametrize('nb_runtime_threads', nb_runtime_threads)
def test_n_best_results_respect_min_score(as_bytes, nb_runtime_threads):
  candidates = [
    { 'candidate': 'hélloz', 'min_score': 1.0 },
    { 'candidate': '中国', 'min_score': 0.0 },
    { 'candidate': 'lolz', 'min_score': 0.750001 },
    { 'candidate': 'hii', 'min_score': 0.5 }
  ]
  res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, min_score=0.75001, n_best_results=2)
  assert res == [
    ('hélloz', 1.0)
  ]

@pytest.mark.parametrize('as_bytes', as_bytes)
@pytest.mark.parametrize('nb_runtime_threads', nb_runtime_threads)
def test_n_best_results_respect_min_score2(as_bytes, nb_runtime_threads):
  candidates = [
    { 'candidate': 'hélloz', 'min_score': 1.0 },
    { 'candidate': '中国', 'min_score': 0.0 },
    { 'candidate': 'lolz', 'min_score': 0.750001 },
    { 'candidate': 'hii', 'min_score': 0.5 }
  ]
  res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, min_score=0.75, n_best_results=2)
  assert res == [
    ('hélloz', 1.0),
    ('lolz', 0.75)
  ]

@pytest.mark.parametrize('as_bytes', as_bytes)
@pytest.mark.parametrize('nb_runtime_threads', nb_runtime_threads)
def test_n_best_results_respect_min_score3(as_bytes, nb_runtime_threads):
  candidates = [
    { 'candidate': 'hélloz', 'min_score': 1.0 },
    { 'candidate': '中国', 'min_score': 0.0 },
    { 'candidate': 'lolz', 'min_score': 0.750001 },
    { 'candidate': 'hii', 'min_score': 0.5 }
  ]
  res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, min_score=0.0, n_best_results=2)
  assert res == [
    ('hélloz', 1.0),
    ('lolz', 0.75)
  ]
