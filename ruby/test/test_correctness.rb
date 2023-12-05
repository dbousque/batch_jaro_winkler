

require 'batch_jaro_winkler'
require 'jaro_winkler'

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

def assert_equal(arg1, arg2)
  return if arg1 == arg2

  puts "#{arg1.inspect} != #{arg2.inspect}"
  exit 1
end

def jaro_winkler_jaro_winkler_distance(candidates, inp, min_score, winkler)
  res = []
  fun = winkler ? JaroWinkler.method(:distance) : JaroWinkler.method(:jaro_distance)
  candidates.each do |candidate|
    score = fun.call(candidate, inp)
    res << [candidate, score] if score >= min_score
  end
  res
end

def encode_utf32_le_without_memory_leak(str)
  str = str.encode('utf-32')
  str = str.bytes[4..-1].map(&:chr).each_slice(4).map{ |c| c.reverse }.flatten.join
  str.force_encoding('utf-32le')
  str
end

module TestCorrectness
  class << self
    filenames = ['../../support/english_words.txt', '../../support/chinese_words.txt']
    nb_runtime_threads = [1, 2, 5]
    min_scores = [0.0, 0.25, 0.738]
    winkler = [false, true]
    as_bytes = [false, true]
    args = [filenames, nb_runtime_threads, min_scores, winkler, as_bytes]

    parametrized_test('words_lists', args) do |filename, nb_runtime_threads, min_score, winkler, as_bytes|
      f = File.open(filename, 'r')
      words = f.read.split("\n")
      f.close
      words.shuffle!(random: Random.new(0))
      # among inputs: some absent from train candidates, some present
      test_inputs, candidates = words[0..40], words[20..20000]
      if as_bytes
        exportable_model = BatchJaroWinkler.build_exportable_model_bytes(4, candidates.map{ |c| encode_utf32_le_without_memory_leak(c) }, nb_runtime_threads: nb_runtime_threads)
      else
        exportable_model = BatchJaroWinkler.build_exportable_model(candidates, nb_runtime_threads: nb_runtime_threads)
      end
      runtime_model = BatchJaroWinkler.build_runtime_model(exportable_model)
      test_inputs.each do |test_input|
        if winkler
          if as_bytes
            results = BatchJaroWinkler.jaro_winkler_distance_bytes(4, runtime_model, encode_utf32_le_without_memory_leak(test_input), min_score: min_score)
            results.each do |res|
              res[0].force_encoding('utf-32le')
              res[0] = res[0].encode('utf-8')
            end
          else
            results = BatchJaroWinkler.jaro_winkler_distance(runtime_model, test_input, min_score: min_score)
          end
        else
          if as_bytes
            results = BatchJaroWinkler.jaro_distance_bytes(4, runtime_model, encode_utf32_le_without_memory_leak(test_input), min_score: min_score)
            results.each do |res|
              res[0].force_encoding('utf-32le')
              res[0] = res[0].encode('utf-8')
            end
          else
            results = BatchJaroWinkler.jaro_distance(runtime_model, test_input, min_score: min_score)
          end
        end
        reference_results = jaro_winkler_jaro_winkler_distance(candidates, test_input, min_score, winkler)
        results = results.sort{ |a, b| a[0] <=> b[0] }
        reference_results = reference_results.sort{ |a, b| a[0] <=> b[0] }
        assert_equal(results.size, reference_results.size)
        results.zip(reference_results).each do |res, expected_res|
          assert_equal(res[0], expected_res[0])
          # enforce precision up to 4th decimal point
          unless res[1].abs - expected_res[1].abs < 0.0001
            puts "Score incorrect for #{res.inspect} #{expected_res.inspect}"
            exit 1
          end
        end
      end
    end
  end
end

TestCorrectness.methods.select{ |m| m.to_s.start_with?('test_') }.each do |func|
  TestCorrectness.method(func).call
end
puts "All ok"
