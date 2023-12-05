

require 'ffi'
require 'batch_jaro_winkler/batch_jaro_winkler'
require 'batch_jaro_winkler/version'

# Memory leak with older MRI versions which you can reproduce with the following program (at least on macOS 10.15.3 19D76):
# while true do
#   1000.times do
#     # random 10 characters string
#     str = (0...10).map{ (65 + rand(26)).chr }.join
#     str.encode('utf-32le')
#   end
#   GC.start(full_mark: true, immediate_sweep: true)
#   GC.start
# end
# Change utf-32le to utf-32 to watch to memory leak vanish
# This is the fix that was deployed: https://github.com/ruby/ruby/compare/v2_6_4...v2_6_5#diff-7a2f2c7dfe0bf61d38272aeaf68ac768R2117
def version_with_memory_leak?(version)
  major, minor, patch = version.split('.')
  memory_leak = false
  if !major.nil? && major.to_i <= 2 && !minor.nil? && minor.to_i <= 6
    major, minor, patch = major.to_i, minor.to_i, patch.to_i
    memory_leak = true
    if major == 2 && minor == 6 && patch >= 5
      memory_leak = false
    elsif major == 2 && minor == 5 && patch >= 8
      memory_leak = false
    end
  end
  memory_leak
end

def encode_utf32_le_without_memory_leak(str)
  str = str.encode('utf-32')
  # Ignore BOM, restore correct byte order within codepoint
  str = (str.bytes[4..-1] || []).map(&:chr).each_slice(4).map{ |c| c.reverse }.flatten.join
  str.force_encoding('utf-32le')
  str
end

module BatchJaroWinkler
  extend FFI::Library
  ffi_lib FFI::CURRENT_PROCESS

  class BjwResult < FFI::Struct
    layout :candidate, :pointer,
      :score, :float,
      :candidate_length, :uint32
  end

  class RuntimeModel
    attr_reader :model

    def initialize(exportable_model)
       # We keep a reference because we use the candidates strings in the runtime model and
      # so we must guarantee that the exportable model is not garbage collected before the runtime model.
      @exportable_model = exportable_model
      @model = BatchJaroWinkler.bjw_build_runtime_model(exportable_model)
      raise 'batch_jaro_winkler.build_runtime_model failed' if @model.nil?
      # Makes a call to bjw_free_runtime_model when the runtime model is GC'd
      @_gced_model = FFI::AutoPointer.new(@model, BatchJaroWinkler.method(:bjw_free_runtime_model))
    end
  end

  attach_function :bjw_build_exportable_model, [:pointer, :uint32, :pointer, :uint32, :pointer, :uint32, :pointer], :pointer
  attach_function :bjw_build_runtime_model, [:buffer_in], :pointer
  attach_function :bjw_free_runtime_model, [:pointer], :void
  attach_function :bjw_jaro_winkler_distance, [:pointer, :buffer_in, :uint32, :float, :float, :float, :uint32, :pointer], :pointer
  # Alias to 'free'
  attach_function :_bjw_free, [:pointer], :void

  # Automatically freed when the block closes
  def self.allocate_c_data(nb_candidates, with_min_scores)
    FFI::MemoryPointer.new(:uint32, 1, false) do |exportable_model_size|
      FFI::MemoryPointer.new(:pointer, nb_candidates, false) do |c_candidates|
        FFI::MemoryPointer.new(:uint32, nb_candidates, false) do |c_candidates_lengths|
          return yield([exportable_model_size, c_candidates, c_candidates_lengths, nil]) unless with_min_scores
          FFI::MemoryPointer.new(:float, nb_candidates, false) do |c_min_scores|
            yield([exportable_model_size, c_candidates, c_candidates_lengths, c_min_scores])
          end
        end
      end
    end
  end

  # inp_candidates must follow one of these formats:
  # - ['hi', 'hello']
  # - [{ candidate: 'hi', min_score: 0.5 }, { candidate: 'hello', min_score: 0.8 }]
  def self.build_exportable_model_bytes(char_width, candidates, opts = {})
    current_version_has_memory_leak = version_with_memory_leak?(RUBY_VERSION)
    nb_runtime_threads = opts[:nb_runtime_threads] || 1
    if nb_runtime_threads < 1
      raise ArgumentError.new('nb_runtime_threads must be > 0')
    end
    candidates_encoded = char_width != 0
    char_width = 4 unless candidates_encoded
    if char_width != 1 && char_width != 2 && char_width != 4
      raise ArgumentError.new('char_width must be 1, 2 or 4')
    end
    # float size is platform dependent, so don't rely on it being 4
    float_size = 0
    FFI::MemoryPointer.new(:float, 1, false) do |one_float|
      float_size = one_float.size
    end
    nb_candidates = candidates.size
    with_min_scores = false
    if nb_candidates > 0 && candidates[0].respond_to?(:each_pair) && candidates[0].key?(:min_score)
      with_min_scores = true
    end

    exportable_model = nil
    allocate_c_data(nb_candidates, with_min_scores) do |(exportable_model_size, c_candidates, c_candidates_lengths, c_min_scores)|
      # Keep in ruby array also to guarantee that encoded strings are not garbage collected.
      _stored_candidates = Array.new(nb_candidates)
      candidates.each_with_index do |cand, i_cand|
        cand_string = cand
        if with_min_scores
          cand_string = cand[:candidate]
          if cand[:min_score] < 0.0 or cand[:min_score] > 1.0
            raise 'min_score must be >= 0.0 and <= 1.0'
          end
          c_min_scores.put(:float, i_cand * float_size, cand[:min_score])
        end
        unless candidates_encoded
          cand_string = current_version_has_memory_leak ? encode_utf32_le_without_memory_leak(cand_string) : cand_string.encode('utf-32le')
        end
        cand_string.force_encoding('ascii')
        cand_length = cand_string.size / char_width
        cand_string = FFI::MemoryPointer.from_string(cand_string)
        _stored_candidates[i_cand] = cand_string
        c_candidates.put(:pointer, i_cand * FFI::Pointer.size, cand_string)
        # sizeof(uint32_t) = 4
        c_candidates_lengths.put(:uint32, i_cand * 4, cand_length)
      end

      exportable_model = BatchJaroWinkler.bjw_build_exportable_model(c_candidates, char_width, c_candidates_lengths, nb_candidates, c_min_scores, nb_runtime_threads, exportable_model_size)
      next unless exportable_model

      # Will free the raw C exportable model when GC'd
      _gced_exportable_model = FFI::AutoPointer.new(exportable_model, BatchJaroWinkler.method(:_bjw_free))
      exportable_model = exportable_model.read_string(exportable_model_size.get(:uint32, 0))
    end

    raise 'batch_jaro_winkler.build_exportable_model failed' unless exportable_model
    exportable_model
  end

  def self.build_exportable_model(candidates, opts = {})
    BatchJaroWinkler.build_exportable_model_bytes(0, candidates, opts)
  end

  def self.build_runtime_model(exportable_model)
    RuntimeModel.new(exportable_model)
  end

  def self.jaro_winkler_distance_bytes(char_width, runtime_model, inp, opts = {})
    return [] if opts[:n_best_results] == 0
    current_version_has_memory_leak = version_with_memory_leak?(RUBY_VERSION)
    opts[:weight] = 0.1 unless opts.key?(:weight)
    opts[:threshold] = 0.7 unless opts.key?(:threshold)
    opts[:n_best_results] = 0 unless opts[:n_best_results]

    if !(opts[:min_score].nil?) && (opts[:min_score] < 0.0 || opts[:min_score] > 1.0)
      raise ArgumentError.new('min_score must be >= 0.0 and <= 1.0')
    end
    if !(opts[:weight].nil?) && (opts[:weight] < 0.0 || opts[:weight] > 0.25)
      raise ArgumentError.new('weight must be >= 0.0 and <= 0.25')
    end
    if !(opts[:threshold].nil?) && (opts[:threshold] < 0.0 || opts[:threshold] > 1.0)
      raise ArgumentError.new('threshold must be >= 0.0 and <= 1.0')
    end
    if opts[:n_best_results] < 0
      raise ArgumentError.new('n_best_results must be >= 0')
    end
    opts[:min_score] = -1.0 if opts[:min_score].nil?
    opts[:weight] = -1.0 if opts[:weight].nil?
    opts[:threshold] = -1.0 if opts[:threshold].nil?

    inp_encoded = char_width != 0
    char_width = 4 unless inp_encoded
    if char_width != 1 && char_width != 2 && char_width != 4
      raise ArgumentError.new('char_width must be 1, 2 or 4')
    end

    unless inp_encoded
      inp = current_version_has_memory_leak ? encode_utf32_le_without_memory_leak(inp) : inp.encode('utf-32le')
    end
    inp.force_encoding('ascii')
    c_results = nil
    nb_results = nil
    FFI::MemoryPointer.new(:uint32, 1, false) do |c_nb_results|
      c_results = BatchJaroWinkler.bjw_jaro_winkler_distance(runtime_model.model, inp, inp.size / char_width, opts[:min_score], opts[:weight], opts[:threshold], opts[:n_best_results], c_nb_results)
      nb_results = c_nb_results.get(:uint32, 0)
    end
    raise 'batch_jaro_winkler.jaro_winkler_distance failed' unless c_results

    # Will free the raw C results when GC'd
    _gced_results = FFI::AutoPointer.new(c_results, BatchJaroWinkler.method(:_bjw_free))
    c_results_address = c_results.address
    c_results = FFI::Pointer.new(BjwResult, c_results)

    native_conversion = true
    begin
      BatchJaroWinkler.method(:rb_bjw_build_runtime_result)
    rescue NameError
      native_conversion = false
    end

    if native_conversion
      res = []
      ok = BatchJaroWinkler.rb_bjw_build_runtime_result([], res, c_results_address, nb_results, inp_encoded, char_width)
      raise 'rb_bjw_build_runtime_result failed' unless ok
      res
    else
      # standard slow ffi version
      Array.new(nb_results) do |i_result|
        res = BjwResult.new(c_results[i_result])
        candidate = res[:candidate].read_string(res[:candidate_length] * char_width)
        unless inp_encoded
          candidate.force_encoding('utf-32le')
          candidate = candidate.encode('utf-8')
        end
        [candidate, res[:score]]
      end
    end
  end

  def self.jaro_winkler_distance(runtime_model, inp, opts = {})
    BatchJaroWinkler.jaro_winkler_distance_bytes(0, runtime_model, inp, opts)
  end

  def self.jaro_distance_bytes(char_width, runtime_model, inp, opts = {})
    opts[:weight] = nil
    opts[:threshold] = nil
    BatchJaroWinkler.jaro_winkler_distance_bytes(char_width, runtime_model, inp, opts)
  end

  def self.jaro_distance(runtime_model, inp, opts = {})
    BatchJaroWinkler.jaro_distance_bytes(0, runtime_model, inp, opts)
  end
end
