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

#include "batch_jaro_winkler_internal.h"

#define BJW_CHAR_TYPE uint32_t
#define BJW_CHAR_ACCESS_TYPE uint32_t
#include "batch_jaro_winkler_runtime.h"

#undef BJW_CHAR_ACCESS_TYPE
#define BJW_CHAR_ACCESS_TYPE uint16_t
#include "batch_jaro_winkler_runtime.h"

#undef BJW_CHAR_ACCESS_TYPE
#define BJW_CHAR_ACCESS_TYPE uint8_t
#include "batch_jaro_winkler_runtime.h"

#undef BJW_CHAR_TYPE
#undef BJW_CHAR_ACCESS_TYPE
#define BJW_CHAR_TYPE uint16_t
#define BJW_CHAR_ACCESS_TYPE uint32_t
#include "batch_jaro_winkler_runtime.h"

#undef BJW_CHAR_ACCESS_TYPE
#define BJW_CHAR_ACCESS_TYPE uint16_t
#include "batch_jaro_winkler_runtime.h"

#undef BJW_CHAR_ACCESS_TYPE
#define BJW_CHAR_ACCESS_TYPE uint8_t
#include "batch_jaro_winkler_runtime.h"

#undef BJW_CHAR_TYPE
#undef BJW_CHAR_ACCESS_TYPE
#define BJW_CHAR_TYPE uint8_t
#define BJW_CHAR_ACCESS_TYPE uint32_t
#include "batch_jaro_winkler_runtime.h"

#undef BJW_CHAR_ACCESS_TYPE
#define BJW_CHAR_ACCESS_TYPE uint16_t
#include "batch_jaro_winkler_runtime.h"

#undef BJW_CHAR_ACCESS_TYPE
#define BJW_CHAR_ACCESS_TYPE uint8_t
#include "batch_jaro_winkler_runtime.h"

#undef BJW_CHAR_TYPE
#undef BJW_CHAR_ACCESS_TYPE

static inline uint32_t	sorted_candidate_char_at(t_sorted_candidate *sorted_candidate, uint32_t i)
{
	uint32_t	res;

	if (sorted_candidate->char_width == 4)
		res = ((uint32_t*)sorted_candidate->candidate)[i];
	else if (sorted_candidate->char_width == 2)
		res = ((uint16_t*)sorted_candidate->candidate)[i];
	else
		res = ((uint8_t*)sorted_candidate->candidate)[i];
	return (res);
}

static int		sort_by_length_and_alphabetical_order(const void *void_cand1, const void *void_cand2)
{
	t_sorted_candidate	*cand1;
	t_sorted_candidate	*cand2;
	uint32_t			i;

	cand1 = (t_sorted_candidate*)void_cand1;
	cand2 = (t_sorted_candidate*)void_cand2;
	if (cand1->candidate_length < cand2->candidate_length)
		return (-1);
	if (cand1->candidate_length > cand2->candidate_length)
		return (1);
	for (i = 0; i < cand1->candidate_length && i < cand2->candidate_length && sorted_candidate_char_at(cand1, i) == sorted_candidate_char_at(cand2, i); i++){}
	return (
		i >= cand1->candidate_length && i >= cand2->candidate_length ? 0 :
		i >= cand1->candidate_length ? -1 :
		i >= cand2->candidate_length ? 1 :
		sorted_candidate_char_at(cand1, i) < sorted_candidate_char_at(cand2, i) ? -1 :
		1
	);
}

static void		free_char_occurrences(t_char_occurrences *char_occurrences)
{
	t_char_occurrences			*tmp_char_occurrence;
	t_char_occurrences			*tmp1;
	t_tmp_candidate_occurrences	*candidate_occurrences;
	t_tmp_candidate_occurrences	*tmp_candidate_occurrences;
	t_tmp_candidate_occurrences	*tmp2;

	HASH_ITER(hh, char_occurrences, tmp_char_occurrence, tmp1)
	{
		HASH_DEL(char_occurrences, tmp_char_occurrence);
		candidate_occurrences = tmp_char_occurrence->candidates_occurrences;
		HASH_ITER(hh, candidate_occurrences, tmp_candidate_occurrences, tmp2)
		{
			HASH_DEL(candidate_occurrences, tmp_candidate_occurrences);
			free(tmp_candidate_occurrences->occ_indexes);
			free(tmp_candidate_occurrences);
		}
		free(tmp_char_occurrence);
	}
}

static void		*exit_build_exportable_model_for_thread_error(t_sorted_candidate *sorted_candidates, t_char_occurrences *char_occurrences)
{
	free(sorted_candidates);
	free_char_occurrences(char_occurrences);
	return (NULL);
}

static uint8_t	*build_exportable_model_for_thread(
	void **original_candidates, uint32_t original_char_width, void **compressed_candidates, uint32_t compressed_char_width,
	uint32_t char_access_width, uint32_t *candidates_lengths, uint32_t nb_candidates, float *min_scores, uint32_t *res_model_size
)
{
	uint32_t					i_candidate;
	uint32_t					i_char;
	uint32_t					i_occurrence;
	uint32_t					i_candidate_occurrrence;
	// important to set to NULL for uthash
	t_char_occurrences			*char_occurrences = NULL;
	t_char_occurrences			*char_occurrence;
	t_tmp_candidate_occurrences	*candidate_occurrences;
	uint32_t					key;
	uint32_t					total_candidates_lengths;
	uint32_t					nb_char_matches;
	uint32_t					nb_candidate_occurrences;
	uint8_t						*model;
	t_sorted_candidate			*sorted_candidates;
	uint32_t					store_original_candidates;

	store_original_candidates = original_candidates != compressed_candidates ? 1 : 0;
	sorted_candidates = malloc(sizeof(t_sorted_candidate) * nb_candidates);
	if (!sorted_candidates)
		return (NULL);
	for (i_candidate = 0; i_candidate < nb_candidates; i_candidate++)
	{
		sorted_candidates[i_candidate] = (t_sorted_candidate){
			.original_ind = i_candidate,
			.candidate = compressed_candidates[i_candidate],
			.char_width = compressed_char_width,
			.min_score = min_scores ? min_scores[i_candidate] : -1.0f,
			.candidate_length = candidates_lengths[i_candidate]
		};
	}

	// we sort to improve the runtime memory access pattern
	qsort(sorted_candidates, nb_candidates, sizeof(t_sorted_candidate), &sort_by_length_and_alphabetical_order);

	nb_char_matches = 0;
	nb_candidate_occurrences = 0;
	total_candidates_lengths = 0;
	for (i_candidate = 0; i_candidate < nb_candidates; i_candidate++)
	{
		for (i_char = 0; i_char < sorted_candidates[i_candidate].candidate_length; i_char++)
		{
			// Find character matches
			if (compressed_char_width == 4)
				key = (uint32_t)(((uint32_t*)sorted_candidates[i_candidate].candidate)[i_char]);
			else if (compressed_char_width == 2)
				key = (uint32_t)(((uint16_t*)sorted_candidates[i_candidate].candidate)[i_char]);
			else
				key = (uint32_t)(((uint8_t*)sorted_candidates[i_candidate].candidate)[i_char]);
			char_occurrence = NULL;
			HASH_FIND(hh, char_occurrences, &key, sizeof(uint32_t), char_occurrence);
			if (!char_occurrence) {
				nb_char_matches++;
				if (!(char_occurrence = malloc(sizeof(t_char_occurrences))))
					return (exit_build_exportable_model_for_thread_error(sorted_candidates, char_occurrences));
				char_occurrence->id = key;
				// important to set to NULL for uthash
				char_occurrence->candidates_occurrences = NULL;
				if (store_original_candidates)
				{
					if (original_char_width == 4)
						char_occurrence->original_representation = (uint32_t)(((uint32_t*)original_candidates[sorted_candidates[i_candidate].original_ind])[i_char]);
					else if (original_char_width == 2)
						char_occurrence->original_representation = (uint32_t)(((uint16_t*)original_candidates[sorted_candidates[i_candidate].original_ind])[i_char]);
					else
						char_occurrence->original_representation = (uint32_t)(((uint8_t*)original_candidates[sorted_candidates[i_candidate].original_ind])[i_char]);
				}
				HASH_ADD(hh, char_occurrences, id, sizeof(uint32_t), char_occurrence);
			}

			// Find character occurences for this candidate
			key = i_candidate;
			candidate_occurrences = NULL;
			HASH_FIND(hh, char_occurrence->candidates_occurrences, &key, sizeof(uint32_t), candidate_occurrences);
			if (!candidate_occurrences)
			{
				nb_candidate_occurrences++;
				if (!(candidate_occurrences = malloc(sizeof(t_tmp_candidate_occurrences))))
					return (exit_build_exportable_model_for_thread_error(sorted_candidates, char_occurrences));
				candidate_occurrences->id = key;
				candidate_occurrences->occ_indexes_len = 0;
				candidate_occurrences->occ_indexes_size = 32;
				candidate_occurrences->occ_indexes = malloc(char_access_width * candidate_occurrences->occ_indexes_size);
				if (!candidate_occurrences->occ_indexes)
				{
					free(candidate_occurrences);
					return (exit_build_exportable_model_for_thread_error(sorted_candidates, char_occurrences));
				}
				HASH_ADD(hh, char_occurrence->candidates_occurrences, id, sizeof(uint32_t), candidate_occurrences);
			}

			// Not big enough, increase size
			if (candidate_occurrences->occ_indexes_len == candidate_occurrences->occ_indexes_size)
			{
				void *new_occ_indexes = malloc(char_access_width * candidate_occurrences->occ_indexes_size * 2);
				if (!new_occ_indexes)
					return (exit_build_exportable_model_for_thread_error(sorted_candidates, char_occurrences));
				memcpy(new_occ_indexes, candidate_occurrences->occ_indexes, char_access_width * candidate_occurrences->occ_indexes_size);
				candidate_occurrences->occ_indexes_size *= 2;
				free(candidate_occurrences->occ_indexes);
				candidate_occurrences->occ_indexes = new_occ_indexes;
			}

			if (char_access_width == 4)
				((uint32_t*)candidate_occurrences->occ_indexes)[candidate_occurrences->occ_indexes_len] = i_char;
			if (char_access_width == 2)
				((uint16_t*)candidate_occurrences->occ_indexes)[candidate_occurrences->occ_indexes_len] = i_char;
			else
				((uint8_t*)candidate_occurrences->occ_indexes)[candidate_occurrences->occ_indexes_len] = i_char;
			candidate_occurrences->occ_indexes_len++;
		}
		total_candidates_lengths += sorted_candidates[i_candidate].candidate_length;
	}

	// candidate_ind + nb_occurrences
	uint32_t metadata_size = (sizeof(uint32_t) + char_access_width) * nb_candidate_occurrences;
	uint32_t indexes_size = char_access_width * total_candidates_lengths;
	uint32_t occurrences_size = metadata_size + indexes_size;

	uint32_t total_size =
		sizeof(uint32_t) +																// nb_candidates
		sizeof(uint32_t) +																// total_candidates_lengths
		sizeof(uint32_t) +																// min_scores present or not (uint32_t used to keep 4 bytes alignment)
		sizeof(uint32_t) +																// nb_char_matches
		sizeof(uint32_t) +																// nb_candidate_occurrences
		sizeof(uint32_t) +																// store_original_candidates
		sizeof(uint32_t) * nb_candidates * (min_scores ? 1 : 0) +						// min_scores - can go from 0.0 to 1.0 -> convert to uint32_t for cross-platform support
		sizeof(uint32_t) * nb_char_matches +											// chars_occurrences_decals
		sizeof(uint32_t) * nb_char_matches +											// nb_candidates_per_char_match
		sizeof(uint32_t) * (nb_candidates + 1) +										// candidates_decal
		original_char_width * total_candidates_lengths * store_original_candidates +	// original_candidates (if store_original_candidates)
		compressed_char_width * total_candidates_lengths +								// candidates (compressed)
		original_char_width * nb_char_matches * store_original_candidates +				// original_chars (if store_original_candidates)
		compressed_char_width * nb_char_matches +										// chars
		occurrences_size; 																// occurrences

	if (!(model = malloc(total_size)))
		return (exit_build_exportable_model_for_thread_error(sorted_candidates, char_occurrences));
	uint8_t *res_buffer_head = model;
	*((uint32_t*)res_buffer_head) = nb_candidates;
	res_buffer_head += sizeof(uint32_t);
	*((uint32_t*)res_buffer_head) = total_candidates_lengths;
	res_buffer_head += sizeof(uint32_t);
	*((uint32_t*)res_buffer_head) = min_scores ? 1 : 0;
	res_buffer_head += sizeof(uint32_t);
	*((uint32_t*)res_buffer_head) = nb_char_matches;
	res_buffer_head += sizeof(uint32_t);
	*((uint32_t*)res_buffer_head) = nb_candidate_occurrences;
	res_buffer_head += sizeof(uint32_t);
	*((uint32_t*)res_buffer_head) = store_original_candidates;
	res_buffer_head += sizeof(uint32_t);

	if (min_scores)
	{
		uint32_t *res_min_scores = (uint32_t*)res_buffer_head;
		for (i_candidate = 0; i_candidate < nb_candidates; i_candidate++)
		{
			// To prevent rounding errors when min_score == 1.0f
			if (sorted_candidates[i_candidate].min_score >= 1.0f)
				res_min_scores[i_candidate] = UINT32_MAX;
			else
				res_min_scores[i_candidate] = (uint32_t)(sorted_candidates[i_candidate].min_score * UINT32_MAX);
		}
		res_buffer_head += sizeof(uint32_t) * nb_candidates;
	}

	uint32_t *chars_occurrences_decals = (uint32_t*)res_buffer_head;
	res_buffer_head += sizeof(uint32_t) * nb_char_matches;
	uint32_t *nb_candidates_per_char_match = (uint32_t*)res_buffer_head;
	res_buffer_head += sizeof(uint32_t) * nb_char_matches;

	uint32_t *candidates_decal = (uint32_t*)res_buffer_head;
	uint32_t decal = 0;
	for (i_candidate = 0; i_candidate < nb_candidates; i_candidate++)
	{
		candidates_decal[i_candidate] = decal;
		decal += sorted_candidates[i_candidate].candidate_length;
	}
	candidates_decal[i_candidate] = decal;
	res_buffer_head += sizeof(uint32_t) * (nb_candidates + 1);

	void *res_original_candidates = res_buffer_head;
	res_buffer_head += original_char_width * total_candidates_lengths * store_original_candidates;
	void *res_compressed_candidates = res_buffer_head;
	res_buffer_head += compressed_char_width * total_candidates_lengths;
	uint32_t candidates_char_decal = 0;
	for (i_candidate = 0; i_candidate < nb_candidates; i_candidate++)
	{
		if (store_original_candidates)
		{
			memcpy(
				res_original_candidates + (candidates_char_decal * original_char_width),
				original_candidates[sorted_candidates[i_candidate].original_ind],
				sorted_candidates[i_candidate].candidate_length * original_char_width
			);
		}
		memcpy(
			res_compressed_candidates + (candidates_char_decal * compressed_char_width),
			sorted_candidates[i_candidate].candidate,
			sorted_candidates[i_candidate].candidate_length * compressed_char_width
		);
		candidates_char_decal += sorted_candidates[i_candidate].candidate_length;
	}

	void *original_chars = res_buffer_head;
	res_buffer_head += original_char_width * nb_char_matches * store_original_candidates;
	void *chars = res_buffer_head;
	res_buffer_head += compressed_char_width * nb_char_matches;

	uint8_t *occurrences = (uint8_t*)res_buffer_head;
	uint8_t *occurrences_head = occurrences;

	i_char = 0;
	for (char_occurrence = char_occurrences; char_occurrence; char_occurrence = char_occurrence->hh.next)
	{
		if (store_original_candidates)
		{
			if (original_char_width == 4)
				((uint32_t*)original_chars)[i_char] = char_occurrence->original_representation;
			else if (original_char_width == 2)
				((uint16_t*)original_chars)[i_char] = char_occurrence->original_representation;
			else
				((uint8_t*)original_chars)[i_char] = char_occurrence->original_representation;
		}
		if (compressed_char_width == 4)
			((uint32_t*)chars)[i_char] = (uint32_t)char_occurrence->id;
		else if (compressed_char_width == 2)
			((uint16_t*)chars)[i_char] = (uint16_t)char_occurrence->id;
		else
			((uint8_t*)chars)[i_char] = (uint8_t)char_occurrence->id;
		chars_occurrences_decals[i_char] = occurrences_head - occurrences;

		i_candidate_occurrrence = 0;
		for (candidate_occurrences = char_occurrence->candidates_occurrences; candidate_occurrences; candidate_occurrences = candidate_occurrences->hh.next)
		{
			// 1 uint32_t for the candidate's index
			// + 1 BJW_CHAR_ACCESS_TYPE for the number of occurrences
			// + N BJW_CHAR_ACCESS_TYPE for the occurrences indexes
			*((uint32_t*)occurrences_head) = (uint32_t)candidate_occurrences->id;
			occurrences_head += sizeof(uint32_t);
			if (char_access_width == 4)
				*((uint32_t*)occurrences_head) = candidate_occurrences->occ_indexes_len;
			else if (char_access_width == 2)
				*((uint16_t*)occurrences_head) = candidate_occurrences->occ_indexes_len;
			else
				*((uint8_t*)occurrences_head) = candidate_occurrences->occ_indexes_len;
			occurrences_head += char_access_width;
			for (i_occurrence = 0; i_occurrence < candidate_occurrences->occ_indexes_len; i_occurrence++)
			{
				if (char_access_width == 4)
					*((uint32_t*)occurrences_head) = ((uint32_t*)candidate_occurrences->occ_indexes)[i_occurrence];
				if (char_access_width == 2)
					*((uint16_t*)occurrences_head) = ((uint16_t*)candidate_occurrences->occ_indexes)[i_occurrence];
				else
					*((uint8_t*)occurrences_head) = ((uint8_t*)candidate_occurrences->occ_indexes)[i_occurrence];
				occurrences_head += char_access_width;
			}

			i_candidate_occurrrence++;
		}

		nb_candidates_per_char_match[i_char] = i_candidate_occurrrence;
		i_char++;
	}

	*res_model_size = total_size;
	free_char_occurrences(char_occurrences);
	free(sorted_candidates);

	return (model);
}

// pack all result data in single buffer
static void		*build_exportable_model(
	void **original_candidates, uint32_t original_char_width, void **compressed_candidates, uint32_t compressed_char_width,
	uint32_t char_access_width, uint32_t *candidates_lengths, uint32_t nb_candidates, float *min_scores, uint32_t nb_runtime_threads, uint32_t *res_model_size
)
{
	uint32_t			i_thread;
	uint32_t			i;
	uint8_t				*model_per_thread[nb_runtime_threads];
	uint32_t			model_size_per_thread[nb_runtime_threads];
	uint8_t				*res_buffer;
	uint8_t				*res_buffer_head;
	void				**original_candidates_for_thread;
	void				**compressed_candidates_for_thread;
	uint32_t			*candidates_lengths_for_thread;
	uint32_t			nb_candidates_for_thread;
	float				*min_scores_for_thread;
	uint32_t			nb_taken_candidates;
	uint32_t			aligned_model_size;

	nb_taken_candidates = 0;
	*res_model_size = 0;
	for (i_thread = 0; i_thread < nb_runtime_threads; i_thread++)
	{
		nb_candidates_for_thread = i_thread == nb_runtime_threads - 1 ? nb_candidates - nb_taken_candidates : (nb_candidates / nb_runtime_threads);
		original_candidates_for_thread = original_candidates + nb_taken_candidates;
		compressed_candidates_for_thread = compressed_candidates + nb_taken_candidates;
		candidates_lengths_for_thread = candidates_lengths + nb_taken_candidates;
		min_scores_for_thread = min_scores ? min_scores + nb_taken_candidates : NULL;

		model_per_thread[i_thread] = build_exportable_model_for_thread(
			original_candidates_for_thread, original_char_width, compressed_candidates_for_thread, compressed_char_width,
			char_access_width, candidates_lengths_for_thread, nb_candidates_for_thread, min_scores_for_thread, &(model_size_per_thread[i_thread])
		);
		if (!model_per_thread[i_thread])
		{
			for (i = 0; i < i_thread; i++)
				free(model_per_thread[i]);
			return (NULL);
		}
		// align on next 4 byte boundary
		*res_model_size += model_size_per_thread[i_thread] + (4 - (model_size_per_thread[i_thread] % 4));
		nb_taken_candidates += nb_candidates_for_thread;
	}

	// we put the number of threads + nb candidates + char_width + char_access_width + original_char_width and the models per thread sizes at the start of the model
	*res_model_size += sizeof(uint32_t) * (nb_runtime_threads + 5);
	res_buffer = malloc(*res_model_size);
	if (!res_buffer)
	{
		for (i_thread = 0; i_thread < nb_runtime_threads; i_thread++)
			free(model_per_thread[i_thread]);
		return (NULL);
	}
	*((uint32_t*)(res_buffer + sizeof(uint32_t) * 0)) = nb_runtime_threads;
	*((uint32_t*)(res_buffer + sizeof(uint32_t) * 1)) = nb_candidates;
	*((uint32_t*)(res_buffer + sizeof(uint32_t) * 2)) = compressed_char_width;
	*((uint32_t*)(res_buffer + sizeof(uint32_t) * 3)) = char_access_width;
	*((uint32_t*)(res_buffer + sizeof(uint32_t) * 4)) = original_char_width;
	res_buffer_head = res_buffer + sizeof(uint32_t) * (nb_runtime_threads + 5);
	for (i_thread = 0; i_thread < nb_runtime_threads; i_thread++)
	{
		// align on next 4 byte boundary
		aligned_model_size = model_size_per_thread[i_thread] + (4 - (model_size_per_thread[i_thread] % 4));
		*((uint32_t*)(res_buffer + sizeof(uint32_t) * (i_thread + 5))) = aligned_model_size;
		memcpy(res_buffer_head, model_per_thread[i_thread], model_size_per_thread[i_thread]);
		// align on next 4 byte boundary
		res_buffer_head += aligned_model_size;
		free(model_per_thread[i_thread]);
	}

	return (res_buffer);
}

static void		free_chars(t_char *chars)
{
	t_char	*tmp_char;
	t_char	*tmp;

	HASH_ITER(hh, chars, tmp_char, tmp)
	{
		HASH_DEL(chars, tmp_char);
		free(tmp_char);
	}
}

// Used by the ruby library
void	_bjw_free(void *ptr)
{
	free(ptr);
}

// Pack all result data in single buffer.
void	*bjw_build_exportable_model(void **candidates, uint32_t char_width, uint32_t *candidates_lengths, uint32_t nb_candidates, float *min_scores, uint32_t nb_runtime_threads, uint32_t *res_model_size)
{
	uint32_t	i_candidate;
	uint32_t	i_char;
	uint32_t	i;
	uint32_t	longest_candidate;
	// important to set to NULL for uthash
	t_char		*chars = NULL;
	t_char		*chr;
	uint32_t	key;
	uint32_t	nb_chars;
	uint32_t	compressed_char_width;
	uint32_t	char_access_width;
	void		**compressed_candidates;
	void		*exportable_model;

	nb_chars = 0;
	longest_candidate = 0;
	for (i_candidate = 0; i_candidate < nb_candidates; i_candidate++)
	{
		if (candidates_lengths[i_candidate] > longest_candidate)
			longest_candidate = candidates_lengths[i_candidate];
		for (i_char = 0; i_char < candidates_lengths[i_candidate]; i_char++)
		{
			if (char_width == 4)
				key = (uint32_t)(((uint32_t**)candidates)[i_candidate][i_char]);
			else if (char_width == 2)
				key = (uint32_t)(((uint16_t**)candidates)[i_candidate][i_char]);
			else
				key = (uint32_t)(((uint8_t**)candidates)[i_candidate][i_char]);
			chr = NULL;
			HASH_FIND(hh, chars, &key, sizeof(uint32_t), chr);
			if (!chr) {
				nb_chars++;
				if (!(chr = malloc(sizeof(t_char))))
				{
					free_chars(chars);
					return (NULL);
				}
				chr->id = key;
				chr->new_representation = nb_chars;
				HASH_ADD(hh, chars, id, sizeof(uint32_t), chr);
			}
		}
	}

	compressed_char_width = char_width;
	// We keep one available char (0) to represent an unknown character in the input at runtime.
	if (nb_chars < 256 - 1)
		compressed_char_width = 1;
	else if (nb_chars < 256 * 256 - 1)
		compressed_char_width = 2;
	char_access_width = 4;
	// We can't go up to 256, since we need to be able to send inputs of arbitrary lengths at runtime
	// and characters up to longest_candidate * 2 can be considered for the score.
	if (longest_candidate < 128)
		char_access_width = 1;
	else if (longest_candidate < 256 * 128)
		char_access_width = 2;

	compressed_candidates = candidates;

	// Rewrite candidates with smallest possible char_width
	if (compressed_char_width < char_width)
	{
		if (!(compressed_candidates = malloc(sizeof(void*) * nb_candidates)))
		{
			free_chars(chars);
			return (NULL);
		}
		for (i_candidate = 0; i_candidate < nb_candidates; i_candidate++)
		{
			if (!(compressed_candidates[i_candidate] = malloc(compressed_char_width * candidates_lengths[i_candidate])))
			{
				free_chars(chars);
				for (i = 0; i < i_candidate; i++)
					free(compressed_candidates[i]);
				free(compressed_candidates);
				return (NULL);
			}
			for (i_char = 0; i_char < candidates_lengths[i_candidate]; i_char++)
			{
				if (char_width == 4)
					key = (uint32_t)(((uint32_t**)candidates)[i_candidate][i_char]);
				else if (char_width == 2)
					key = (uint32_t)(((uint16_t**)candidates)[i_candidate][i_char]);
				else
					key = (uint32_t)(((uint8_t**)candidates)[i_candidate][i_char]);
				HASH_FIND(hh, chars, &key, sizeof(uint32_t), chr);
				if (compressed_char_width == 4)
					((uint32_t**)compressed_candidates)[i_candidate][i_char] = chr->new_representation;
				else if (compressed_char_width == 2)
					((uint16_t**)compressed_candidates)[i_candidate][i_char] = chr->new_representation;
				else
					((uint8_t**)compressed_candidates)[i_candidate][i_char] = chr->new_representation;
			}
		}
	}

	free_chars(chars);

	exportable_model = build_exportable_model(
		candidates, char_width, compressed_candidates, compressed_char_width, char_access_width,
		candidates_lengths, nb_candidates, min_scores, nb_runtime_threads, res_model_size
	);

	if (compressed_candidates != candidates)
	{
		for (i_candidate = 0; i_candidate < nb_candidates; i_candidate++)
			free(compressed_candidates[i_candidate]);
		free(compressed_candidates);
	}

	return (exportable_model);
}

void	bjw_free_runtime_model(void *runtime_model)
{
	uint32_t	char_width;
	uint32_t	char_access_width;

	char_width = *((uint32_t*)(runtime_model + sizeof(uint32_t) * 2));
	char_access_width = *((uint32_t*)(runtime_model + sizeof(uint32_t) * 3));
	
	void (*free_function)(void*) = NULL;
	if (char_width == 4 && char_access_width == 4)
		free_function = free_runtime_model_uint32_t_uint32_t;
	else if (char_width == 4 && char_access_width == 2)
		free_function = free_runtime_model_uint32_t_uint16_t;
	else if (char_width == 4 && char_access_width == 1)
		free_function = free_runtime_model_uint32_t_uint8_t;
	else if (char_width == 2 && char_access_width == 4)
		free_function = free_runtime_model_uint16_t_uint32_t;
	else if (char_width == 2 && char_access_width == 2)
		free_function = free_runtime_model_uint16_t_uint16_t;
	else if (char_width == 2 && char_access_width == 1)
		free_function = free_runtime_model_uint16_t_uint8_t;
	else if (char_width == 1 && char_access_width == 4)
		free_function = free_runtime_model_uint8_t_uint32_t;
	else if (char_width == 1 && char_access_width == 2)
		free_function = free_runtime_model_uint8_t_uint16_t;
	else if (char_width == 1 && char_access_width == 1)
		free_function = free_runtime_model_uint8_t_uint8_t;

	free_function(runtime_model);
}

void	*bjw_build_runtime_model(void *exportable_model)
{
	uint32_t	nb_runtime_threads;
	uint32_t	nb_candidates;
	uint32_t	*model_size_per_thread;
	uint32_t	char_width;
	uint32_t	char_access_width;
	uint32_t	original_char_width;

	uint8_t *exportable_model_head = (uint8_t*)exportable_model;
	nb_runtime_threads = *((uint32_t*)exportable_model_head);
	exportable_model_head += sizeof(uint32_t);
	nb_candidates = *((uint32_t*)exportable_model_head);
	exportable_model_head += sizeof(uint32_t);
	char_width = *((uint32_t*)exportable_model_head);
	exportable_model_head += sizeof(uint32_t);
	char_access_width = *((uint32_t*)exportable_model_head);
	exportable_model_head += sizeof(uint32_t);
	original_char_width = *((uint32_t*)exportable_model_head);
	exportable_model_head += sizeof(uint32_t);
	model_size_per_thread = (uint32_t*)exportable_model_head;
	exportable_model_head += sizeof(uint32_t) * nb_runtime_threads;

	void *(*build_function)(uint8_t*, uint32_t, uint32_t, uint32_t, uint32_t*) = NULL;
	if (char_width == 4 && char_access_width == 4)
		build_function = build_runtime_model_uint32_t_uint32_t;
	else if (char_width == 4 && char_access_width == 2)
		build_function = build_runtime_model_uint32_t_uint16_t;
	else if (char_width == 4 && char_access_width == 1)
		build_function = build_runtime_model_uint32_t_uint8_t;
	else if (char_width == 2 && char_access_width == 4)
		build_function = build_runtime_model_uint16_t_uint32_t;
	else if (char_width == 2 && char_access_width == 2)
		build_function = build_runtime_model_uint16_t_uint16_t;
	else if (char_width == 2 && char_access_width == 1)
		build_function = build_runtime_model_uint16_t_uint8_t;
	else if (char_width == 1 && char_access_width == 4)
		build_function = build_runtime_model_uint8_t_uint32_t;
	else if (char_width == 1 && char_access_width == 2)
		build_function = build_runtime_model_uint8_t_uint16_t;
	else if (char_width == 1 && char_access_width == 1)
		build_function = build_runtime_model_uint8_t_uint8_t;

	return (build_function(exportable_model_head, nb_runtime_threads, nb_candidates, original_char_width, model_size_per_thread));
}

static int		sort_results_by_score(const void *void_res1, const void *void_res2)
{
	bjw_result	*res1;
	bjw_result	*res2;

	res1 = (bjw_result*)void_res1;
	res2 = (bjw_result*)void_res2;
	return (res1->score < res2->score ? 1 : res1->score == res2->score ? 0 : -1);
}

bjw_result	*bjw_jaro_winkler_distance(void *runtime_model, void *input, uint32_t input_length, float min_score, float weight, float threshold, uint32_t n_best_results, uint32_t *nb_results)
{
	uint32_t		i_thread;
	uint32_t		nb_runtime_threads;
	uint32_t		nb_candidates;
	uint32_t		char_width;
	uint32_t		char_access_width;
	uint32_t		original_char_width;
	char			both_min_score_and_min_scores;
	uint32_t		n_best_i_try;
	uint32_t		n_best_nb_tries;
	float			n_best_tries[3];
	bjw_result		*results;
	bjw_result		*tmp_results;
	uint32_t		results_decal;
	t_thread_data	*threads_data;
#if BJW_USE_THREADS
# ifdef _WIN32
	HANDLE			*threads;
# else
	pthread_t		*threads;
# endif
#endif

	nb_runtime_threads = *((uint32_t*)(runtime_model + sizeof(uint32_t) * 0));
	nb_candidates = *((uint32_t*)(runtime_model + sizeof(uint32_t) * 1));
	char_width = *((uint32_t*)(runtime_model + sizeof(uint32_t) * 2));
	char_access_width = *((uint32_t*)(runtime_model + sizeof(uint32_t) * 3));
	original_char_width = *((uint32_t*)(runtime_model + sizeof(uint32_t) * 4));

	// Characters after 256 won't be taken into consideration for score calculation anyway, and uint8_t won't be able to represent the indices.
	if (char_access_width == 1 && input_length >= 256)
		input_length = 256 - 1;
	else if (char_access_width == 2 && input_length >= 256 * 256)
		input_length = (256 * 256) - 1;
	both_min_score_and_min_scores = min_score < 0.0f && n_best_results != 0;
	if (n_best_results > nb_candidates)
		n_best_results = nb_candidates;

#if BJW_USE_THREADS
# ifdef _WIN32
	if (!(threads = malloc(sizeof(HANDLE) * nb_runtime_threads)))
		return (NULL);
# else
	if (!(threads = malloc(sizeof(pthread_t) * nb_runtime_threads)))
		return (NULL);
# endif
#endif

	if (!(threads_data = malloc(sizeof(t_thread_data) * nb_runtime_threads)))
		return (NULL);

	void* (*runtime_function)(void*) = NULL;
	if (char_width == 4 && char_access_width == 4)
		runtime_function = jaro_winkler_distance_for_thread_uint32_t_uint32_t;
	else if (char_width == 4 && char_access_width == 2)
		runtime_function = jaro_winkler_distance_for_thread_uint32_t_uint16_t;
	else if (char_width == 4 && char_access_width == 1)
		runtime_function = jaro_winkler_distance_for_thread_uint32_t_uint8_t;
	else if (char_width == 2 && char_access_width == 4)
		runtime_function = jaro_winkler_distance_for_thread_uint16_t_uint32_t;
	else if (char_width == 2 && char_access_width == 2)
		runtime_function = jaro_winkler_distance_for_thread_uint16_t_uint16_t;
	else if (char_width == 2 && char_access_width == 1)
		runtime_function = jaro_winkler_distance_for_thread_uint16_t_uint8_t;
	else if (char_width == 1 && char_access_width == 4)
		runtime_function = jaro_winkler_distance_for_thread_uint8_t_uint32_t;
	else if (char_width == 1 && char_access_width == 2)
		runtime_function = jaro_winkler_distance_for_thread_uint8_t_uint16_t;
	else if (char_width == 1 && char_access_width == 1)
		runtime_function = jaro_winkler_distance_for_thread_uint8_t_uint8_t;

	if (n_best_results != 0)
	{
		if (nb_candidates > 0)
			n_best_tries[0] = 1.0f - (((float)n_best_results) / nb_candidates);
		else
			n_best_tries[0] = -1.0f;
		if (n_best_tries[0] > 0.8f)
			n_best_tries[0] = 0.8f;
		n_best_tries[1] = n_best_tries[0] - 0.2f;
		n_best_tries[1] = n_best_tries[1] < 0.0f ? -1.0f : n_best_tries[1];
		n_best_tries[2] = min_score;
		n_best_nb_tries = 3;

		if (n_best_tries[1] <= min_score)
		{
			n_best_nb_tries--;
			n_best_tries[1] = min_score;
		}
		if (n_best_tries[0] <= min_score)
		{
			n_best_nb_tries--;
			n_best_tries[0] = min_score;
		}
	}
	else
	{
		n_best_tries[0] = min_score;
		n_best_nb_tries = 1;
	}

	for (n_best_i_try = 0; n_best_i_try < n_best_nb_tries; n_best_i_try++)
	{
		for (i_thread = 0; i_thread < nb_runtime_threads; i_thread++)
		{
			threads_data[i_thread] = (t_thread_data){
				.runtime_models = runtime_model + sizeof(uint32_t) * 5,
				.i_thread = i_thread,
				.original_char_width = original_char_width,
				.input = input,
				.input_length = input_length,
				.min_score = n_best_tries[n_best_i_try],
				.weight = weight,
				.threshold = threshold,
				.both_min_score_and_min_scores = both_min_score_and_min_scores,
				.results = NULL,
				.nb_results = 0
			};

#if BJW_USE_THREADS
# ifdef _WIN32
			threads[i_thread] = CreateThread(NULL, 0, runtime_function, &(threads_data[i_thread]), 0, NULL);
# else
			pthread_create(&(threads[i_thread]), NULL, runtime_function, &(threads_data[i_thread]));
# endif
#else
			runtime_function(&(threads_data[i_thread]));
#endif
		}

		*nb_results = 0;
		for (i_thread = 0; i_thread < nb_runtime_threads; i_thread++)
		{
#if BJW_USE_THREADS
# ifdef _WIN32
			WaitForSingleObject(threads[i_thread], INFINITE);
			CloseHandle(threads[i_thread]);
# else
			pthread_join(threads[i_thread], NULL);
# endif
#endif
			*nb_results += threads_data[i_thread].nb_results;
		}

		if (n_best_results == 0 || *nb_results >= n_best_results)
			break ;
	}

	if (!(results = malloc(sizeof(bjw_result) * (*nb_results))))
		return (NULL);
	results_decal = 0;
	for (i_thread = 0; i_thread < nb_runtime_threads; i_thread++)
	{
		memcpy(&(results[results_decal]), threads_data[i_thread].results, sizeof(bjw_result) * threads_data[i_thread].nb_results);
		free(threads_data[i_thread].results);
		results_decal += threads_data[i_thread].nb_results;
	}

	if (n_best_results != 0)
	{
		qsort(results, *nb_results, sizeof(bjw_result), &sort_results_by_score);
		if (*nb_results > n_best_results)
		{
			if (!(tmp_results = malloc(sizeof(bjw_result) * n_best_results)))
				return (NULL);
			memcpy(tmp_results, results, sizeof(bjw_result) * n_best_results);
			free(results);
			results = tmp_results;
			*nb_results = n_best_results;
		}
	}

#if BJW_USE_THREADS
	free(threads);
#endif
	free(threads_data);

	return (results);
}

bjw_result	*bjw_jaro_distance(void *runtime_model, void *input, uint32_t input_length, float min_score, uint32_t n_best_results, uint32_t *nb_results)
{
	return (bjw_jaro_winkler_distance(runtime_model, input, input_length, min_score, -1.0f, -1.0f, n_best_results, nb_results));
}
