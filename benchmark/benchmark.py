

import os
import sys
import random
import time

# Many libraries are not working correctly, for example pyjarowinkler with 'nebraskans' and 'darbs', but we still want their performance
from pyjarowinkler import distance as pyjarowinkler_distance
import textdistance
import jellyfish
import jaro
import Levenshtein
import py_stringmatching
import batch_jaro_winkler

def textdistance_jaro_winkler_distance(candidates, inp, min_score, winkler):
  res = []
  fun = textdistance.jaro_winkler if winkler else textdistance.jaro
  for candidate in candidates:
    score = fun(candidate, inp)
    if score >= min_score:
      res.append((candidate, score))
  return res

def pyjarowinkler_jaro_winkler_distance(candidates, inp, min_score, winkler):
  res = []
  for candidate in candidates:
    score = pyjarowinkler_distance.get_jaro_distance(candidate, inp, winkler=winkler)
    if score >= min_score:
      res.append((candidate, score))
  return res

def jellyfish_jaro_winkler_distance(candidates, inp, min_score, winkler):
  res = []
  fun = jellyfish.jaro_winkler if winkler else jellyfish.jaro_distance
  for candidate in candidates:
    score = fun(candidate, inp)
    if score >= min_score:
      res.append((candidate, score))
  return res

def jaro_jaro_winkler_distance(candidates, inp, min_score, winkler):
  res = []
  fun = jaro.jaro_winkler_metric if winkler else jaro.jaro_metric
  for candidate in candidates:
    score = fun(candidate, inp)
    if score >= min_score:
      res.append((candidate, score))
  return res

def levenshtein_jaro_winkler_distance(candidates, inp, min_score, winkler):
  res = []
  fun = Levenshtein.jaro_winkler if winkler else Levenshtein.jaro
  for candidate in candidates:
    score = fun(candidate, inp)
    if score >= min_score:
      res.append((candidate, score))
  return res

def py_stringmatching_jaro_winkler_distance(candidates, inp, min_score, winkler):
  res = []
  fun = py_stringmatching.JaroWinkler().get_raw_score if winkler else py_stringmatching.Jaro().get_raw_score
  for candidate in candidates:
    score = fun(candidate, inp)
    if score >= min_score:
      res.append((candidate, score))
  return res

def benchmark_bjw(candidates, test_inputs, min_score, winkler, n_best_results, nb_runtime_threads):
  exportable_model = batch_jaro_winkler.build_exportable_model(candidates, nb_runtime_threads=nb_runtime_threads)

  print('batch_jaro_winkler {} threads ...'.format(nb_runtime_threads))
  start = time.time()
  fun = batch_jaro_winkler.jaro_winkler_distance if winkler else batch_jaro_winkler.jaro_distance
  runtime_model = batch_jaro_winkler.build_runtime_model(exportable_model)
  for test_input in test_inputs:
    fun(runtime_model, test_input, min_score=min_score, n_best_results=n_best_results)
  print('Time for batch_jaro_winkler {} threads: {} seconds'.format(nb_runtime_threads, time.time() - start))

def benchmark_library(candidates, test_inputs, library_name, fun, min_score, winkler, n_best_results):
  print('{}...'.format(library_name))
  start = time.time()
  for test_input in test_inputs:
    res = fun(candidates, test_input, min_score, winkler)
    if n_best_results:
      res.sort(key=lambda r: r[0])
      res = res[:n_best_results]
  print('Time for {}: {} seconds'.format(library_name, time.time() - start))

def benchmark(filename, min_score, winkler, n_best_results):
  words = []
  with open(os.path.join(sys.path[0], filename), 'rb') as f:
    words = f.read().decode('utf-8').split('\n')
  random.seed(0)
  random.shuffle(words)
  # among inputs: some absent from train candidates, some present
  test_inputs, candidates = words[:100], words[50:len(words)]

  for nb_runtime_threads in [1, 4, 6, 8, 12]:
    benchmark_bjw(candidates, test_inputs, min_score, winkler, n_best_results, nb_runtime_threads)

  benchmark_library(candidates, test_inputs, 'textdistance', textdistance_jaro_winkler_distance, min_score, winkler, n_best_results)
  benchmark_library(candidates, test_inputs, 'pyjarowinkler', pyjarowinkler_jaro_winkler_distance, min_score, winkler, n_best_results)
  benchmark_library(candidates, test_inputs, 'jellyfish', jellyfish_jaro_winkler_distance, min_score, winkler, n_best_results)
  benchmark_library(candidates, test_inputs, 'jaro', jaro_jaro_winkler_distance, min_score, winkler, n_best_results)
  benchmark_library(candidates, test_inputs, 'Levenshtein', levenshtein_jaro_winkler_distance, min_score, winkler, n_best_results)
  benchmark_library(candidates, test_inputs, 'py_stringmatching', py_stringmatching_jaro_winkler_distance, min_score, winkler, n_best_results)


filenames = ['../support/english_words.txt', '../support/chinese_words.txt']
min_scores = [0.9, 0.8, 0.5, 0.0]
winkler = [False, True]
n_best_results = [None, 10]

for filename in filenames:
  for min_score in min_scores:
    for wink in winkler:
      for n_best_res in n_best_results:
        print('__________________________________________')
        print('file: {} | min_score: {} | winkler: {} | n_best_results: {}'.format(filename, min_score, wink, n_best_res))
        print('')
        benchmark(filename, min_score, wink, n_best_res)
