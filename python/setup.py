

from setuptools import setup
from Cython.Build import cythonize
from distutils.extension import Extension
import sys

python_version = sys.version_info[0]

setup(
    name='batch_jaro_winkler',
    version='0.1.6',
    install_requires=["Cython"],
    ext_modules=cythonize([Extension('batch_jaro_winkler', ['cbatch_jaro_winkler.pyx', 'ext/batch_jaro_winkler.c'], language='c')], language_level=python_version)
)
