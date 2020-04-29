

/*
 For this file to work with other ruby implementations than MRI, replace everything with:

#include "ext/batch_jaro_winkler.c"
void	Init_batch_jaro_winkler(void){}
*/

#include "ext/batch_jaro_winkler.c"
#include "ruby.h"
#include "ruby/encoding.h"

VALUE	rb_bjw_build_runtime_result(VALUE self, VALUE tmp_store, VALUE rb_results, VALUE rb_c_results, VALUE rb_nb_results, VALUE rb_inp_encoded, VALUE rb_char_width)
{
	bjw_result	*results;
	uint32_t	nb_results;
	uint32_t	i_result;
	VALUE		tmp_candidate;
	rb_encoding	*utf32le_encoding;
	rb_encoding	*utf8_encoding;
	VALUE		rb_utf8_encoding;
	uint32_t	char_width;
	int			inp_encoded;
	char		*all_candidates;
	VALUE		rb_all_candidates;
	uint64_t	total_nb_bytes;
	uint64_t	decal;
	uint64_t	bytes_len;
	uint64_t	candidate_length_in_bytes;
	uint64_t	i_char;

	nb_results = (uint32_t)(NUM2ULL(rb_nb_results));
	results = (bjw_result*)(NUM2ULL(rb_c_results));
	char_width = (uint32_t)(NUM2ULL(rb_char_width));
	inp_encoded = RTEST(rb_inp_encoded);

	utf32le_encoding = rb_enc_find("UTF-32LE");
	utf8_encoding = rb_enc_find("UTF-8");
	rb_utf8_encoding = rb_enc_from_encoding(utf8_encoding);
	// We use tmp_store so that local ruby objects are marked by the GC
	rb_ary_push(tmp_store, rb_utf8_encoding);

	if (!inp_encoded)
	{
		total_nb_bytes = 0;
		for (i_result = 0; i_result < nb_results; i_result++)
			total_nb_bytes += results[i_result].candidate_length;
		total_nb_bytes *= char_width;
		all_candidates = malloc(total_nb_bytes);
		if (!all_candidates)
			return (Qfalse);
		decal = 0;
		for (i_result = 0; i_result < nb_results; i_result++)
		{
			bytes_len = results[i_result].candidate_length * char_width;
			for (i_char = 0; i_char < bytes_len; i_char++)
				all_candidates[decal + i_char] = ((char*)results[i_result].candidate)[i_char];
			decal += bytes_len;
		}
		rb_all_candidates = rb_enc_str_new(all_candidates, total_nb_bytes, utf32le_encoding);
		// We use tmp_store so that local ruby objects are marked by the GC
		rb_ary_push(tmp_store, rb_all_candidates);
		free(all_candidates);
		rb_all_candidates = rb_str_encode(rb_all_candidates, rb_utf8_encoding, 0, Qnil);
		// We use tmp_store so that local ruby objects are marked by the GC
		rb_ary_push(tmp_store, rb_all_candidates);
		all_candidates = RSTRING_PTR(rb_all_candidates);
	}

	decal = 0;
	for (i_result = 0; i_result < nb_results; i_result++)
	{
		if (!inp_encoded)
		{
			candidate_length_in_bytes = 0;
			for (i_char = 0; i_char < results[i_result].candidate_length; i_char++)
			{
				if ((all_candidates[decal + candidate_length_in_bytes] & 0xf8) == 0xf0)
					candidate_length_in_bytes += 4;
				else if ((all_candidates[decal + candidate_length_in_bytes] & 0xf0) == 0xe0)
					candidate_length_in_bytes += 3;
				else if ((all_candidates[decal + candidate_length_in_bytes] & 0xe0) == 0xc0)
					candidate_length_in_bytes += 2;
				else
					candidate_length_in_bytes += 1;
			}
			tmp_candidate = rb_enc_str_new(all_candidates + decal, candidate_length_in_bytes, utf8_encoding);
			decal += candidate_length_in_bytes;
		}
		else
			tmp_candidate = rb_str_new(results[i_result].candidate, results[i_result].candidate_length * char_width);
		rb_ary_push(rb_results, rb_ary_new_from_args(2, tmp_candidate, rb_float_new(results[i_result].score)));
	}
	return (Qtrue);
}

void	Init_batch_jaro_winkler(void)
{
	VALUE	cBatchJaroWinkler;

	cBatchJaroWinkler = rb_define_module("BatchJaroWinkler");
	rb_define_singleton_method(cBatchJaroWinkler, "rb_bjw_build_runtime_result", rb_bjw_build_runtime_result, 6);
}
