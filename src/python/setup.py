from distutils.core import setup
from distutils.extension import Extension
from Cython.Build import cythonize

setup(
    name = "Open SoC Debug",
    ext_modules=cythonize([
        Extension("osd",
                  ['src/osd.pyx'],
                  libraries=["osd"])
    ]),
)
