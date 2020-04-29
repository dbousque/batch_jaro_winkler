

from distutils.core import setup
from distutils.extension import Extension
from Cython.Build import cythonize
import sys

python_version = sys.version_info[0]

setup(
  name='batch_jaro_winkler',
  ext_modules=cythonize([Extension('batch_jaro_winkler', ['cbatch_jaro_winkler.pyx'])], language_level=python_version)
)