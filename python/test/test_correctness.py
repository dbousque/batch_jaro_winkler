

import pytest
import os
import sys
import random

import py_stringmatching
import batch_jaro_winkler as bjw

def textdistance_jaro_winkler_distance(candidates, inp, min_score, winkler):
  res = []
  fun = py_stringmatching.JaroWinkler().get_raw_score if winkler else py_stringmatching.Jaro().get_raw_score
  for candidate in candidates:
    score = fun(candidate, inp)
    if score >= min_score:
      res.append((candidate, score))
  return res

filenames = ['../../support/english_words.txt', '../../support/chinese_words.txt']
nb_runtime_threads = [1, 2, 5]
min_scores = [0.0, 0.25, 0.738]
winkler = [False, True]
as_bytes = [False, True]

words_cache = {}
exportable_models_cache = {}

@pytest.mark.parametrize('filename', filenames)
@pytest.mark.parametrize('nb_runtime_threads', nb_runtime_threads)
@pytest.mark.parametrize('min_score', min_scores)
@pytest.mark.parametrize('winkler', winkler)
@pytest.mark.parametrize('as_bytes', as_bytes)
def test_words_lists(filename, nb_runtime_threads, min_score, winkler, as_bytes):
  test_inputs, candidates = words_cache.get(filename, (None, None))
  if test_inputs is None:
    words = []
    with open(os.path.join(sys.path[0], filename), 'rb') as f:
      words = f.read().decode('utf-8').split('\n')
    random.seed(0)
    random.shuffle(words)
    # among inputs: some absent from train candidates, some present
    test_inputs, candidates = words[:40], words[20:20000]
    words_cache[filename] = (test_inputs, candidates)
  exportable_model = exportable_models_cache.get('{}-{}'.format(filename, nb_runtime_threads))
  if exportable_model is None:
    if as_bytes:
      exportable_model = bjw.build_exportable_model_bytes(4, list(map(lambda x: x.encode('utf-32-le'), candidates)), nb_runtime_threads=nb_runtime_threads)
    else:
      exportable_model = bjw.build_exportable_model(candidates, nb_runtime_threads=nb_runtime_threads)
    exportable_models_cache['{}-{}'.format(filename, nb_runtime_threads)] = exportable_model
  runtime_model = bjw.build_runtime_model(exportable_model)
  for test_input in test_inputs:
    if winkler:
      if as_bytes:
        results = bjw.jaro_winkler_distance_bytes(4, runtime_model, test_input.encode('utf-32-le'), min_score=min_score, threshold=0.0)
        for i_res, res in enumerate(results):
          results[i_res] = (res[0].decode('utf-32-le'), res[1])
      else:
        results = bjw.jaro_winkler_distance(runtime_model, test_input, min_score=min_score, threshold=0.0)
    else:
      if as_bytes:
        results = bjw.jaro_distance_bytes(4, runtime_model, test_input.encode('utf-32-le'), min_score=min_score)
        for i_res, res in enumerate(results):
          results[i_res] = (res[0].decode('utf-32-le'), res[1])
      else:
        results = bjw.jaro_distance(runtime_model, test_input, min_score=min_score)
    reference_results = textdistance_jaro_winkler_distance(candidates, test_input, min_score, winkler)
    results.sort(key=lambda r: r[0])
    reference_results.sort(key=lambda r: r[0])
    assert len(results) == len(reference_results)
    for res, expected_res in zip(results, reference_results):
      assert res[0] == expected_res[0]
      # enforce precision up to 4th decimal point
      assert abs(res[1] - expected_res[1]) < 0.0001