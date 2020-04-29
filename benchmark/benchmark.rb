

require 'batch_jaro_winkler'

require 'jaro_winkler'
require 'amatch'
require 'hotwater'
require 'fuzzystringmatch'

def jaro_winkler_jaro_winkler_distance(candidates, inp, min_score, winkler)
  res = []
  fun = winkler ? JaroWinkler.method(:distance) : JaroWinkler.method(:jaro_distance)
  candidates.each do |candidate|
    score = fun.call(candidate, inp)
    res << [candidate, score] if score >= min_score
  end
end

def amatch_jaro_winkler_distance(candidates, inp, min_score, winkler)
  res = []
  fun = winkler ? Amatch::JaroWinkler.method(:new) : Amatch::Jaro.method(:new)
  candidates.each do |candidate|
    m = fun.call(candidate)
    score = m.match(inp)
    res << [candidate, score] if score >= min_score
  end
end

def hotwater_jaro_winkler_distance(candidates, inp, min_score, winkler)
  res = []
  fun = winkler ? Hotwater.method(:jaro_winkler_distance) : Hotwater.method(:jaro_distance)
  candidates.each do |candidate|
    score = fun.call(candidate, inp)
    res << [candidate, score] if score >= min_score
  end
end

def fuzzystringmatch_jaro_winkler_distance(candidates, inp, min_score, winkler)
  jaro = FuzzyStringMatch::JaroWinkler.create(:native)
  res = []
  candidates.each do |candidate|
    score = jaro.getDistance(candidate, inp)
    res << [candidate, score] if score >= min_score
  end
end

def benchmark_bjw(candidates, test_inputs, min_score, winkler, n_best_results, nb_runtime_threads)
  exportable_model = BatchJaroWinkler.build_exportable_model(candidates, nb_runtime_threads: nb_runtime_threads)

  puts "batch_jaro_winkler #{nb_runtime_threads} threads ..."
  start = Time.now
  fun = winkler ? BatchJaroWinkler.method(:jaro_winkler_distance) : BatchJaroWinkler.method(:jaro_distance)
  runtime_model = BatchJaroWinkler.build_runtime_model(exportable_model)
  test_inputs.each do |test_input|
    fun.call(runtime_model, test_input, min_score: min_score, n_best_results: n_best_results)
  end
  puts "Time for batch_jaro_winkler #{nb_runtime_threads} threads: #{Time.now - start} seconds"
end

def benchmark_library(candidates, test_inputs, library_name, fun, min_score, winkler, n_best_results)
  puts "#{library_name}..."
  start = Time.now
  test_inputs.each do |test_input|
    res = fun.call(candidates, test_input, min_score, winkler)
    res = res.sort_by{ |r| r[0] }[0..(n_best_results - 1)] if n_best_results
  end
  puts "Time for #{library_name}: #{Time.now - start} seconds"
end

def benchmark(filename, min_score, winkler, n_best_results)
  f = File.open(filename, 'r')
  words = f.read.split("\n")
  f.close
  words.shuffle!(random: Random.new(0))
  # among inputs: some absent from train candidates, some present
  test_inputs, candidates = words[0..100], words[50..]

  [1, 4, 6, 8, 12].each do |nb_runtime_threads|
    benchmark_bjw(candidates, test_inputs, min_score, winkler, n_best_results, nb_runtime_threads)
  end

  benchmark_library(candidates, test_inputs, 'jaro_winkler', self.method(:jaro_winkler_jaro_winkler_distance), min_score, winkler, n_best_results)
  benchmark_library(candidates, test_inputs, 'amatch', self.method(:amatch_jaro_winkler_distance), min_score, winkler, n_best_results)
  benchmark_library(candidates, test_inputs, 'hotwater', self.method(:hotwater_jaro_winkler_distance), min_score, winkler, n_best_results)
  benchmark_library(candidates, test_inputs, 'fuzzy-string-match', self.method(:fuzzystringmatch_jaro_winkler_distance), min_score, winkler, n_best_results)
end

filenames = ['../support/english_words.txt', '../support/chinese_words.txt']
min_scores = [0.9, 0.8, 0.5, 0.0]
winkler = [false, true]
n_best_results = [nil, 10]

filenames.each do |filename|
  min_scores.each do |min_score|
    winkler.each do |wink|
      n_best_results.each do |n_best_res|
        puts '__________________________________________'
        puts "file: #{filename} | min_score: #{min_score} | winkler: #{wink} | n_best_results: #{n_best_res}"
        puts ''
        benchmark(filename, min_score, wink, n_best_res)
      end
    end
  end
end
