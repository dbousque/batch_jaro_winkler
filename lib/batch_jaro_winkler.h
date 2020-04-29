/*
MIT License

Copyright (c) 2020 Dominik Bousquet https://github.com/dbousque/batch_jaro_winkler

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef BATCH_JARO_WINKLER_H
# define BATCH_JARO_WINKLER_H

# include <stdint.h>

// Set to 0 if you don't have access to POSIX threads or Windows threads
# define BJW_USE_THREADS 1

typedef struct
{
	void		*candidate;
	float		score;
	uint32_t	candidate_length;
} bjw_result;

// You can pass the resulting buffer around, you are responsible for freeing it.
void	*bjw_build_exportable_model(void **candidates, uint32_t char_width, uint32_t *candidates_lengths, uint32_t nb_candidates, float *min_scores, uint32_t nb_runtime_threads, uint32_t *res_model_size);

// Allocates buffers required for the runtime. You can use a runtime model multiple times.
void	*bjw_build_runtime_model(void *exportable_model);
void	bjw_free_runtime_model(void *runtime_model);

bjw_result	*bjw_jaro_winkler_distance(void *runtime_model, void *input, uint32_t input_length, float min_score, float weight, float threshold, uint32_t n_best_results, uint32_t *nb_results);
bjw_result	*bjw_jaro_distance(void *runtime_model, void *input, uint32_t input_length, float min_score, uint32_t n_best_results, uint32_t *nb_results);

#endif
