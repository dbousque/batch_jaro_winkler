

#include <stdint.h>
#include <string.h>
#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/fail.h>
#include <caml/alloc.h>
#include <caml/threads.h>

#include "ext/batch_jaro_winkler.h"

uint32_t	*encode_on_4_bytes(uint8_t *candidate_string, uint32_t candidate_length, char *encoding, uint32_t *res_candidate_length)
{
	uint32_t	i_char;
	uint32_t	i_byte;
	uint32_t	*res_candidate;

	i_char = 0;
	i_byte = 0;
	while (i_byte < candidate_length)
	{
		if (strcmp(encoding, "utf8") == 0)
		{
			if ((candidate_string[i_byte] & 0xf8) == 0xf0)
				i_byte += 4;
			else if ((candidate_string[i_byte] & 0xf0) == 0xe0)
				i_byte += 3;
			else if ((candidate_string[i_byte] & 0xe0) == 0xc0)
				i_byte += 2;
			else
				i_byte++;
		}
		else if (strcmp(encoding, "utf16") == 0)
		{
			if (*((uint16_t*)(candidate_string + i_byte)) >= 0xD800 && *((uint16_t*)(candidate_string + i_byte)) <= 0xDBFF)
				i_byte += 4;
			else
				i_byte += 2;
		}
		i_char++;
	}
	res_candidate = malloc(sizeof(uint32_t) * i_char);
	if (!res_candidate)
		return (NULL);
	*res_candidate_length = i_char;
	i_char = 0;
	i_byte = 0;
	while (i_byte < candidate_length)
	{
		if (strcmp(encoding, "utf8") == 0)
		{
			if ((candidate_string[i_byte] & 0xf8) == 0xf0)
			{
				res_candidate[i_char] = *((uint32_t*)(candidate_string + i_byte));
				i_byte += 4;
			}
			else if ((candidate_string[i_byte] & 0xf0) == 0xe0)
			{
				res_candidate[i_char] = *((uint16_t*)(candidate_string + i_byte));
				((uint8_t*)(res_candidate + i_char))[2] = *((uint8_t*)(candidate_string + i_byte + 2));
				i_byte += 3;
			}
			else if ((candidate_string[i_byte] & 0xe0) == 0xc0)
			{
				res_candidate[i_char] = *((uint16_t*)(candidate_string + i_byte));
				i_byte += 2;
			}
			else
			{
				res_candidate[i_char] = *((uint8_t*)(candidate_string + i_byte));
				i_byte++;
			}
		}
		else if (strcmp(encoding, "utf16") == 0)
		{
			if (*((uint16_t*)(candidate_string + i_byte)) >= 0xD800 && *((uint16_t*)(candidate_string + i_byte)) <= 0xDBFF)
			{
				res_candidate[i_char] = *((uint32_t*)(candidate_string + i_byte));
				i_byte += 4;
			}
			else
			{
				res_candidate[i_char] = *((uint16_t*)(candidate_string + i_byte));
				i_byte += 2;
			}
		}
		i_char++;
	}
	return (res_candidate);
}

uint8_t		*decode_4_bytes(uint32_t *candidate, uint32_t candidate_length, char *encoding, uint8_t *res_candidate, uint32_t *res_candidate_length_bytes)
{
	uint32_t	i_char;
	uint32_t	res_length_bytes;

	res_length_bytes = 0;
	*res_candidate_length_bytes = 0;
	for (i_char = 0; i_char < candidate_length; i_char++)
	{
		if (strcmp(encoding, "utf8") == 0)
		{
			if (*(((uint8_t*)(candidate + i_char)) + 3) == 0)
			{
				if (*(((uint8_t*)(candidate + i_char)) + 2) == 0)
				{
					if (*(((uint8_t*)(candidate + i_char)) + 1) == 0)
					{
						*((uint8_t*)(res_candidate + res_length_bytes)) = ((uint8_t*)(candidate + i_char))[0];
						res_length_bytes += 1;
					}
					else
					{
						*((uint16_t*)(res_candidate + res_length_bytes)) = ((uint16_t*)(candidate + i_char))[0];
						res_length_bytes += 2;
					}
				}
				else
				{
					*((uint16_t*)(res_candidate + res_length_bytes)) = ((uint16_t*)(candidate + i_char))[0];
					*((uint8_t*)(res_candidate + res_length_bytes + 2)) = *((uint8_t*)(((uint8_t*)&(candidate[i_char])) + 2));
					res_length_bytes += 3;
				}
			}
			else
			{
				*((uint32_t*)(res_candidate + res_length_bytes)) = candidate[i_char];
				res_length_bytes += 4;
			}
		}
		else if (strcmp(encoding, "utf16") == 0)
		{
			if (*(((uint8_t*)(candidate + i_char)) + 2) == 0 && *(((uint8_t*)(candidate + i_char)) + 3) == 0)
			{
				*((uint16_t*)(res_candidate + res_length_bytes)) = ((uint16_t*)(candidate + i_char))[0];
				res_length_bytes += 2;
			}
			else
			{
				*((uint32_t*)(res_candidate + res_length_bytes)) = candidate[i_char];
				res_length_bytes += 4;
			}
		}
	}
	*res_candidate_length_bytes = res_length_bytes;
	return (res_candidate);
}

CAMLprim value	caml_c_build_exportable_model(value ml_encoding, value ml_nb_runtime_threads, value ml_candidates)
{
	CAMLparam3(ml_candidates, ml_encoding, ml_nb_runtime_threads);
	CAMLlocal5(ml_candidates_head, ml_candidate, ml_candidate_string, ml_min_score, ml_exportable_model);
	char		with_min_scores;
	uint32_t	nb_candidates;
	uint32_t	nb_runtime_threads;
	void		**candidates;
	uint32_t	*candidates_lengths;
	float		*min_scores;
	uint32_t	char_width;
	uint32_t	i_candidate;
	uint32_t	candidate_length;
	uint8_t		*candidate_string;
	char		encoding[13];
	void		*exportable_model;
	uint32_t	res_model_size;

	// We copy because we'll use the value after the GIL has been release and reacquired,
	// so we must not be reading previous OCaml values by then.
	memcpy(encoding, String_val(ml_encoding), caml_string_length(ml_encoding) + 1);
	char_width = 1;
	if (strcmp(encoding, "char_width_2") == 0)
		char_width = 2;
	if (strcmp(encoding, "char_width_4") == 0 || strcmp(encoding, "utf8") == 0 || strcmp(encoding, "utf16") == 0 || strcmp(encoding, "utf32") == 0)
		char_width = 4;
	nb_runtime_threads = Int_val(ml_nb_runtime_threads);

	nb_candidates = 0;
	ml_candidates_head = ml_candidates;
	with_min_scores = 0;
	while (ml_candidates_head != Val_emptylist)
	{
		ml_candidate = Field(ml_candidates_head, 0);
		ml_candidate_string = Field(ml_candidate, 0);
		ml_min_score = Field(ml_candidate, 1);
		if (ml_min_score != Val_int(0))
			with_min_scores = 1;
		ml_candidates_head = Field(ml_candidates_head, 1);
		nb_candidates++;
	}

	if (!(candidates = malloc(sizeof(void*) * nb_candidates)))
	{
		caml_failwith("Allocation error");
		CAMLreturn(Val_int(0));
	}
	if (!(candidates_lengths = malloc(sizeof(uint32_t) * nb_candidates)))
	{
		caml_failwith("Allocation error");
		CAMLreturn(Val_int(0));
	}
	min_scores = NULL;
	if (with_min_scores && !(min_scores = malloc(sizeof(float) * nb_candidates)))
	{
		caml_failwith("Allocation error");
		CAMLreturn(Val_int(0));
	}

	i_candidate = 0;
	ml_candidates_head = ml_candidates;
	while (ml_candidates_head != Val_emptylist)
	{
		ml_candidate = Field(ml_candidates_head, 0);
		ml_candidate_string = Field(ml_candidate, 0);
		candidate_string = (uint8_t*)String_val(ml_candidate_string);
		ml_min_score = Field(ml_candidate, 1);
		if (with_min_scores)
			min_scores[i_candidate] = ml_min_score == Val_int(0) ? 0.0f : Double_val(Field(ml_min_score, 0));

		candidate_length = caml_string_length(ml_candidate_string);
		if (char_width == 1)
		{
			candidates[i_candidate] = candidate_string;
			candidates_lengths[i_candidate] = candidate_length;
		}
		else if (char_width == 2)
		{
			candidates[i_candidate] = candidate_string;
			candidates_lengths[i_candidate] = candidate_length / sizeof(uint16_t);
		}
		else if (strcmp(encoding, "utf32") == 0 || strcmp(encoding, "char_width_4") == 0)
		{
			candidates[i_candidate] = candidate_string;
			candidates_lengths[i_candidate] = candidate_length / sizeof(uint32_t);
		}
		else
		{
			candidates[i_candidate] = encode_on_4_bytes(candidate_string, candidate_length, encoding, &(candidates_lengths[i_candidate]));
			if (!candidates[i_candidate])
			{
				caml_failwith("Allocation error");
				CAMLreturn(Val_int(0));
			}
		}
		ml_candidates_head = Field(ml_candidates_head, 1);
		i_candidate++;
	}

	// We copied the candidates into C data, so we can release the GIL
	if (strcmp(encoding, "utf8") == 0 || strcmp(encoding, "utf16") == 0)
		caml_release_runtime_system();
	exportable_model = bjw_build_exportable_model(candidates, char_width, candidates_lengths, nb_candidates, min_scores, nb_runtime_threads, &res_model_size);
	if (strcmp(encoding, "utf8") == 0 || strcmp(encoding, "utf16") == 0)
		caml_acquire_runtime_system();

	if (!exportable_model)
	{
		caml_failwith("Allocation error");
		CAMLreturn(Val_int(0));
	}
	if (strcmp(encoding, "utf8") == 0 || strcmp(encoding, "utf16") == 0)
	{
		for (i_candidate = 0; i_candidate < nb_candidates; i_candidate++)
			free(candidates[i_candidate]);
	}
	if (with_min_scores)
		free(min_scores);
	free(candidates);
	free(candidates_lengths);

	ml_exportable_model = caml_alloc_initialized_string(res_model_size, exportable_model);
	free(exportable_model);
	CAMLreturn(ml_exportable_model);
}

CAMLprim value	caml_c_build_runtime_model(value ml_exportable_model)
{
	CAMLparam1(ml_exportable_model);
	void		*exportable_model;
	void		*runtime_model;
	uint32_t	runtime_model_length;

	exportable_model = String_val(ml_exportable_model);
	runtime_model = bjw_build_runtime_model(exportable_model);
	if (!runtime_model)
	{
		caml_failwith("Allocation error");
		CAMLreturn(Val_int(0));
	}
	CAMLreturn((value)runtime_model);
}

CAMLprim value	caml_c_free_runtime_model(value ml_runtime_model_ptr)
{
	CAMLparam1(ml_runtime_model_ptr);
	void	*runtime_model;

	runtime_model = (void*)ml_runtime_model_ptr;
	bjw_free_runtime_model(runtime_model);
	CAMLreturn(Val_unit);
}

// We pass the entire runtime model with the exportable model to make sure it is not garbage collected
CAMLprim value	caml_c_jaro_winkler_distance(value ml_encoding, value ml_min_score, value ml_weight_threshold_n_best_results, value ml_runtime_model, value ml_input)
{
	CAMLparam5(ml_encoding, ml_min_score, ml_weight_threshold_n_best_results, ml_runtime_model, ml_input);
	CAMLlocal4(ml_results, ml_result, ml_result_tuple, ml_candidate_string);
	char		encoding[13];
	uint32_t	char_width;
	float		min_score;
	float		weight;
	float		threshold;
	uint32_t	n_best_results;
	void		*runtime_model;
	void		*input;
	uint32_t	input_length;
	uint32_t	nb_results;
	bjw_result	*c_results;
	uint32_t	i_result;
	uint8_t		*candidate;
	uint32_t	candidate_length_bytes;
	uint32_t	longest_result;

	memcpy(encoding, String_val(ml_encoding), caml_string_length(ml_encoding) + 1);
	char_width = 1;
	if (strcmp(encoding, "char_width_2") == 0)
		char_width = 2;
	if (strcmp(encoding, "char_width_4") == 0 || strcmp(encoding, "utf8") == 0 || strcmp(encoding, "utf16") == 0 || strcmp(encoding, "utf32") == 0)
		char_width = 4;
	// We copy because we'll release the GIL
	if (strcmp(encoding, "utf8") != 0 && strcmp(encoding, "utf16") != 0)
	{
		input = malloc(caml_string_length(ml_input));
		if (!input)
		{
			caml_failwith("Allocation error");
			CAMLreturn(Val_int(0));
		}
		memcpy(input, String_val(ml_input), caml_string_length(ml_input));
		input_length = caml_string_length(ml_input) / char_width;
	}
	else
	{
		input = encode_on_4_bytes((uint8_t*)String_val(ml_input), caml_string_length(ml_input), encoding, &input_length);
		if (!input)
		{
			caml_failwith("Allocation error");
			CAMLreturn(Val_int(0));
		}
	}
	min_score = Double_val(ml_min_score);
	weight = Field(ml_weight_threshold_n_best_results, 0) == Val_int(0) ? -1.0 : Double_val(Field(Field(ml_weight_threshold_n_best_results, 0), 0));
	threshold = Field(ml_weight_threshold_n_best_results, 1) == Val_int(0) ? -1.0 : Double_val(Field(Field(ml_weight_threshold_n_best_results, 1), 0));
	n_best_results = Field(ml_weight_threshold_n_best_results, 2) == Val_int(0) ? 0 : Int_val(Field(Field(ml_weight_threshold_n_best_results, 2), 0));
	runtime_model = (void*)Field(ml_runtime_model, 1);

	caml_release_runtime_system();
	c_results = bjw_jaro_winkler_distance(runtime_model, input, input_length, min_score, weight, threshold, n_best_results, &nb_results);
	free(input);
	caml_acquire_runtime_system();
	if (!c_results)
	{
		caml_failwith("Allocation error");
		CAMLreturn(Val_int(0));
	}

	if (strcmp(encoding, "utf8") == 0 || strcmp(encoding, "utf16") == 0)
	{
		longest_result = 1;
		for (i_result = 0; i_result < nb_results; i_result++)
			longest_result = c_results[i_result].candidate_length > longest_result ? c_results[i_result].candidate_length : longest_result;
		candidate = malloc(longest_result * char_width);
		bzero(candidate, longest_result * char_width);
	}

	ml_results = Val_emptylist;
	for (i_result = 0; i_result < nb_results; i_result++)
	{
		if (strcmp(encoding, "utf8") == 0 || strcmp(encoding, "utf16") == 0)
		{
			decode_4_bytes(c_results[i_result].candidate, c_results[i_result].candidate_length, encoding, candidate, &candidate_length_bytes);
			ml_candidate_string = caml_alloc_string(candidate_length_bytes);
			memcpy(String_val(ml_candidate_string), candidate, candidate_length_bytes);
			bzero(candidate, candidate_length_bytes);
		}
		else
		{
			ml_candidate_string = caml_alloc_string(c_results[i_result].candidate_length * char_width);
			memcpy(String_val(ml_candidate_string), c_results[i_result].candidate, c_results[i_result].candidate_length * char_width);
		}

		ml_result = caml_alloc(2, 0);
		ml_result_tuple = caml_alloc(2, 0);
		Store_field(ml_result_tuple, 0, ml_candidate_string);
		Store_field(ml_result_tuple, 1, caml_copy_double(c_results[i_result].score));
		Store_field(ml_result, 0, ml_result_tuple);
		Store_field(ml_result, 1, ml_results);
		ml_results = ml_result;
	}

	if (strcmp(encoding, "utf8") == 0 || strcmp(encoding, "utf16") == 0)
		free(candidate);

	CAMLreturn(ml_results);
}
