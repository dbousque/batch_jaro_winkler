# batch_jaro_winkler
[![Build Status](https://travis-ci.org/dbousque/batch_jaro_winkler.svg?branch=master)](https://travis-ci.org/dbousque/batch_jaro_winkler)
![License: MIT](https://img.shields.io/badge/License-MIT-brightgreen.svg)

Fast batch jaro winkler distance implementation in C99 with Ruby and Python bindings.

This project gets its performance from the pre-calculation of an optimized model in advance of the actual runtime calculations. Supports any encoding.

C99, Python >= 3.3 and Ruby >= 2.1 ([Warning regarding ruby versions](#warning-regarding-ruby-versions))

Language specific parts:
  - [Python](#python)
  - [Ruby](#ruby)
  - [C](#c)

## Benchmark

Linear scale for dramatic effect.

![](benchmark/benchmark.png?raw=true "")

2 datasets are used for the benchmark: english words and chinese words. These datasets are fairly small (~ 5 MB). The bigger the dataset, the greater the speedup you get by using batch_jaro_winkler over other libraries.

The metric `MB/s` refers to the number of megabytes of the original datasets processed per second. The datasets are utf-8 encoded, so 1 byte = 1 character for english text. So if we have an english dataset of 1 million 10 characters words, our dataset is 10 MB, and `batch_jaro_winkler 4 threads` is able to calculate the score for each of the original words against a new value in 10 MB / 400 MB/s = 25 ms with `min_score=0.0`, and 10 MB / 1.7 GB/s = 6 ms with `min_score=0.9`.

Libraries used for comparison: [Levenshtein](https://github.com/ztane/python-Levenshtein), [jellyfish](https://github.com/jamesturk/jellyfish), [hotwater](https://github.com/colinsurprenant/hotwater), [jaro_winkler](https://github.com/tonytonyjan/jaro_winkler), [fuzzy-string-match](https://github.com/kiyoka/fuzzy-string-match), [amatch](https://github.com/flori/amatch), [textdistance](https://github.com/life4/textdistance), [py_stringmatching](https://github.com/anhaidgroup/py_stringmatching), [jaro](https://github.com/richmilne/JaroWinkler), [pyjarowinkler](https://github.com/nap/jaro-winkler-distance)

## Installation

Python: `pip3 install batch_jaro_winkler`

Ruby: `gem install batch_jaro_winkler`

Issues?
- You need a development version of python or ruby, with `apt-get` that would be `apt-get install python3-dev` or `apt-get install ruby-dev`.
- You need a C compiler installed, `gcc` or `clang` for example.
- You need `make` for the ruby library.

## Examples

```python
import batch_jaro_winkler as bjw

candidates = ['héllo', '中国', 'hiz']
exportable_model = bjw.build_exportable_model(candidates)
runtime_model = bjw.build_runtime_model(exportable_model)
res = bjw.jaro_winkler_distance(runtime_model, 'hélloz')
# res = [('中国', 0.0), ('hiz', 0.5), ('héllo', 0.9666666388511658)]
```

Use of `min_score` for each candidate:
```python
candidates = [{ 'candidate': 'héllo', 'min_score': 0.99 }, { 'candidate': '中国', 'min_score': 0.0 }, { 'candidate': 'hiz', 'min_score': 0.4 }]
res = bjw.jaro_winkler_distance(runtime_model, 'hélloz')
# res = [('中国', 0.0), ('hiz', 0.5)]
```

Use of `min_score` as runtime argument, which takes precedence over the min scores for each candidate:
```python
candidates = [{ 'candidate': 'héllo', 'min_score': 0.99 }, { 'candidate': '中国', 'min_score': 0.0 }, { 'candidate': 'hiz', 'min_score': 0.4 }]
res = bjw.jaro_winkler_distance(runtime_model, 'hélloz', min_score=0.5)
# res = [('hiz', 0.5), ('héllo', 0.9666666388511658)]
```

Use of `weight`, `threshold` and `n_best_results`:
```python
candidates = ['héllo', '中国', 'hiz']
res = bjw.jaro_winkler_distance(runtime_model, 'hélloz', weight=0.2, threshold=0.5, n_best_results=1)
# res = [('héllo', 0.9888888597488403)]
```

## Correctness

Different libraries calculate scores differently. The output of this project matches the output of
the original implementation by Bill Winkler, George McLaughlin and Matt Jaro.
See [here](https://github.com/tonytonyjan/jaro_winkler#compare-with-other-gems) for more details.

## How to use

### The exportable model

The first step is to build a model from a set of _candidates_. This model is a simple string (`bytes` in Python) that we can store where we like: RAM, disk, a database, S3 etc.

We can optionally set a `min_score` requirement for each candidate, so that a candidate is only returned at runtime if the matching score is higher than a certain value, except if we manually pass a `min_score` at runtime, which takes precedence.

We need to choose how many threads we want to use for runtime calculations when we build the exportable model, as this value is used for an optimized internal representation.

The exportable model compresses very well, but decompressing at runtime might be too slow depending on your needs:
```python
>>> import gzip
>>> exportable_model = bjw.build_exportable_model(candidates)
>>> len(exportable_model)
34738694
>>> compressed_exportable_model = gzip.compress(exportable_model)
>>> len(compressed_exportable_model)
9443218
```

### The runtime model and runtime calculations

Once we have an exportable model, we can make runtime score calculations. A prerequisite is to build a runtime model. This is a very cheap operation, but to prevent us from doing it over and over we can reuse the model for any number of runtime calculations. Beware that while an exportable model can be used by multiple threads at the same time as we only read from it, we write to the runtime model passed when doing runtime calculations, so it isn't thread safe.

Then come the score calculations. We can perform _jaro_ or _jaro winkler_ distance calculations. We can set the `min_score` argument so that only candidates matching with at least a certain score are returned. This argument overrides the `min_score` that we may have set for each candidate when building the exportable model. A high value improves the performance, see the [benchmark](#benchmark). We can also set the `n_best_results` argument, it filters the candidates and makes the runtime function return only the best scoring candidates. A small value (< 20% of the dataset size) improves the performance, see the [benchmark](#benchmark).

## How does it work?

One possible approach to performing jaro winkler distance calculations against a set of
values is to test each value one after the other. This project leverages the fact that
values are known in advance to build a data structure (a _model_) that prevents us
from looping to find matches, using a hash table linking to a list of matches instead.
More often than not, you only care about matches that have a score higher than a threshold
value. We benefit from this fact by skipping candidates that can't match the threshold as the
calculations go on, allowing us to speed up further.

We calculate the jaro winkler distance in 2 steps. The first one calculates the number of
matches for a candidate, and populates a 'flags' data structure that is used later on
to calculate the number of transpositions.

These would be the flags for 'im marhta yo' as the input and 'martha' as the candidate:

```
input_flags = [0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0]
cand_flags_flags = [1, 1, 1, 1, 1, 1]
```

The data structure used to speed up the finding of matching characters is a hash table
linking characters to their occurrences in candidates. It is stored this way:
```
{
  'a': |  cand1 |  cand1 |  cand1 |  cand1 |  nb_oc |  ind1 |  ind2 |  ind3 |  cand2 | ...,
  'b': |  cand1 |  cand1 |  cand1 |  cand1 |  nb_oc |  ind1 |  ind2 |  cand2 |  ...,
}
```
Where 'a' and 'b' are characters which appear in candidates, 'cand1' and 'cand2' are the
indexes of the candidates in the model, 'nb_oc' is the number of occurrences of the character in
the candidate and 'ind1', 'ind2' etc. are the indexes of the character's occurrences
in the candidate.

Additionally, we can know in advance how many occurrences matches we need to satisfy the
`min_score` requirement. This allows us to ignore candidates once we know that they won't
be able to match. For example, with a `min_score` of 1.0:

```
runtime_input_len = 8, candidate_len = 8 => We know that we must have 8 matches
runtime_input_len = 9, candidate_len = 8 => We know that it is impossible that they match
```

With a `min_score` of 0.9:

```
runtime_input_len = 8, candidate_len = 8 =>
  (3.0 * min_score * candidate_len * runtime_input_len - (candidate_len * runtime_input_len)) / (candidate_len + runtime_input_len)
  => 6.8, we need at least 7 matches
runtime_input_len = 9, candidate_len = 8 =>
  (3.0 * min_score * candidate_len * runtime_input_len - (candidate_len * runtime_input_len)) / (candidate_len + runtime_input_len)
  => 7.2, we need at least 8 matches
```

This data representation allows us to have an efficient runtime, which looks something like this (simplified):

```python
for i_char, char in runtime_input:
  remaining_chars = len(runtime_input) - i_char
  occurrences = char_matches[char]
  for candidate_ind, indexes in occurrences:
    enough_matches_possible = candidates[candidate_ind].nb_matches + remaining_chars >= candidates[candidate_ind].required_nb_matches
    if not enough_matches_possible:
      continue
    for ind in indexes:
      match_in_search_range = ind >= i_char - candidates[candidate_ind].search_range and ind <= i_char + candidates[candidate_ind].search_range
      if match_in_search_range:
        candidates[candidate_ind].nb_matches += 1
        runtime_input_flags[candidate_ind][i_char] = 1
        candidates_flags[candidate_ind][ind] = 1
        break
```

Once this is done, all that is left to do is to calculate the number of transpositions
for possibly matching candidates (candidates that have a number of matches at least equal
to the required number of matches) from the flags.

Things that were tried but did not improve performance. They could still bring performance gains if done right:
  - Instead of iterating over all occurrences matches for a character everytime, keep a linked list (as offsets in the candidates' array) of possible
    candidate occurrences for a given character. We can eliminate candidate occurrences when we know a candidate
    can't match, or when we explored all occurrences for this candidate and this character.
  - Using bits instead of uint8_t for runtime_input_flags and candidate_flags.
  - Keep track of the number of potential matches left for a candidate, that way we can skip impossible candidates
    in try_to_match_occurrence the same way we do with nb_matches + remaining_chars < required_nb_matches

Things that were not tried:
  - Sort candidates by jaro distance when building the model. This would greatly optimize cache usage, as potential
    candidates for a particular runtime_input would all be near one another, making memory accesses more efficient.
    Right now we are using an approximation of this, sorting by alphabetical order + length.
  - Better split candidates across each thread's local storage so that each thread takes around the same time to finish.
    This would greatly improve the multi-threading performance, which caps at around 3 times faster than with 1 thread,
    even with 6 threads.
  - If bits are used for the flags, maybe use binary operations for transpositions calculation? Seems unlikely.
  - Use SIMD instructions, probably tough because there is a lot of branching right now.
  - Keep track of the current best score when finding matches, to invalidate candidates that are guaranteed to
    have a smaller score. Same when calculating transpositions.

## Python

```python
import batch_jaro_winkler as bjw

candidates = ['héllo', '中国', 'hiz']
exportable_model = bjw.build_exportable_model(candidates)
runtime_model = bjw.build_runtime_model(exportable_model)
res = bjw.jaro_winkler_distance(runtime_model, 'hélloz')
# res = [('中国', 0.0), ('hiz', 0.5), ('héllo', 0.9666666388511658)]
```

- #### build_exportable_model

```python
build_exportable_model(candidates, nb_runtime_threads=1)
```
Parameter           | Type      | Comment
------------------- | --------- | ---------------------------------------------------------------------------------------------------
candidates          | list-like | A list (or list-like object) containing the strings to match runtime values against. Must respect one of these 2 schemas: `['hi', 'hello']` or `[{ 'candidate': 'hi', 'min_score': 0.5 }, { 'candidate': 'hello', 'min_score': 0.8 }]`. If one candidate has a `min_score`, all of them must have one. If `min_score` is provided, a candidate is only returned at runtime if the matching score is higher than its min score specified here, except if we manually pass a `min_score` at runtime, which takes precedence.
nb_runtime_threads  | int       | The number of threads to use at runtime (`jaro_distance[_bytes]` and `jaro_winkler_distance[_bytes]`).

Returns a `bytes` object.


- #### build_exportable_model_bytes

```python
build_exportable_model_bytes(char_width, candidates, nb_runtime_threads=1)
```
Parameter           | Type      | Comment
------------------- | --------- | ---------------------------------------------------------------------------------------------------
char_width          | int       | Must be one of {1, 2, 4}. The width in bytes of a single character in the strings you provide in the `candidates` parameter. For example, if you use `utf-32`, set `char_width` to 4.
candidates          | list-like | A list (or list-like object) containing the strings to match runtime values against. They can be encoded however you like, including in custom encodings, and including with `0` characters in the middle of the encoded strings. Must respect one of these 2 schemas: `[b'hi', b'hello']` or `[{ 'candidate': b'hi', 'min_score': 0.5 }, { 'candidate': b'hello', 'min_score': 0.8 }]`. If one candidate has a `min_score`, all of them must have one. If `min_score` is provided, a candidate is only returned at runtime if the matching score is higher than its min score specified here, except if we manually pass a `min_score` at runtime, which takes precedence.
nb_runtime_threads  | int       | The number of threads to use at runtime (`jaro_distance[_bytes]` and `jaro_winkler_distance[_bytes]`).

Returns a `bytes` object.


- #### build_runtime_model

```python
build_runtime_model(exportable_model)
```
Parameter           | Type      | Comment
------------------- | --------- | ---------------------------------------------------------------------------------------------------
exportable_model    | bytes     | An exportable model built with `build_exportable_model[_bytes]`.

Returns an object that you can then pass as argument to one of the runtime functions: `jaro_distance[_bytes]` and `jaro_winkler_distance[_bytes]`.


- #### jaro_winkler_distance

```python
jaro_winkler_distance(runtime_model, inp, min_score=None, weight=0.1, threshold=0.7, n_best_results=None)
```
Parameter           | Type         | Comment
------------------- | ------------ | ---------------------------------------------------------------------------------------------------
runtime_model       | RuntimeModel | Object returned by `build_runtime_model`.
inp                 | str          | The input to get scores for.
min_score           | float        | If set, the function only returns the candidates that have a matching score at least as high as this value. Improves performance. Takes precedence over the min scores that may be set for each candidate when building the exportable model.
weight              | float        | The jaro winkler algorithm gives a scoring bonus for matching prefixes, this parameter controls how big the bonus is. This value must be >= 0.0 and <= 0.25. For the standard jaro winkler score calculation, use the default value.
threshold           | float        | The jaro winkler algorithm gives a scoring bonus for matching prefixes, this parameter controls what the minimum score should be as a condition for applying the bonus. For the standard jaro winkler score calculation, use the default value.
n_best_results      | int          | Makes the function return the `n_best_results` best scoring candidates only. Improves performance.

Returns a list of tuples containing the candidates and the matching scores, following this schema: `[('中国', 0.0), ('hiz', 0.5)]`.


- #### jaro_winkler_distance_bytes

```python
jaro_winkler_distance_bytes(char_width, runtime_model, inp, min_score=None, weight=0.1, threshold=0.7, n_best_results=None)
```
Parameter           | Type         | Comment
------------------- | ------------ | ---------------------------------------------------------------------------------------------------
char_width          | int          | Must be one of {1, 2, 4}. The value used must match with the `char_width` passed when calling `build_exportable_model`. The width in bytes of a single character in the `inp` parameter, as well as in the candidates in the exportable model.
runtime_model       | RuntimeModel | Object returned by `build_runtime_model`.
inp                 | bytes        | The input to get scores for.
min_score           | float        | If set, the function only returns the candidates that have a matching score at least as high as this value. Improves performance. Takes precedence over the min scores that may be set for each candidate when building the exportable model.
weight              | float        | The jaro winkler algorithm gives a scoring bonus for matching prefixes, this parameter controls how big the bonus is. This value must be >= 0.0 and <= 0.25. For the standard jaro winkler score calculation, use the default value.
threshold           | float        | The jaro winkler algorithm gives a scoring bonus for matching prefixes, this parameter controls what the minimum score should be as a condition for applying the bonus. For the standard jaro winkler score calculation, use the default value.
n_best_results      | int          | Makes the function return the `n_best_results` best scoring candidates only. Improves performance.

Returns a list of tuples containing the candidates (as bytes) and the matching scores, following this schema: `[(b'-N\x00\x00\xfdV\x00\x00', 0.0), (b'hiz', 0.5)]`.


- #### jaro_distance

```python
jaro_distance(runtime_model, inp, min_score=None, n_best_results=None)
```
Parameter           | Type         | Comment
------------------- | ------------ | ---------------------------------------------------------------------------------------------------
runtime_model       | RuntimeModel | Object returned by `build_runtime_model`.
inp                 | bytes        | The input to get scores for.
min_score           | float        | If set, the function only returns the candidates that have a matching score at least as high as this value. Improves performance. Takes precedence over the min scores that may be set for each candidate when building the exportable model.
n_best_results      | int          | Makes the function return the `n_best_results` best scoring candidates only. Improves performance.

Returns a list of tuples containing the candidates and the matching scores, following this schema: `[('中国', 0.0), ('hiz', 0.5)]`.


- #### jaro_distance_bytes

```python
jaro_distance_bytes(char_width, runtime_model, inp, min_score=None, n_best_results=None)
```
Parameter           | Type         | Comment
------------------- | ------------ | ---------------------------------------------------------------------------------------------------
char_width          | int          | Must be one of {1, 2, 4}. The value used must match with the `char_width` passed when calling `build_exportable_model`. The width in bytes of a single character in the `inp` parameter, as well as in the candidates in the exportable model.
runtime_model       | RuntimeModel | Object returned by `build_runtime_model`.
inp                 | bytes        | The input to get scores for.
min_score           | float        | If set, the function only returns the candidates that have a matching score at least as high as this value. Improves performance. Takes precedence over the min scores that may be set for each candidate when building the exportable model.
n_best_results      | int          | Makes the function return the `n_best_results` best scoring candidates only. Improves performance.

Returns a list of tuples containing the candidates and the matching scores, following this schema: `[(b'-N\x00\x00\xfdV\x00\x00', 0.0), (b'hiz', 0.5)]`.


## Ruby

```ruby
require 'batch_jaro_winkler'

candidates = ['héllo', '中国', 'hiz']
exportable_model = BatchJaroWinkler.build_exportable_model(candidates)
runtime_model = BatchJaroWinkler.build_runtime_model(exportable_model)
res = BatchJaroWinkler.jaro_winkler_distance(runtime_model, 'hélloz')
# res = [['中国', 0.0], ['hiz', 0.5], ['héllo', 0.9666666388511658]]
```

- #### build_exportable_model

```ruby
build_exportable_model(candidates, nb_runtime_threads: 1)
```
Parameter           | Type       | Comment
------------------- | ---------- | ---------------------------------------------------------------------------------------------------
candidates          | array-like | An array (or array-like object) containing the strings to match runtime values against. Must respect one of these 2 schemas: `['hi', 'hello']` or `[{ candidate: 'hi', min_score: 0.5 }, { candidate: 'hello', min_score: 0.8 }]`. If one candidate has a `min_score`, all of them must have one. If `min_score` is provided, a candidate is only returned at runtime if the matching score is higher than its min score specified here, except if we manually pass a `min_score` at runtime, which takes precedence.
nb_runtime_threads  | Integer    | The number of threads to use at runtime (`jaro_distance[_bytes]` and `jaro_winkler_distance[_bytes]`).

Returns an ascii encoded `String` object.


- #### build_exportable_model_bytes

```ruby
build_exportable_model_bytes(char_width, candidates, nb_runtime_threads: 1)
```
Parameter           | Type       | Comment
------------------- | ---------- | ---------------------------------------------------------------------------------------------------
char_width          | Integer    | Must be one of {1, 2, 4}. The width in bytes of a single character in the strings you provide in the `candidates` parameter. For example, if you use `utf-32`, set `char_width` to 4.
candidates          | array-like | An array (or array-like object) containing the strings to match runtime values against. They can be encoded however you like, including in custom encodings, and including with `0` characters in the middle of the encoded strings. Must respect one of these 2 schemas: `['hi'.encode('utf-32le'), 'hello'.encode('utf-32le')]` or `[{ candidate: 'hi'.encode('utf-32le'), min_score: 0.5 }, { candidate: 'hello'.encode('utf-32le'), min_score: 0.8 }]`. If one candidate has a `min_score`, all of them must have one. If `min_score` is provided, a candidate is only returned at runtime if the matching score is higher than its min score specified here, except if we manually pass a `min_score` at runtime, which takes precedence.
nb_runtime_threads  | Integer    | The number of threads to use at runtime (`jaro_distance[_bytes]` and `jaro_winkler_distance[_bytes]`).

Returns an ascii encoded `String` object.


- #### build_runtime_model

```ruby
build_runtime_model(exportable_model)
```
Parameter           | Type      | Comment
------------------- | --------- | ---------------------------------------------------------------------------------------------------
exportable_model    | String    | An exportable model built with `build_exportable_model[_bytes]`.

Returns an object that you can then pass as argument to one of the runtime functions: `jaro_distance[_bytes]` and `jaro_winkler_distance[_bytes]`.


- #### jaro_winkler_distance

```ruby
jaro_winkler_distance(runtime_model, inp, min_score: nil, weight: 0.1, threshold: 0.7, n_best_results: nil)
```
Parameter           | Type         | Comment
------------------- | ------------ | ---------------------------------------------------------------------------------------------------
runtime_model       | RuntimeModel | Object returned by `build_runtime_model`.
inp                 | String       | The input to get scores for.
min_score           | Float        | If set, the function only returns the candidates that have a matching score at least as high as this value. Improves performance. Takes precedence over the min scores that may be set for each candidate when building the exportable model.
weight              | Float        | The jaro winkler algorithm gives a scoring bonus for matching prefixes, this parameter controls how big the bonus is. This value must be >= 0.0 and <= 0.25. For the standard jaro winkler score calculation, use the default value.
threshold           | Float        | The jaro winkler algorithm gives a scoring bonus for matching prefixes, this parameter controls what the minimum score should be as a condition for applying the bonus. For the standard jaro winkler score calculation, use the default value.
n_best_results      | Integer      | Makes the function return the `n_best_results` best scoring candidates only. Improves performance.

Returns an array of arrays containing the candidates and the matching scores, following this schema: `[['中国', 0.0], ['hiz', 0.5]]`.


- #### jaro_winkler_distance_bytes

```ruby
jaro_winkler_distance_bytes(char_width, runtime_model, inp, min_score: nil, weight: 0.1, threshold: 0.7, n_best_results: nil)
```
Parameter           | Type         | Comment
------------------- | ------------ | ---------------------------------------------------------------------------------------------------
char_width          | Integer      | Must be one of {1, 2, 4}. The value used must match with the `char_width` passed when calling `build_exportable_model`. The width in bytes of a single character in the `inp` parameter, as well as in the candidates in the exportable model.
runtime_model       | RuntimeModel | Object returned by `build_runtime_model`.
inp                 | String       | The input to get scores for.
min_score           | Float        | If set, the function only returns the candidates that have a matching score at least as high as this value. Improves performance. Takes precedence over the min scores that may be set for each candidate when building the exportable model.
weight              | Float        | The jaro winkler algorithm gives a scoring bonus for matching prefixes, this parameter controls how big the bonus is. This value must be >= 0.0 and <= 0.25. For the standard jaro winkler score calculation, use the default value.
threshold           | Float        | The jaro winkler algorithm gives a scoring bonus for matching prefixes, this parameter controls what the minimum score should be as a condition for applying the bonus. For the standard jaro winkler score calculation, use the default value.
n_best_results      | Integer      | Makes the function return the `n_best_results` best scoring candidates only. Improves performance.

Returns an array of arrays containing the candidates and the matching scores, following this schema: `[['\u4E2D\u56FD', 0.0], ['hiz', 0.5]]`.


- #### jaro_distance

```ruby
jaro_distance(runtime_model, inp, min_score: nil, n_best_results: nil)
```
Parameter           | Type         | Comment
------------------- | ------------ | ---------------------------------------------------------------------------------------------------
runtime_model       | RuntimeModel | Object returned by `build_runtime_model`.
inp                 | String       | The input to get scores for.
min_score           | Float        | If set, the function only returns the candidates that have a matching score at least as high as this value. Improves performance. Takes precedence over the min scores that may be set for each candidate when building the exportable model.
n_best_results      | Integer      | Makes the function return the `n_best_results` best scoring candidates only. Improves performance.

Returns an array of arrays containing the candidates and the matching scores, following this schema: `[['中国', 0.0], ['hiz', 0.5]]`.


- #### jaro_distance_bytes

```ruby
jaro_distance_bytes(char_width, runtime_model, inp, min_score: nil, n_best_results: nil)
```
Parameter           | Type         | Comment
------------------- | ------------ | ---------------------------------------------------------------------------------------------------
char_width          | Integer      | Must be one of {1, 2, 4}. The value used must match with the `char_width` passed when calling `build_exportable_model`. The width in bytes of a single character in the `inp` parameter, as well as in the candidates in the exportable model.
runtime_model       | RuntimeModel | Object returned by `build_runtime_model`.
inp                 | String       | The input to get scores for.
min_score           | Float        | If set, the function only returns the candidates that have a matching score at least as high as this value. Improves performance. Takes precedence over the min scores that may be set for each candidate when building the exportable model.
n_best_results      | Integer      | Makes the function return the `n_best_results` best scoring candidates only. Improves performance.

Returns an array of arrays containing the candidates and the matching scores, following this schema: `[['\u4E2D\u56FD', 0.0], ['hiz', 0.5]]`.


## C

The files you need are in the `lib` folder.

`test.c`:
```c
#include "batch_jaro_winkler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    char      *candidates[] = { "hello", "hiz" };
    uint32_t  candidates_lengths[] = { 5, 3 };
    uint32_t  exportable_model_size;
    uint32_t  nb_results;

    // char_width = 1 ; nb_candidates = 2 ; nb_runtime_threads = 1
    void *exportable_model = bjw_build_exportable_model(
      candidates, 1, candidates_lengths, 2, NULL, 1, &exportable_model_size
    );
    if (!exportable_model)
        exit(1);
    void *runtime_model = bjw_build_runtime_model(exportable_model);
    if (!runtime_model)
        exit(1);

    // input_length = 5 ; min_score = -1.0 (deactivate)
    // weight = 0.1 (default value for standard jaro winkler)
    // threshold = 0.7 (default value for standard jaro winkler)
    // n_best_results = 0 (deactivate)
    bjw_result *res = bjw_jaro_winkler_distance(runtime_model, "hallo", 5, -1.0, 0.1, 0.7, 0, &nb_results);
    if (!res)
        exit(1);

    uint32_t best_candidate_ind = 0;
    for (uint32_t i_res = 0; i_res < nb_results; i_res++)
    {
        // Warning: candidates are not null terminated, as the meaning of bytes within candidates
        // depends on the encoding, including for 0.
        printf(
            "{ .candidate = \"%.*s\", .score = %f }\n",
            res[i_res].candidate_length, res[i_res].candidate, res[i_res].score
        );
        if (res[i_res].score > res[best_candidate_ind].score)
            best_candidate_ind = i_res;
    }

    // Important: the 'candidate' field in `bjw_result` is a pointer to somewhere within the exportable model.
    // If you want to keep candidates after the exportable model is being freed, you must copy the data.
    // char_width = 1
    char *best_candidate = malloc(res[best_candidate_ind].candidate_length * 1);
    memcpy(best_candidate, res[best_candidate_ind].candidate, res[best_candidate_ind].candidate_length * 1);
    uint32_t best_candidate_length = res[best_candidate_ind].candidate_length;

    free(res);
    bjw_free_runtime_model(runtime_model);
    free(exportable_model);

    printf("best candidate: \"%.*s\"\n", best_candidate_length, best_candidate);
    free(best_candidate);
    return (0);
}
```

```
$ ls -l
-rw-r--r--  1 user  wheel  33490 27 avr 13:12 batch_jaro_winkler.c
-rw-r--r--  1 user  wheel   1111 27 avr 13:12 batch_jaro_winkler.h
-rw-r--r--  1 user  wheel   1533 27 avr 13:12 batch_jaro_winkler_internal.h
-rw-r--r--  1 user  wheel  22514 27 avr 13:12 batch_jaro_winkler_runtime.h
-rw-r--r--  1 user  wheel   2190 27 avr 13:15 test.c
-rw-r--r--  1 user  wheel  78701 27 avr 13:12 uthash.h
$ gcc -O3 batch_jaro_winkler.c test.c
$ ./a.out
{ .candidate = "hiz", .score = 0.511111 }
{ .candidate = "hello", .score = 0.880000 }
best candidate: "hello"
```

- #### bjw_build_exportable_model

```c
void  *bjw_build_exportable_model(
    void **candidates, uint32_t char_width, uint32_t *candidates_lengths, uint32_t nb_candidates,
    float *min_scores, uint32_t nb_runtime_threads, uint32_t *res_model_size
)
```
Parameter           | Type         | Comment
------------------- | ------------ | ---------------------------------------------------------------------------------------------------
candidates          | void**       | An array of character arrays. Each character must be `char_width` bytes wide. They can be encoded however you like, including in custom encodings, and including with `0` characters in the middle of the encoded strings.
char_width          | uint32_t     | Must be one of {1, 2, 4}. The width in bytes of a single character in the strings you provide in the `candidates` parameter. For example, if you use `utf-32`, set `char_width` to 4.
candidates_lengths  | uint32_t*    | Array containing the length of each candidate. If the strings are null-terminated, don't count the last byte when determining the length.
nb_candidates       | uint32_t     | The number of elements in the `candidates`, `candidates_lengths` and `min_scores` arrays.
min_scores          | float*       | Can be NULL. If provided, a candidate is only returned at runtime if the matching score is higher than its min score specified here, except if we manually pass a `min_score` at runtime, which takes precedence.
nb_runtime_threads  | uint32_t     | The number of threads to use at runtime (`bjw_jaro_distance` and `bjw_jaro_winkler_distance`).
res_model_size      | uint32_t*    | The value is set by the function to the size in bytes of the resulting exportable model.

Returns a buffer. You are responsible for freeing it. Warning: you need to keep it in memory as long as you want to make runtime calculations if you passed it as argument to `bjw_build_runtime_model`, as the runtime functions (`bjw_jaro_distance` and `bjw_jaro_winkler_distance`) return pointers to the candidates' strings stored inside the exportable model.

- #### bjw_build_runtime_model

```c
void  *bjw_build_runtime_model(void *exportable_model)
```
Parameter           | Type      | Comment
------------------- | --------- | ---------------------------------------------------------------------------------------------------
exportable_model    | void*     | An exportable model built with `bjw_build_exportable_model`.

Returns an object that you can then pass as argument to one of the runtime functions: `bjw_jaro_distance` and `bjw_jaro_winkler_distance`. You must call `bjw_free_runtime_model` when you are done using it.

- #### bjw_free_runtime_model

```c
void  bjw_free_runtime_model(void *runtime_model)
```
Parameter           | Type      | Comment
------------------- | --------- | ---------------------------------------------------------------------------------------------------
runtime_model       | void*     | A runtime model built with `bjw_build_runtime_model`.

Frees the runtime model.

- #### bjw_jaro_winkler_distance

```c
bjw_result  *bjw_jaro_winkler_distance(
    void *runtime_model, void *input, uint32_t input_length, float min_score,
    float weight, float threshold, uint32_t n_best_results, uint32_t *nb_results
)
```
Parameter           | Type         | Comment
------------------- | ------------ | ---------------------------------------------------------------------------------------------------
runtime_model       | void*        | Object returned by `bjw_build_runtime_model`.
input               | void*        | The input to get scores for. Must be encoded in the same way as candidates.
input_length        | uint32_t     | Length of the input in characters, not bytes.
min_score           | float        | If >= 0.0, the function only returns the candidates that have a matching score at least as high as this value. Improves performance. Takes precedence over the min scores that may be set for each candidate when building the exportable model.
weight              | float        | The jaro winkler algorithm gives a scoring bonus for matching prefixes, this parameter controls how big the bonus is. This value must be >= 0.0 and <= 0.25. For the standard jaro winkler score calculation, use the default value.
threshold           | float        | The jaro winkler algorithm gives a scoring bonus for matching prefixes, this parameter controls what the minimum score should be as a condition for applying the bonus. For the standard jaro winkler score calculation, use the default value.
n_best_results      | uint32_t     | If > 0, the function returns the `n_best_results` best scoring candidates only. Improves performance.
nb_results          | uint32_t*    | This value is set by the function to the number of results.

Returns an array of `*nb_results` `bjw_result`. You are responsible for freeing the resulting array. Here is the definition for `bjw_result`, `candidate_length` is the number of characters, not bytes:
```c
typedef struct
{
  void      *candidate;
  float     score;
  uint32_t  candidate_length;
} bjw_result;
```

- #### bjw_jaro_distance

```c
bjw_result  *bjw_jaro_distance(
    void *runtime_model, void *input, uint32_t input_length,
    float min_score, uint32_t n_best_results, uint32_t *nb_results
)
```
Parameter           | Type         | Comment
------------------- | ------------ | ---------------------------------------------------------------------------------------------------
runtime_model       | void*        | Object returned by `bjw_build_runtime_model`.
input               | void*        | The input to get scores for. Must be encoded in the same way as candidates.
input_length        | uint32_t     | Length of the input in characters, not bytes.
min_score           | float        | If >= 0.0, the function only returns the candidates that have a matching score at least as high as this value. Improves performance. Takes precedence over the min scores that may be set for each candidate when building the exportable model.
n_best_results      | uint32_t     | If > 0, the function returns the `n_best_results` best scoring candidates only. Improves performance.
nb_results          | uint32_t*    | This value is set by the function to the number of results.

Returns an array of `*nb_results` `bjw_result`. You are responsible for freeing the resulting array. Here is the definition for `bjw_result`, `candidate_length` is the number of characters, not bytes:
```c
typedef struct
{
  void      *candidate;
  float     score;
  uint32_t  candidate_length;
} bjw_result;
```

## Warning regarding Ruby versions
If you use older MRI versions (< 2.5.8 or between 2.6.0 and 2.6.4 included), you may experience memory leaks. It could somehow be related to MRI's string implementation, which was fixed at the end of 2019: https://github.com/ruby/ruby/compare/v2_6_4...v2_6_5#diff-7a2f2c7dfe0bf61d38272aeaf68ac768R2117. Work was done in this library to mitigate the issue, but the absence of leaks is not guaranteed. If you're interested, you can most likely reproduce the leak with this program, change `utf-32le` to `utf-32` to watch to memory leak disappear:
```ruby
while true do
  1000.times do
    # random 10 characters string
    str = (0...10).map{ (65 + rand(26)).chr }.join
    str.encode('utf-32le')
  end
  GC.start(full_mark: true, immediate_sweep: true)
  GC.start
end
```

## The future

A similar approach could probably also benefit other fuzzy matching algorithms.

Something that would be really neat would be the ability to add candidates or remove candidates from an exportable model on the fly.
This would allow for more flexible scenarios, where the set of candidates can change very often, as in a database for instance.
It would make the project appropriate as a PostgreSQL extension for example.
