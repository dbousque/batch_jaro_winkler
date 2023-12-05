import matplotlib.pyplot as plt
import bjw_times
import numpy as np
import matplotlib.ticker as ticker
from scipy.interpolate import InterpolatedUnivariateSpline
from scipy.interpolate import interp1d

font = 'BentonSans'
mode = 'log'

res = bjw_times.res
for r in res:
  if r['file'] == '../support/chinese_words.txt':
    r['nb_bytes'] = 4479572
  if r['file'] == '../support/english_words.txt':
    r['nb_bytes'] = 4863004
times_per_min_score = { None: { 0.0: {}, 0.5: {}, 0.8: {}, 0.9: {} }, 10: { 0.0: {}, 0.5: {}, 0.8: {}, 0.9: {} } }
for r in res:
  for lib, time in r['times'].items():
    if not lib in times_per_min_score[r['n_best_results']][r['min_score']]:
      times_per_min_score[r['n_best_results']][r['min_score']][lib] = { 'time': 0.0, 'nb_bytes': 0 }
    times_per_min_score[r['n_best_results']][r['min_score']][lib]['time'] += time
    times_per_min_score[r['n_best_results']][r['min_score']][lib]['nb_bytes'] += r['nb_bytes'] * 100
plots_by_lib = {}
min_scores_x = [0.0, 0.5, 0.8, 0.9]
for min_score in min_scores_x:
  lib_results = times_per_min_score[None][min_score]
  for lib, lib_res in lib_results.items():
    if not lib in plots_by_lib:
      plots_by_lib[lib] = []
    plots_by_lib[lib].append(lib_res['nb_bytes'] / lib_res['time'] / 1000 / 1000)
plots_by_lib['batch_jaro_winkler 4 threads\nn_best_results=10'] = []
for min_score, lib_results in times_per_min_score[10].items():
  lib_res = lib_results['batch_jaro_winkler 4 threads']
  plots_by_lib['batch_jaro_winkler 4 threads\nn_best_results=10'].append(lib_res['nb_bytes'] / lib_res['time'] / 1000 / 1000)
fix, ax = plt.subplots(figsize=(7, 9))
for lib, y in sorted(list(plots_by_lib.items()), key=lambda x: -x[1][-1]):
  width = 1.0 if 'batch_jaro_winkler' in lib else 1.0
  ax.plot(min_scores_x, y, linewidth=width, label=lib)

performance_base = plots_by_lib['Levenshtein'][-1]

for line in ax.lines:
  lib = line.get_label()
  y = line.get_ydata()[-1]
  perf = plots_by_lib[lib][-1] / performance_base
  y_text = 0
  if mode == 'log' and lib == 'batch_jaro_winkler 4 threads':
    y_text = 7
  if lib == 'batch_jaro_winkler 4 threads\nn_best_results=10':
    y_text = -5
  if mode == 'log' and lib == 'jellyfish':
    y_text = 3
  if mode == 'log' and lib == 'jaro_winkler':
    y_text = -1
  if mode == 'log' and lib == 'fuzzy-string-match':
    y_text = -3
  ax.annotate('%.{}fx'.format('1' if perf >= 10 else '2') % (perf,), xy=(1,y), xytext=(6,y_text), color=line.get_color(), 
              xycoords = ax.get_yaxis_transform(), textcoords="offset points",
              size=10, va="center", fontname=font)
  ax.annotate(lib.replace('batch_jaro_winkler', 'bjw'), xy=(1,y), xytext=(40,y_text), color=line.get_color(), 
              xycoords = ax.get_yaxis_transform(), textcoords="offset points",
              size=10, va="center", fontname=font)

class CustomTicker(ticker.ScalarFormatter):
  def __call__(self, x, pos=None):
    return '{} MB/s'.format(x)

title = None
ticks = None
if mode == 'linear':
  title = 'Linear scale'
  ticks = [200, 400, 600, 800, 1000, 1200, 1400, 1600]
else:
  plt.yscale('log')
  title = 'Logarithmic scale'
  ticks = [1, 3, 6, 12, 24, 48, 96, 192, 384, 768, 1536]
plt.title(title, fontname=font)
plt.yticks(ticks, fontname=font)
plt.xticks([0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9], fontname=font)
ax.get_yaxis().set_major_formatter(CustomTicker())
plt.xlabel('min_score', fontname=font)
plt.hlines([100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100, 1200, 1300, 1400, 1500, 1600], 0, 0.9, linestyles='dashed', linewidth=0.5)
#plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.05), ncol=3, fancybox=True, shadow=True)
#plt.legend()
plt.minorticks_off()
plt.show()