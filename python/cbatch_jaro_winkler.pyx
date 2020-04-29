

cimport cbatch_jaro_winkler_headers
from libc cimport stdint
from libc.stdlib cimport malloc, free
from libc.string cimport memcpy
from libc.stddef cimport wchar_t

cdef extern from "Python.h":
  char* PyBytes_AS_STRING(object)
  object PyBytes_FromStringAndSize(const char *v, Py_ssize_t len)

cdef class RuntimeModel:
  cdef object exportable_model
  cdef void* model

  def __cinit__(self, exportable_model):
    # We keep a reference because we use the candidates strings in the runtime model and
    # so we must guarantee that the exportable model is not garbage collected before the runtime model.
    self.exportable_model = exportable_model
    self.model = cbatch_jaro_winkler_headers.bjw_build_runtime_model(PyBytes_AS_STRING(exportable_model))
    if not self.model:
      raise RuntimeError('batch_jaro_winkler.build_runtime_model failed')

  def __dealloc__(self):
    cbatch_jaro_winkler_headers.bjw_free_runtime_model(self.model)

# candidates must follow one of these formats:
# - ['hi', 'hello']
# - [{ 'candidate': 'hi', 'min_score': 0.5 }, { 'candidate': 'hello', 'min_score': 0.8 }]
def build_exportable_model_bytes(char_width, candidates, nb_runtime_threads=1):
  if nb_runtime_threads < 1:
    raise ValueError('nb_runtime_threads must be > 0')
  candidates_encoded = char_width != 0
  if not candidates_encoded:
    char_width = 4
  if char_width != 1 and char_width != 2 and char_width != 4:
    raise ValueError('char_width must be 1, 2 or 4')
  nb_candidates = len(candidates)
  with_min_scores = False
  if nb_candidates > 0 and (type(candidates[0]) is not bytes and getattr(candidates[0], '__getitem__')) and 'min_score' in candidates[0]:
    with_min_scores = True
  c_candidates = <void**> malloc(sizeof(void*) * nb_candidates)
  c_candidates_lengths = <stdint.uint32_t*> malloc(sizeof(stdint.uint32_t) * nb_candidates)
  # Keep in python list also to guarantee that encoded strings are not garbage collected.
  _stored_candidates = []
  c_min_scores = <float*> malloc(sizeof(float) * nb_candidates) if with_min_scores else NULL
  for i_cand, cand in enumerate(candidates):
    cand_string = cand
    if with_min_scores:
      cand_string = cand['candidate']
      if cand['min_score'] < 0.0 or cand['min_score'] > 1.0:
        raise ValueError('min_score must be >= 0.0 and <= 1.0')
      c_min_scores[i_cand] = cand['min_score']
    if not candidates_encoded:
      cand_string = cand_string.encode('utf-32-le')
    _stored_candidates.append(cand_string)
    c_candidates[i_cand] = <void*> PyBytes_AS_STRING(cand_string)
    c_candidates_lengths[i_cand] = len(cand_string)
    c_candidates_lengths[i_cand] = <stdint.uint32_t> (c_candidates_lengths[i_cand] // char_width)

  cdef stdint.uint32_t exportable_model_size = 0
  exportable_model = cbatch_jaro_winkler_headers.bjw_build_exportable_model(c_candidates, char_width, c_candidates_lengths, nb_candidates, c_min_scores, nb_runtime_threads, &exportable_model_size)
  if not exportable_model:
    free(c_candidates)
    free(c_candidates_lengths)
    free(c_min_scores)
    raise RuntimeError('batch_jaro_winkler.build_exportable_model failed')
  # Makes a copy, would be better to directly use the buffer returned by the C function.
  cdef bytes res_exportable_model = (<stdint.uint8_t*> exportable_model)[:exportable_model_size]
  # Since we made a copy, we can free the buffer.
  free(exportable_model)
  free(c_candidates)
  free(c_candidates_lengths)
  free(c_min_scores)
  return res_exportable_model

def build_exportable_model(candidates, nb_runtime_threads=1):
  return build_exportable_model_bytes(0, candidates, nb_runtime_threads)

def build_runtime_model(exportable_model):
  return RuntimeModel(exportable_model)

cdef c_results_to_python(cbatch_jaro_winkler_headers.bjw_result *c_results, stdint.uint32_t nb_results, stdint.uint32_t char_width, char inp_encoded):
  cdef stdint.uint32_t total_candidates_length
  cdef stdint.uint32_t i_result
  cdef void *tmp_all_candidates
  cdef void *tmp_all_candidates_head
  cdef stdint.uint32_t cpy_size

  all_candidates = None
  if not inp_encoded:
    total_candidates_length = 0
    i_result = 0

    while i_result < nb_results:
      total_candidates_length += c_results[i_result].candidate_length
      i_result += 1

    tmp_all_candidates = malloc(char_width * total_candidates_length)
    tmp_all_candidates_head = tmp_all_candidates
    cpy_size = 0

    i_result = 0
    while i_result < nb_results:
      cpy_size = c_results[i_result].candidate_length * char_width
      memcpy(tmp_all_candidates_head, c_results[i_result].candidate, cpy_size)
      tmp_all_candidates_head += cpy_size
      i_result += 1

    all_candidates_pybytes = PyBytes_FromStringAndSize(<char*> tmp_all_candidates, char_width * total_candidates_length)
    free(tmp_all_candidates)
    all_candidates = all_candidates_pybytes.decode('utf-32-le')

  # Preallocate for small speedup
  results = [None] * nb_results
  i_result = 0
  cdef stdint.uint32_t candidate_length = 0
  cdef stdint.uint32_t candidate_decal = 0
  while i_result < nb_results:
    candidate_length = c_results[i_result].candidate_length
    if inp_encoded:
      candidate = PyBytes_FromStringAndSize(<char*> c_results[i_result].candidate, char_width * candidate_length)
    else:
      candidate = all_candidates[candidate_decal:(candidate_decal + candidate_length)]

    results[i_result] = (candidate, c_results[i_result].score)
    candidate_decal += candidate_length
    i_result += 1

  return results

def jaro_winkler_distance_bytes(char_width, RuntimeModel runtime_model, inp, min_score=None, weight=0.1, threshold=0.7, n_best_results=None):
  if min_score is not None and (min_score < 0.0 or min_score > 1.0):
    raise ValueError('min_score must be >= 0.0 and <= 1.0')
  if weight is not None and (weight < 0.0 or weight > 0.25):
    raise ValueError('weight must be >= 0.0 and <= 0.25')
  if threshold is not None and (threshold < 0.0 or threshold > 1.0):
    raise ValueError('threshold must be >= 0.0 and <= 1.0')
  if n_best_results is not None and n_best_results < 0:
    raise ValueError('n_best_results must be >= 0')
  if n_best_results == 0:
    return []
  if min_score is None:
    min_score = -1.0
  if weight is None:
    weight = -1.0
  if threshold is None:
    threshold = -1.0
  if n_best_results is None:
    n_best_results = 0

  inp_encoded = char_width != 0
  if not inp_encoded:
    char_width = 4
  if char_width != 1 and char_width != 2 and char_width != 4:
    raise ValueError('char_width must be 1, 2 or 4')

  if not inp_encoded:
    inp = inp.encode('utf-32-le')
  # We explicitely cast before the GIL release because we don't want to touch python objects with the GIL released
  cdef void *c_inp = <void*> PyBytes_AS_STRING(inp)
  cdef stdint.uint32_t c_input_length = len(inp)
  c_input_length = <stdint.uint32_t> (c_input_length // char_width)
  cdef void *c_runtime_model = runtime_model.model
  cdef float c_min_score = min_score
  cdef float c_weight = weight
  cdef float c_threshold = threshold
  cdef stdint.uint32_t c_n_best_results = n_best_results
  cdef stdint.uint32_t nb_results = 0
  cdef cbatch_jaro_winkler_headers.bjw_result *c_results = NULL
  with nogil:
    c_results = cbatch_jaro_winkler_headers.bjw_jaro_winkler_distance(c_runtime_model, c_inp, c_input_length, c_min_score, c_weight, c_threshold, c_n_best_results, &nb_results)
  if not c_results:
    raise RuntimeError('batch_jaro_winkler.jaro_winkler_distance failed')

  results = c_results_to_python(c_results, nb_results, char_width, inp_encoded)

  free(c_results)
  return results

def jaro_winkler_distance(RuntimeModel runtime_model, inp, min_score=None, weight=0.1, threshold=0.7, n_best_results=None):
  return jaro_winkler_distance_bytes(0, runtime_model, inp, min_score=min_score, weight=weight, threshold=threshold, n_best_results=n_best_results)

def jaro_distance_bytes(char_width, RuntimeModel runtime_model, inp, min_score=None, n_best_results=None):
  return jaro_winkler_distance_bytes(char_width, runtime_model, inp, min_score=min_score, weight=None, threshold=None, n_best_results=n_best_results)

def jaro_distance(RuntimeModel runtime_model, inp, min_score=None, n_best_results=None):
  return jaro_distance_bytes(0, runtime_model, inp, min_score=min_score, n_best_results=n_best_results)
