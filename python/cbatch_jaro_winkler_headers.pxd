

import os
from libc cimport stdint

cdef extern from "ext/batch_jaro_winkler.h":
  ctypedef struct bjw_result:
    void *candidate
    float score
    stdint.uint32_t candidate_length

  void *bjw_build_exportable_model(void **candidates, stdint.uint32_t char_width, stdint.uint32_t *candidates_lengths, stdint.uint32_t nb_candidates, float *min_scores, stdint.uint32_t nb_runtime_threads, stdint.uint32_t *res_model_size);

  void *bjw_build_runtime_model(void *exportable_model);
  void bjw_free_runtime_model(void *runtime_model);

  bjw_result *bjw_jaro_winkler_distance(void *runtime_model, void *input, stdint.uint32_t input_length, float min_score, float weight, float threshold, stdint.uint32_t n_best_results, stdint.uint32_t *nb_results) nogil;