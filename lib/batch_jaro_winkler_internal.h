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

#ifndef BATCH_JARO_WINKLER_INTERNAL_H
# define BATCH_JARO_WINKLER_INTERNAL_H

# include "batch_jaro_winkler.h"

// used for malloc, free
# include <stdlib.h>
// used for uint8_t, int16_t etc.
# include <stdint.h>
// used for ceil
# include <math.h>

# if BJW_USE_THREADS
#  ifdef _WIN32
#   include <windows.h>
#  else
#   include <pthread.h>
#  endif
# endif

# include "uthash.h"

typedef struct		s_char
{
	uint32_t		id;
	uint32_t		new_representation;
	UT_hash_handle	hh;
}					t_char;

typedef struct			s_tmp_candidate_occurrences
{
	uint32_t			id;
	void				*occ_indexes;
	uint32_t			occ_indexes_len;
	uint32_t			occ_indexes_size;
	// internal data used by uthash
	UT_hash_handle		hh;
}						t_tmp_candidate_occurrences;

typedef struct					s_char_occurrences
{
	// character represented as an int for uthash
	uint32_t					id;
	uint32_t					original_representation;
	t_tmp_candidate_occurrences	*candidates_occurrences;
	// internal data used by uthash
	UT_hash_handle				hh;
}								t_char_occurrences;

typedef struct			s_sorted_candidate
{
	uint32_t			original_ind;
	void				*candidate;
	uint32_t			char_width;
	float				min_score;
	uint32_t			candidate_length;
}						t_sorted_candidate;

typedef struct			s_thread_data
{
	void				*runtime_models;
	uint32_t			i_thread;
	uint32_t			original_char_width;
	void				*input;
	uint32_t			input_length;
	float				min_score;
	float				weight;
	float				threshold;
	char				both_min_score_and_min_scores;
	bjw_result			*results;
	uint32_t			nb_results;
}						t_thread_data;

#endif