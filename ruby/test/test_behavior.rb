

require 'batch_jaro_winkler'

def build_all_args(args)
  return args[0].map{ |a| [a] } if args.size == 1

  rest = build_all_args(args[1..args.size])
  res = []
  args[0].each do |first_arg|
    rest.each do |rest_args|
      res << [first_arg] + rest_args
    end
  end
  res
end

def parametrized_test(test_name, args)
  all_args = build_all_args(args)
  all_args.each do |a|
    test_full_name = "test_#{test_name}_#{a.map(&:to_s).join('_')}"
    define_method(test_full_name) do
      puts "- #{test_full_name}"
      yield(a)
    end
  end
end

def run_jaro(candidates, inp, as_bytes, nb_runtime_threads, min_score = nil, n_best_results = nil)
  if as_bytes
    with_min_scores = candidates.size > 0 && candidates[0].is_a?(Hash)
    if with_min_scores
      candidates.each do |candidate|
        candidate[:candidate] = candidate[:candidate].encode('utf-16le')
      end
    else
      candidates = candidates.map{ |candidate| candidate.encode('utf-16le') }
    end
    exportable_model = BatchJaroWinkler.build_exportable_model_bytes(2, candidates, nb_runtime_threads: nb_runtime_threads)
  else
    exportable_model = BatchJaroWinkler.build_exportable_model(candidates, nb_runtime_threads: nb_runtime_threads)
  end
  runtime_model = BatchJaroWinkler.build_runtime_model(exportable_model)
  res = nil
  if as_bytes
    inp = inp.encode('utf-16le')
    res = BatchJaroWinkler.jaro_distance_bytes(2, runtime_model, inp, min_score: min_score, n_best_results: n_best_results)
    res.each do |r|
      r[0].force_encoding('utf-16le')
      r[0] = r[0].encode('utf-8')
    end
  else
    res = BatchJaroWinkler.jaro_distance(runtime_model, inp, min_score: min_score, n_best_results: n_best_results)
  end
  res.sort_by{ |r| r[0] }
end

def assert_equal(arg1, arg2)
  return if arg1 == arg2

  puts "#{arg1.inspect} != #{arg2.inspect}"
  exit 1
end

module TestBehavior
  class << self
    as_bytes = [false, true]
    nb_runtime_threads = [1, 2, 3, 4, 5, 6]
    args = [as_bytes, nb_runtime_threads]

    parametrized_test('no_candidates', args) do |as_bytes, nb_runtime_threads|
      candidates = []
      assert_equal run_jaro(candidates, 'hi', as_bytes, nb_runtime_threads), []
    end

    parametrized_test('no_candidates_empty_input', args) do |as_bytes, nb_runtime_threads|
      candidates = []
      assert_equal run_jaro(candidates, '', as_bytes, nb_runtime_threads), []
    end

    parametrized_test('one_empty_candidate', args) do |as_bytes, nb_runtime_threads|
      candidates = ['']
      assert_equal run_jaro(candidates, 'hi', as_bytes, nb_runtime_threads), [['', 0.0]]
    end

    parametrized_test('one_empty_candidate_and_input', args) do |as_bytes, nb_runtime_threads|
      candidates = ['']
      assert_equal run_jaro(candidates, '', as_bytes, nb_runtime_threads), [['', 0.0]]
    end

    parametrized_test('one_perfect_match', args) do |as_bytes, nb_runtime_threads|
      candidates = ['hélloz']
      assert_equal run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads), [['hélloz', 1.0 ]]
    end

    test_candidates_results = [
      ['hii', 0.5],
      ['hélloz', 1.0],
      ['lolz', 0.75],
      ['中国', 0.0]
    ]

    parametrized_test('multiple_matches', args) do |as_bytes, nb_runtime_threads|
      candidates = ['hélloz', '中国', 'lolz', 'hii']
      res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads)
      # .sort to define the result's order (for assert)
      res = res.sort{ |a, b| a[0] <=> b[0] }
      assert_equal res, test_candidates_results
    end

    parametrized_test('min_scores_all_ok', args) do |as_bytes, nb_runtime_threads|
      candidates = [
        { candidate: 'hélloz', min_score: 0.9 },
        { candidate: '中国', min_score: 0.0 },
        { candidate: 'lolz', min_score: 0.7 },
        { candidate: 'hii', min_score: 0.4 }
      ]
      res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads)
      # .sort to define the result's order (for assert)
      res = res.sort{ |a, b| a[0] <=> b[0] }
      assert_equal res, test_candidates_results
    end

    parametrized_test('min_scores_all_ok_exact', args) do |as_bytes, nb_runtime_threads|
      candidates = [
        { candidate: 'hélloz', min_score: 1.0 },
        { candidate: '中国', min_score: 0.0 },
        { candidate: 'lolz', min_score: 0.75 },
        { candidate: 'hii', min_score: 0.5 }
      ]
      res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads)
      # .sort to define the result's order (for assert)
      res = res.sort{ |a, b| a[0] <=> b[0] }
      assert_equal res, test_candidates_results
    end

    parametrized_test('min_scores_some_filtered', args) do |as_bytes, nb_runtime_threads|
      candidates = [
        { candidate: 'hélloz', min_score: 1.0 },
        { candidate: '中国', min_score: 0.0 },
        { candidate: 'lolz', min_score: 0.750001 },
        { candidate: 'hii', min_score: 0.500001 }
      ]
      res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads)
      # .sort to define the result's order (for assert)
      res = res.sort{ |a, b| a[0] <=> b[0] }
      assert_equal res, [
        ['hélloz', 1.0],
        ['中国', 0.0]
      ]
    end

    parametrized_test('min_scores_all_filtered', args) do |as_bytes, nb_runtime_threads|
      candidates = [
        { candidate: '中国', min_score: 0.000001 },
        { candidate: 'lolz', min_score: 0.750001 },
        { candidate: 'hii', min_score: 0.500001 }
      ]
      res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads)
      # .sort to define the result's order (for assert)
      res = res.sort{ |a, b| a[0] <=> b[0] }
      assert_equal res, []
    end

    parametrized_test('global_min_score_all_ok', args) do |as_bytes, nb_runtime_threads|
      candidates = ['hélloz', '中国', 'lolz', 'hii']
      res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, min_score=0.0)
      # .sort to define the result's order (for assert)
      res = res.sort{ |a, b| a[0] <=> b[0] }
      assert_equal res, test_candidates_results
    end

    parametrized_test('global_min_score_some_filtered', args) do |as_bytes, nb_runtime_threads|
      candidates = ['hélloz', '中国', 'lolz', 'hii']
      res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, min_score=0.5)
      # .sort to define the result's order (for assert)
      res = res.sort{ |a, b| a[0] <=> b[0] }
      assert_equal res, [
        ['hii', 0.5],
        ['hélloz', 1.0],
        ['lolz', 0.75]
      ]
    end

    parametrized_test('global_min_score_some_filtered2', args) do |as_bytes, nb_runtime_threads|
      candidates = ['hélloz', '中国', 'lolz', 'hii']
      res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, min_score=0.500001)
      # .sort to define the result's order (for assert)
      res = res.sort{ |a, b| a[0] <=> b[0] }
      assert_equal res, [
        ['hélloz', 1.0],
        ['lolz', 0.75]
      ]
    end

    parametrized_test('global_min_score_all_filtered', args) do |as_bytes, nb_runtime_threads|
      candidates = ['中国', 'lolz', 'hii']
      res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, min_score=0.8)
      # .sort to define the result's order (for assert)
      res = res.sort{ |a, b| a[0] <=> b[0] }
      assert_equal res, []
    end

    parametrized_test('global_min_score_ovveride_min_scores', args) do |as_bytes, nb_runtime_threads|
      candidates = [
        { candidate: 'hélloz', min_score: 1.0 },
        { candidate: '中国', min_score: 0.0 },
        { candidate: 'lolz', min_score: 0.750001 },
        { candidate: 'hii', min_score: 0.500001 }
      ]
      res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, min_score=0.75)
      # .sort to define the result's order (for assert)
      res = res.sort{ |a, b| a[0] <=> b[0] }
      assert_equal res, [
        ['hélloz', 1.0],
        ['lolz', 0.75]
      ]
    end

    parametrized_test('n_best_results_zero', args) do |as_bytes, nb_runtime_threads|
      candidates = ['hélloz', '中国', 'lolz', 'hii']
      res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, min_score=nil, n_best_results=0)
      assert_equal res, []
    end

    parametrized_test('n_best_results_too_big', args) do |as_bytes, nb_runtime_threads|
      candidates = ['hélloz', '中国', 'lolz', 'hii']
      res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, min_score=nil, n_best_results=5)
      assert_equal res, test_candidates_results
    end

    parametrized_test('n_best_results_all', args) do |as_bytes, nb_runtime_threads|
      candidates = ['hélloz', '中国', 'lolz', 'hii']
      res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, min_score=nil, n_best_results=4)
      assert_equal res, test_candidates_results
    end

    parametrized_test('n_best_results_some_filtered', args) do |as_bytes, nb_runtime_threads|
      candidates = ['hélloz', '中国', 'lolz', 'hii']
      res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, min_score=nil, n_best_results=3)
      assert_equal res, [
        ['hii', 0.5],
        ['hélloz', 1.0],
        ['lolz', 0.75]
      ]
    end

    parametrized_test('n_best_results_some_filtered2', args) do |as_bytes, nb_runtime_threads|
      candidates = ['hélloz', '中国', 'lolz', 'hii']
      res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, min_score=nil, n_best_results=2)
      assert_equal res, [
        ['hélloz', 1.0],
        ['lolz', 0.75]
      ]
    end

    parametrized_test('n_best_results_respect_min_scores', args) do |as_bytes, nb_runtime_threads|
      candidates = [
        { candidate: 'hélloz', min_score: 1.0 },
        { candidate: '中国', min_score: 0.0 },
        { candidate: 'lolz', min_score: 0.750001 },
        { candidate: 'hii', min_score: 0.5 }
      ]
      res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, min_score=nil, n_best_results=2)
      assert_equal res, [
        ['hii', 0.5],
        ['hélloz', 1.0]
      ]
    end

    parametrized_test('n_best_results_respect_min_scores2', args) do |as_bytes, nb_runtime_threads|
      candidates = [
        { candidate: 'hélloz', min_score: 1.0 },
        { candidate: '中国', min_score: 0.0 },
        { candidate: 'lolz', min_score: 0.75 },
        { candidate: 'hii', min_score: 0.5 }
      ]
      res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, min_score=nil, n_best_results=2)
      assert_equal res, [
        ['hélloz', 1.0],
        ['lolz', 0.75]
      ]
    end

    parametrized_test('n_best_results_respect_min_score', args) do |as_bytes, nb_runtime_threads|
      candidates = [
        { candidate: 'hélloz', min_score: 1.0 },
        { candidate: '中国', min_score: 0.0 },
        { candidate: 'lolz', min_score: 0.750001 },
        { candidate: 'hii', min_score: 0.5 }
      ]
      res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, min_score=0.75001, n_best_results=2)
      assert_equal res, [
        ['hélloz', 1.0]
      ]
    end

    parametrized_test('n_best_results_respect_min_score2', args) do |as_bytes, nb_runtime_threads|
      candidates = [
        { candidate: 'hélloz', min_score: 1.0 },
        { candidate: '中国', min_score: 0.0 },
        { candidate: 'lolz', min_score: 0.750001 },
        { candidate: 'hii', min_score: 0.5 }
      ]
      res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, min_score=0.75, n_best_results=2)
      assert_equal res, [
        ['hélloz', 1.0],
        ['lolz', 0.75]
      ]
    end

    parametrized_test('n_best_results_respect_min_score3', args) do |as_bytes, nb_runtime_threads|
      candidates = [
        { candidate: 'hélloz', min_score: 1.0 },
        { candidate: '中国', min_score: 0.0 },
        { candidate: 'lolz', min_score: 0.750001 },
        { candidate: 'hii', min_score: 0.5 }
      ]
      res = run_jaro(candidates, 'hélloz', as_bytes, nb_runtime_threads, min_score=0.0, n_best_results=2)
      assert_equal res, [
        ['hélloz', 1.0],
        ['lolz', 0.75]
      ]
    end

    parametrized_test('long_candidate', args) do |as_bytes, nb_runtime_threads|
      long_candidate = 'b' * (256 * 128)
      normal_candidate = 'aaaaaaaaaaa'
      candidates = [normal_candidate, long_candidate]
      res = run_jaro(candidates, normal_candidate, as_bytes, nb_runtime_threads, min_score=0.9)
      assert_equal res, [
        [normal_candidate, 1.0]
      ]
    end

    parametrized_test('long_candidate2', args) do |as_bytes, nb_runtime_threads|
      long_candidate = 'b' * (256 * 128)
      normal_candidate = 'aaaaaaaaaaa'
      candidates = [normal_candidate, long_candidate]
      res = run_jaro(candidates, long_candidate, as_bytes, nb_runtime_threads, min_score=0.9)
      assert_equal res, [
        [long_candidate, 1.0]
      ]
    end
  end
end

TestBehavior.methods.select{ |m| m.to_s.start_with?('test_') }.each do |func|
  TestBehavior.method(func).call
end
puts "All ok"
