

from distutils.core import setup
from distutils.extension import Extension

setup(
  name='batch_jaro_winkler',
  version='0.1.0',
  description='Fast batch jaro winkler distance implementation in C99.',
  long_description='This project gets its performance from the pre-calculation of an optimized model in advance of the actual runtime calculations. Supports any encoding.',
  author='Dominik Bousquet',
  author_email='bousquet.dominik@gmail.com',
  url='https://github.com/dbousque/batch_jaro_winkler',
  license='MIT',
  # I know, doesn't work but I don't want to use setuptools. Won't compile if < 3.3 anyway.
  python_requires='>=3.3',
  ext_modules=[Extension('batch_jaro_winkler', ['cbatch_jaro_winkler.c', 'ext/batch_jaro_winkler.c'], language='c')]
)