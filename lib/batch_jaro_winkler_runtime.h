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

#define BJW_SUFFIX_PASTER(name, type1, type2) name ## _ ## type1 ## _ ## type2
#define BJW_SUFFIX_HANDLER(name, type1, type2) BJW_SUFFIX_PASTER(name, type1, type2)
#define BJW_SUFFIX(name) BJW_SUFFIX_HANDLER(name, BJW_CHAR_TYPE, BJW_CHAR_ACCESS_TYPE)

// this represents the data needed for a candidate when finding matches
typedef struct
{
	BJW_CHAR_ACCESS_TYPE	candidate_length;
	BJW_CHAR_ACCESS_TYPE	nb_matches;
	BJW_CHAR_ACCESS_TYPE	required_nb_matches;
	// the search_range cannot be higher than max(runtime_input_len, max_candidate_len) / 2 - 1,
	// so 1 byte is enough
	BJW_CHAR_ACCESS_TYPE	search_range;
}							BJW_SUFFIX(t_candidate_data);

// holds match information needed to calculate the jaro distance
typedef struct	
{
	uint8_t		*input_flags;
	uint8_t		*candidates_flags;
	uint32_t	*candidates_decal;
}				BJW_SUFFIX(t_occurrences_matches);

// map from character to matching candidate occurrences of the character
typedef struct			
{
	// character represented as an uint32 for uthash
	uint32_t				id;
	// candidate character occurrences as explained above
	uint8_t					*occurrences;
	// number of occurrences to skip per candidate
	BJW_CHAR_ACCESS_TYPE	*nb_already_considered;
	uint32_t				nb_matching_candidates;
	// internal data used by uthash
	UT_hash_handle			hh;
}							BJW_SUFFIX(t_char_matches);

typedef struct			
{
	BJW_SUFFIX(t_char_matches)	*all_char_matches;
	BJW_CHAR_ACCESS_TYPE		*all_nb_already_considered;
	uint32_t					all_nb_candidate_occurrences;
}								BJW_SUFFIX(t_char_matches_data);

typedef struct				
{
	uint32_t							nb_candidates;
	uint32_t							total_candidates_lengths;
	BJW_SUFFIX(t_candidate_data)		*candidates_data;
	void								*original_candidates;
	BJW_CHAR_TYPE						*candidates;
	uint32_t							*min_scores;
	BJW_SUFFIX(t_occurrences_matches)	occurrences_matches;
	t_char								*original_chars_to_new;
	t_char								*all_original_chars_to_new;
	BJW_SUFFIX(t_char_matches)			*char_matches;
	BJW_SUFFIX(t_char_matches_data)		char_matches_data;
}										BJW_SUFFIX(t_runtime_model);

static void		BJW_SUFFIX(free_runtime_model_for_thread)
(BJW_SUFFIX(t_runtime_model) *runtime_model)
{
	HASH_CLEAR(hh, runtime_model->char_matches);
	HASH_CLEAR(hh, runtime_model->original_chars_to_new);
	free(runtime_model->candidates_data);
	runtime_model->candidates_data = NULL;
	free(runtime_model->occurrences_matches.candidates_flags);
	runtime_model->occurrences_matches.candidates_flags = NULL;
	free(runtime_model->char_matches_data.all_char_matches);
	runtime_model->char_matches_data.all_char_matches = NULL;
	free(runtime_model->char_matches_data.all_nb_already_considered);
	runtime_model->char_matches_data.all_nb_already_considered = NULL;
	free(runtime_model->all_original_chars_to_new);
	runtime_model->all_original_chars_to_new = NULL;
}

static char		BJW_SUFFIX(build_runtime_model_for_thread)
(uint8_t *exportable_model_head, uint32_t original_char_width, BJW_SUFFIX(t_runtime_model) *runtime_model)
{
	uint32_t					nb_candidates;
	uint32_t					total_candidates_lengths;
	char						min_scores_present;
	uint32_t					nb_char_matches;
	uint32_t					nb_candidate_occurrences;
	uint32_t					store_original_candidates;
	void						*original_candidates;
	BJW_CHAR_TYPE				*candidates;
	uint32_t					*min_scores;
	void						*original_chars;
	t_char						*original_char_match;
	BJW_CHAR_TYPE				*chars;
	uint32_t					*chars_occurrences_decals;
	uint32_t					*nb_candidates_per_char_match;
	uint32_t					*candidates_decal;
	uint8_t						*occurrences;
	uint32_t					i_char;
	uint32_t					nb_already_considered_decal;
	BJW_SUFFIX(t_char_matches)	*match;

	nb_candidates = *((uint32_t*)exportable_model_head);
	exportable_model_head += sizeof(uint32_t);
	total_candidates_lengths = *((uint32_t*)exportable_model_head);
	exportable_model_head += sizeof(uint32_t);
	min_scores_present = *((uint32_t*)exportable_model_head) ? 1 : 0;
	exportable_model_head += sizeof(uint32_t);
	nb_char_matches = *((uint32_t*)exportable_model_head);
	exportable_model_head += sizeof(uint32_t);
	nb_candidate_occurrences = *((uint32_t*)exportable_model_head);
	exportable_model_head += sizeof(uint32_t);
	store_original_candidates = *((uint32_t*)exportable_model_head);
	exportable_model_head += sizeof(uint32_t);
	min_scores = min_scores_present ? (uint32_t*)exportable_model_head : NULL;
	exportable_model_head += sizeof(uint32_t) * nb_candidates * min_scores_present;
	chars_occurrences_decals = (uint32_t*)exportable_model_head;
	exportable_model_head += sizeof(uint32_t) * nb_char_matches;
	nb_candidates_per_char_match = (uint32_t*)exportable_model_head;
	exportable_model_head += sizeof(uint32_t) * nb_char_matches;
	candidates_decal = (uint32_t*)exportable_model_head;
	exportable_model_head += sizeof(uint32_t) * (nb_candidates + 1);
	original_candidates = exportable_model_head;
	exportable_model_head += original_char_width * total_candidates_lengths * store_original_candidates;
	candidates = (BJW_CHAR_TYPE*)exportable_model_head;
	exportable_model_head += sizeof(BJW_CHAR_TYPE) * total_candidates_lengths;
	original_chars = exportable_model_head;
	exportable_model_head += original_char_width * nb_char_matches * store_original_candidates;
	chars = (BJW_CHAR_TYPE*)exportable_model_head;
	exportable_model_head += sizeof(BJW_CHAR_TYPE) * nb_char_matches;
	occurrences = (uint8_t*)exportable_model_head;

	runtime_model->nb_candidates = nb_candidates;
	runtime_model->total_candidates_lengths = total_candidates_lengths;
	runtime_model->candidates_data = malloc(sizeof(BJW_SUFFIX(t_candidate_data)) * nb_candidates);
	runtime_model->original_candidates = store_original_candidates ? original_candidates : candidates;
	runtime_model->candidates = candidates;
	runtime_model->min_scores = min_scores;
	runtime_model->occurrences_matches.candidates_flags = malloc(sizeof(uint8_t) * total_candidates_lengths);
	runtime_model->occurrences_matches.candidates_decal = candidates_decal;
	// important to set to NULL for uthash
	runtime_model->original_chars_to_new = NULL;
	runtime_model->all_original_chars_to_new = store_original_candidates ? malloc(sizeof(t_char) * nb_char_matches) : NULL;
	// important to set to NULL for uthash
	runtime_model->char_matches = NULL;
	runtime_model->char_matches_data.all_char_matches = malloc(sizeof(BJW_SUFFIX(t_char_matches)) * nb_char_matches);
	runtime_model->char_matches_data.all_nb_already_considered = malloc(sizeof(BJW_CHAR_ACCESS_TYPE) * nb_candidate_occurrences);
	runtime_model->char_matches_data.all_nb_candidate_occurrences = nb_candidate_occurrences;

	if (!runtime_model->candidates_data || !runtime_model->occurrences_matches.candidates_flags
		|| !runtime_model->char_matches_data.all_char_matches || !runtime_model->char_matches_data.all_nb_already_considered
		|| (store_original_candidates && !runtime_model->all_original_chars_to_new)
	)
	{
		BJW_SUFFIX(free_runtime_model_for_thread)(runtime_model);
		return (0);
	}

	nb_already_considered_decal = 0;
	for (i_char = 0; i_char < nb_char_matches; i_char++)
	{
		if (store_original_candidates)
		{
			original_char_match = &(runtime_model->all_original_chars_to_new[i_char]);
			if (original_char_width == 4)
				original_char_match->id = (uint32_t)(((uint32_t*)original_chars)[i_char]);
			else if (original_char_width == 2)
				original_char_match->id = (uint32_t)(((uint16_t*)original_chars)[i_char]);
			else
				original_char_match->id = (uint32_t)(((uint8_t*)original_chars)[i_char]);
			original_char_match->new_representation = chars[i_char];
			HASH_ADD(hh, runtime_model->original_chars_to_new, id, sizeof(uint32_t), original_char_match);
		}

		match = &(runtime_model->char_matches_data.all_char_matches[i_char]);
		match->id = chars[i_char];
		match->occurrences = occurrences + chars_occurrences_decals[i_char];
		match->nb_already_considered = runtime_model->char_matches_data.all_nb_already_considered + nb_already_considered_decal;
		match->nb_matching_candidates = nb_candidates_per_char_match[i_char];
		HASH_ADD(hh, runtime_model->char_matches, id, sizeof(uint32_t), match);
		nb_already_considered_decal += nb_candidates_per_char_match[i_char];
	}

	return (1);
}

static void		BJW_SUFFIX(free_runtime_model)
(void *runtime_model)
{
	BJW_SUFFIX(t_runtime_model)		*runtime_models;
	uint32_t						i_thread;
	uint32_t						nb_runtime_threads;

	nb_runtime_threads = *((uint32_t*)(runtime_model + sizeof(uint32_t) * 0));
	runtime_models = (BJW_SUFFIX(t_runtime_model)*)(runtime_model + sizeof(uint32_t) * 5);
	for (i_thread = 0; i_thread < nb_runtime_threads; i_thread++)
		BJW_SUFFIX(free_runtime_model_for_thread)(&(runtime_models[i_thread]));
	free(runtime_model);
}

static void		*BJW_SUFFIX(build_runtime_model)
(uint8_t *exportable_model_head, uint32_t nb_runtime_threads, uint32_t nb_candidates, uint32_t original_char_width, uint32_t *model_size_per_thread)
{
	uint32_t					i_thread;
	uint32_t					i;
	void						*res;
	BJW_SUFFIX(t_runtime_model)	*runtime_models;

	res = malloc(sizeof(BJW_SUFFIX(t_runtime_model)) * nb_runtime_threads + sizeof(uint32_t) * 5);
	if (!res)
		return (NULL);
	*((uint32_t*)(res + sizeof(uint32_t) * 0)) = nb_runtime_threads;
	*((uint32_t*)(res + sizeof(uint32_t) * 1)) = nb_candidates;
	*((uint32_t*)(res + sizeof(uint32_t) * 2)) = sizeof(BJW_CHAR_TYPE);
	*((uint32_t*)(res + sizeof(uint32_t) * 3)) = sizeof(BJW_CHAR_ACCESS_TYPE);
	*((uint32_t*)(res + sizeof(uint32_t) * 4)) = original_char_width;
	runtime_models = res + sizeof(uint32_t) * 5;

	for (i_thread = 0; i_thread < nb_runtime_threads; i_thread++)
	{
		if (!BJW_SUFFIX(build_runtime_model_for_thread)(exportable_model_head, original_char_width, &(runtime_models[i_thread])))
		{
			for (i = 0; i < i_thread; i++)
				BJW_SUFFIX(free_runtime_model_for_thread)(&(runtime_models[i]));
			free(res);
			return (NULL);
		}
		exportable_model_head += model_size_per_thread[i_thread];
	}

	return (res);
}

static void		BJW_SUFFIX(populate_candidates_data)
(BJW_SUFFIX(t_candidate_data) *candidates_data, uint32_t *candidates_decal, uint32_t nb_candidates, float min_score, uint32_t *min_scores, uint32_t input_length, float weight, char both_min_score_and_min_scores)
{
	uint32_t				i_candidate;
	float					candidate_min_score;
	BJW_CHAR_ACCESS_TYPE	candidate_length;
	BJW_CHAR_ACCESS_TYPE	required_nb_matches;
	BJW_CHAR_ACCESS_TYPE	search_range;
	float					float_required_nb_matches;
	float					bottom_part;

	for (i_candidate = 0; i_candidate < nb_candidates; i_candidate++)
	{
		candidate_min_score = min_score;
		if (min_score < 0.0f || (both_min_score_and_min_scores && ((float)min_scores[i_candidate]) / UINT32_MAX > candidate_min_score))
			candidate_min_score = ((float)min_scores[i_candidate]) / UINT32_MAX;

		candidate_length = candidates_decal[i_candidate + 1] - candidates_decal[i_candidate];
		if (candidate_length < 1)
		{
			candidates_data[i_candidate] = (BJW_SUFFIX(t_candidate_data)){
				.candidate_length = candidate_length,
				.nb_matches = 0,
				.required_nb_matches = 0,
				.search_range = 1
			};
			continue ;
		}

		// equations solved using: https://www.mathpapa.com/equation-solver
		// jaro distance
		if (weight < 0.0f)
			float_required_nb_matches = (3.0f * candidate_min_score * candidate_length * input_length - (candidate_length * input_length)) / (candidate_length + input_length);
		else
		{
			// jaro winkler distance. Assume all prefixing characters match
			bottom_part = -(4.0f * candidate_length * weight) - (4.0f * input_length * weight) + candidate_length + input_length;
			if (bottom_part == 0.0f || bottom_part == -0.0f)
				float_required_nb_matches = candidate_length > input_length ? candidate_length + 1 : input_length + 1;
			else
			{
				float_required_nb_matches = (
					(3.0f * candidate_min_score * candidate_length * input_length) -
					(8.0f * weight * candidate_length * input_length) -
					(candidate_length * input_length)
				) / bottom_part;
			}
		}
		if (float_required_nb_matches < 0.0f)
			float_required_nb_matches = 0.0f;
		required_nb_matches = (BJW_CHAR_ACCESS_TYPE)(ceil(float_required_nb_matches));

		search_range = (input_length > candidate_length ? input_length : candidate_length) / 2;
		search_range = search_range <= 1 ? 0 : search_range - 1;

		candidates_data[i_candidate] = (BJW_SUFFIX(t_candidate_data)){
			.candidate_length = candidate_length,
			.nb_matches = 0,
			.required_nb_matches = required_nb_matches,
			.search_range = search_range
		};
	}
}

static void		BJW_SUFFIX(find_occurrences_matches)
(BJW_SUFFIX(t_runtime_model) *runtime_model, BJW_SUFFIX(t_char_matches) *match, uint32_t remaining_chars, uint32_t i_char, uint32_t input_length)
{
	uint8_t							*occurrences_head;
	uint32_t						i_candidate;
	uint32_t						candidate_ind;
	BJW_CHAR_ACCESS_TYPE			nb_occurrences;
	BJW_CHAR_ACCESS_TYPE			*occurrences;
	BJW_SUFFIX(t_candidate_data)	*candidate_data;
	uint32_t						i_occurrence;
	uint32_t						low_search_range;
	uint32_t						high_search_range;
	uint32_t						candidate_decal;

	occurrences_head = match->occurrences;
	for (i_candidate = 0; i_candidate < match->nb_matching_candidates; i_candidate++)
	{
		candidate_ind = *((uint32_t*)occurrences_head);
		candidate_data = &(runtime_model->candidates_data[candidate_ind]);

		nb_occurrences = *((BJW_CHAR_ACCESS_TYPE*)(occurrences_head + sizeof(uint32_t)));
		occurrences = (BJW_CHAR_ACCESS_TYPE*)(occurrences_head + sizeof(uint32_t) + sizeof(BJW_CHAR_ACCESS_TYPE));
		occurrences_head += sizeof(uint32_t) + sizeof(BJW_CHAR_ACCESS_TYPE) + sizeof(BJW_CHAR_ACCESS_TYPE) * nb_occurrences;

		// assuming all remaining chars match, is it enough to get enough matches?
		if (candidate_data->nb_matches + remaining_chars < candidate_data->required_nb_matches)
			continue ;

		i_occurrence = match->nb_already_considered[i_candidate];
		// if we already tried all occurrences
		if (i_occurrence >= nb_occurrences)
			continue ;

		low_search_range = i_char < candidate_data->search_range ? 0 : i_char - candidate_data->search_range;
		high_search_range = i_char + candidate_data->search_range;
		while (i_occurrence < nb_occurrences && occurrences[i_occurrence] < low_search_range)
			i_occurrence++;
		// don't increment when i_occurrence >= nb_occurrences, to prevent overflow
		if (i_occurrence >= nb_occurrences || occurrences[i_occurrence] <= high_search_range)
			match->nb_already_considered[i_candidate] = i_occurrence + (i_occurrence < nb_occurrences);
		if (i_occurrence < nb_occurrences && occurrences[i_occurrence] <= high_search_range)
		{
			candidate_data->nb_matches++;
			runtime_model->occurrences_matches.input_flags[candidate_ind * input_length + i_char] = 1;
			candidate_decal = runtime_model->occurrences_matches.candidates_decal[candidate_ind];
			runtime_model->occurrences_matches.candidates_flags[candidate_decal + occurrences[i_occurrence]] = 1;
		}
	}
}

static uint32_t		BJW_SUFFIX(get_nb_transpositions)
(BJW_CHAR_TYPE *input, uint8_t *input_flags, BJW_CHAR_TYPE *candidate, uint8_t *candidate_flags, uint32_t nb_matches)
{
	uint32_t	trans_count;
	uint32_t	input_ind;
	uint32_t	candidate_ind;
	uint32_t	i_match;

	trans_count = 0;
	input_ind = 0;
	candidate_ind = 0;
	for (i_match = 0; i_match < nb_matches; i_match++)
	{
		// Go to next ok input flag
		while (!input_flags[input_ind])
			input_ind++;
		// Go to next ok candidate flag
		while (!candidate_flags[candidate_ind])
			candidate_ind++;
		if (input[input_ind] != candidate[candidate_ind])
			trans_count++;
		input_ind++;
		candidate_ind++;
	}
	return (trans_count);
}

static uint32_t		BJW_SUFFIX(jaro_winkler_distance_from_flags)
(BJW_SUFFIX(t_runtime_model) *runtime_model, uint32_t original_char_width, BJW_CHAR_TYPE *input, uint32_t input_length, float min_score, float weight, float threshold, char both_min_score_and_min_scores, bjw_result *results)
{
	uint32_t						i_candidate;
	BJW_CHAR_TYPE					*candidate;
	BJW_SUFFIX(t_candidate_data)	*candidate_data;
	uint32_t						candidate_decal;
	uint32_t						nb_transpositions;
	float							score;
	float							candidate_min_score;
	uint32_t						nb_results;
	uint32_t						i_char;
	uint32_t						prefix_length;

	nb_results = 0;
	for (i_candidate = 0; i_candidate < runtime_model->nb_candidates; i_candidate++)
	{
		candidate_data = &(runtime_model->candidates_data[i_candidate]);

		if (candidate_data->nb_matches < candidate_data->required_nb_matches)
			continue ;

		candidate_min_score = min_score;
		if (min_score < 0.0f || (both_min_score_and_min_scores && ((float)runtime_model->min_scores[i_candidate]) / UINT32_MAX > candidate_min_score))
			candidate_min_score = ((float)runtime_model->min_scores[i_candidate]) / UINT32_MAX;

		candidate_decal = runtime_model->occurrences_matches.candidates_decal[i_candidate];
		candidate = &(runtime_model->candidates[candidate_decal]);

		if (candidate_data->nb_matches == 0)
		{
			if (candidate_min_score <= 0.0f)
			{
				results[nb_results].candidate = runtime_model->original_candidates + original_char_width * candidate_decal;
				results[nb_results].score = 0.0f;
				results[nb_results].candidate_length = candidate_data->candidate_length;
				nb_results++;
			}
			continue ;
		}

		nb_transpositions = BJW_SUFFIX(get_nb_transpositions)(
			input, &(runtime_model->occurrences_matches.input_flags[i_candidate * input_length]),
			candidate, &(runtime_model->occurrences_matches.candidates_flags[candidate_decal]),
			candidate_data->nb_matches
		);
		nb_transpositions /= 2;

		score =
			candidate_data->nb_matches / ((float)input_length) +
			candidate_data->nb_matches / ((float)candidate_data->candidate_length) +
			((float)(candidate_data->nb_matches - nb_transpositions)) / ((float)candidate_data->nb_matches);
		score /= 3.0f;

		if (weight >= 0.0f && score >= threshold)
		{
			prefix_length = candidate_data->candidate_length < input_length ? candidate_data->candidate_length : input_length;
			prefix_length = prefix_length > 4 ? 4 : prefix_length;
			for (i_char = 0; i_char < prefix_length && input[i_char] == candidate[i_char]; i_char++){}
			score = score + (i_char * weight * (1.0f - score));
		}

		if (score < candidate_min_score)
			continue ;

		results[nb_results].candidate = runtime_model->original_candidates + original_char_width * candidate_decal;
		results[nb_results].score = score;
		results[nb_results].candidate_length = candidate_data->candidate_length;
		nb_results++;
	}
	return (nb_results);
}

static void		*BJW_SUFFIX(build_compressed_input)
(BJW_SUFFIX(t_runtime_model) *runtime_model, void *input, uint32_t input_length, uint32_t original_char_width)
{
	BJW_CHAR_TYPE	*compressed_input;
	uint32_t		i_char;
	t_char			*original_char_match;
	uint32_t		key;

	if (sizeof(BJW_CHAR_TYPE) == original_char_width)
		return (input);
	if (!(compressed_input = malloc(sizeof(BJW_CHAR_TYPE) * input_length)))
		return (NULL);
	for (i_char = 0; i_char < input_length; i_char++)
	{
		if (original_char_width == 4)
			key = (uint32_t)(((uint32_t*)input)[i_char]);
		else if (original_char_width == 2)
			key = (uint32_t)(((uint16_t*)input)[i_char]);
		else
			key = (uint32_t)(((uint8_t*)input)[i_char]);
		HASH_FIND(hh, runtime_model->original_chars_to_new, &key, sizeof(uint32_t), original_char_match);
		if (!original_char_match)
			compressed_input[i_char] = 0;
		else
			compressed_input[i_char] = original_char_match->new_representation;
	}
	return (compressed_input);
}

static void		*BJW_SUFFIX(jaro_winkler_distance_for_thread)
(void *thread_data_raw)
{
	t_thread_data				*thread_data;
	BJW_SUFFIX(t_runtime_model)	*runtime_model;
	void						*input;
	uint32_t					input_length;
	float						min_score;
	float						weight;
	float						threshold;
	char						both_min_score_and_min_scores;
	uint32_t					i_char;
	uint32_t					key;
	BJW_SUFFIX(t_char_matches)	*match;
	BJW_CHAR_TYPE				*compressed_input;

	thread_data = (t_thread_data*)thread_data_raw;

	runtime_model = &(((BJW_SUFFIX(t_runtime_model)*)thread_data->runtime_models)[thread_data->i_thread]);
	input = thread_data->input;
	input_length = thread_data->input_length;
	min_score = thread_data->min_score;
	weight = thread_data->weight;
	threshold = thread_data->threshold;
	both_min_score_and_min_scores = thread_data->both_min_score_and_min_scores;

	compressed_input = BJW_SUFFIX(build_compressed_input)(runtime_model, input, input_length, thread_data->original_char_width);
	if (!compressed_input)
		return (NULL);

	if (!runtime_model->min_scores && min_score < 0.0f)
		min_score = 0.0f;
	both_min_score_and_min_scores = both_min_score_and_min_scores && runtime_model->min_scores;
	if (!(thread_data->results = malloc(sizeof(bjw_result) * runtime_model->nb_candidates)))
	{
		if (compressed_input != input)
			free(compressed_input);
		return (NULL);
	}
	runtime_model->occurrences_matches.input_flags = malloc(sizeof(uint8_t) * input_length * runtime_model->nb_candidates);
	if (!runtime_model->occurrences_matches.input_flags)
	{
		free(thread_data->results);
		thread_data->results = NULL;
		if (compressed_input != input)
			free(compressed_input);
		return (NULL);
	}
	bzero(runtime_model->occurrences_matches.input_flags, sizeof(uint8_t) * input_length * runtime_model->nb_candidates);
	bzero(runtime_model->occurrences_matches.candidates_flags, sizeof(uint8_t) * runtime_model->total_candidates_lengths);
	bzero(runtime_model->char_matches_data.all_nb_already_considered, sizeof(BJW_CHAR_ACCESS_TYPE) * runtime_model->char_matches_data.all_nb_candidate_occurrences);

	BJW_SUFFIX(populate_candidates_data)(
		runtime_model->candidates_data, runtime_model->occurrences_matches.candidates_decal, runtime_model->nb_candidates,
		min_score, runtime_model->min_scores, input_length, weight, both_min_score_and_min_scores
	);

	// we populate the flags
	for (i_char = 0; i_char < input_length; i_char++)
	{
		key = compressed_input[i_char];
		HASH_FIND(hh, runtime_model->char_matches, &key, sizeof(uint32_t), match);
		// no occurrences for this character
		if (!match)
			continue ;
		BJW_SUFFIX(find_occurrences_matches)(runtime_model, match, input_length - i_char, i_char, input_length);
	}

	// we use the flags to calculate the rest of the jaro winkler distance
	thread_data->nb_results = BJW_SUFFIX(jaro_winkler_distance_from_flags)(
		runtime_model, thread_data->original_char_width, compressed_input, input_length, min_score, weight, threshold, both_min_score_and_min_scores, thread_data->results
	);

	free(runtime_model->occurrences_matches.input_flags);
	runtime_model->occurrences_matches.input_flags = NULL;

	if (compressed_input != input)
		free(compressed_input);

	return (NULL);
}
