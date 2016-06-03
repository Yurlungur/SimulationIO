"""The setup module for pysimulationio
"""

from setuptools import setup, find_packages
from setuptools.extension import Extension
from distutils.sysconfig import get_python_lib, get_python_inc
from distutils.command.build import build
from setuptools.command.install import install
from distutils.command.clean import clean
from os import path, environ
import os, glob
from shutil import rmtree

# access some simple stuff
here = path.abspath(path.dirname(__file__))
py_include_dirs=list(set([get_python_inc(i) for i in range(2)]
                         +[get_python_lib(i,j)\
                           for i,j in zip(range(2),range(2))]))

# Read in the package information generated by the makefile.
with open('VERSION','r') as f:
    version = float(f.read().strip())

with open('README.md','r') as f:
    long_description = f.read()

try:
    with open('includes.txt','r') as f:
        includes = [x.strip() for x in f.readlines()]
    includes += py_include_dirs
    includes = [path.realpath(x) for x in includes]

    with open('flags.txt','r') as f:
        flags = f.read().strip().split(' ')

    with open('links.txt','r') as f:
        links = [x.strip() for x in f.readlines()]
    links = [path.realpath(x) for x in links]
    link_args = ["-Wl,-rpath,{}".format(p) for p in links]

    with open('cxx.txt','r') as f:
        cxx = f.read().strip()
    environ["CC"] = cxx # Python only looks at CC.

    with open('libs.txt','r') as f:
        libs = [x. strip() for x in f.readlines()]
    libs = reduce(lambda x,y: x+y,[x.split(' ') for x in libs])
    libs = [x.lstrip('-l') for x in libs]

    with open('lib_sources.txt','r') as f:
        lib_sources = f.read().strip().split(' ')
except IOError:
    raise IOError("There is missing metadata. Try running "
                  +"'make meta' to generate it.")


all_object_files = glob.glob(path.join(here,'*.o'))
if len(all_object_files) == 0:
    raise IOError("There are missing object files! Try running "
                  +"'make' to build SimulationIO.")
object_files = [src.replace('.cpp','.o') for src in lib_sources]

swig_modules = ['H5','SimulationIO','RegionCalculus']
swig_module_files = [path.join(here,"{}.py".format(m)) for m in swig_modules]
swig_module_targets = [path.join(here,'pysimulationio',"{}.py".format(m))\
                       for m in swig_modules]
swig_wrap_files = (
    [path.join(here,"{}_wrap.cxx".format(m)) for m in swig_modules]
    +[path.join(here,"{}_wrap.o".format(m)) for m in swig_modules])
swig_opts = ['-c++','-Wall']

# Overload the build and install commands so that swig behaves. See:
# stackoverflow.com/questions/12491328/python-distutils-not-include-the-swig-generated-module
# I would consider this a bug in setuptools.
class CustomBuild(build):
    def run(self):
        self.run_command('build_ext')
        # We copy the *.py files generated by swig by hand.
        # TODO: find cleaner way to handle this
        for i in range(len(swig_modules)):
            if path.exists(swig_module_targets[i]):
                os.remove(swig_module_targets[i])
            if path.exists(swig_module_files[i]):
                os.rename(swig_module_files[i],swig_module_targets[i])
        build.run(self)

class CustomInstall(install):
    def run(self):
        self.run_command('build_ext')
        for i in range(len(swig_modules)):
            if path.exists(swig_module_targets[i]):
                os.remove(swig_module_targets[i])
            if path.exists(swig_module_files[i]):
                os.rename(swig_module_files[i],swig_module_targets[i])
        self.do_egg_install()

# Extend "clean" to remove swig wrappers
class CustomClean(clean):
    def run(self):
        for t in swig_module_targets:
            if path.exists(t):
                os.remove(t)
        for f in swig_wrap_files:
            if path.exists(f):
                os.remove(f)
        if path.exists(path.join(here,'dist')):
            rmtree(path.join(here,'dist'))
        if path.exists(path.join(here,'pysimulationio.egg-info')):
            rmtree(path.join(here,'pysimulationio.egg-info'))
        clean.run(self)

# The standard setup declarations
setup(
    name = 'pysimulationio',
    version = version,
    description = 'Python wrappers for SimulationIO',
    long_description = long_description,
    url = 'https://github.com/eschnett/SimulationIO',
    author = 'Erik Schnetter',
    author_email = 'eschnetter@perimeterinstitute.ca',
    licence = 'LGPLv3+',
    classifiers = [
        'Development status :: 4 - Beta',
        'Intended Audience :: Science/Research',
        'Environment :: Console',
        'License :: GNU Lesser General Public License v3+',
        'Natural Language :: English',
        'Operating System :: Unix',
        'Programming Language :: C++',
        'Programming Language :: Python :: 2',
        'Programming Language :: Python :: 2.6',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3.3',
        'Programming Language :: Python :: 3.4',
        'Programming Language :: Python :: 3.5',
    ],
    keywords = 'HPC simulations relativity science computing',
    cmdclass = {'build' : CustomBuild,
                'install' : CustomInstall,
                'clean' : CustomClean},
    packages = find_packages(),
    package_data = {'' : ['*.o','LICENCE']},
    include_package_data = True,
    ext_package = 'pysimulationio',
    ext_modules = [Extension('_H5',
                             ['H5.i'],
                             swig_opts = swig_opts,
                             include_dirs = includes,
                             library_dirs = links,
                             runtime_library_dirs = links,
                             extra_link_args = link_args,
                             extra_compile_args = flags,
                             extra_objects = object_files,
                             libraries = libs),      
                   Extension('_RegionCalculus',
                             ['RegionCalculus.i'],
                             swig_opts = swig_opts,
                             include_dirs = includes,
                             library_dirs = links,
                             runtime_library_dirs = links,
                             extra_link_args = link_args,
                             extra_compile_args = flags,
                             extra_objects = object_files,
                             libraries = libs),
                   Extension('_SimulationIO',
                             ['SimulationIO.i'],
                             swig_opts = swig_opts,
                             include_dirs = includes,
                             library_dirs = links,
                             runtime_library_dirs = links,
                             extra_link_args = link_args,
                             extra_compile_args = flags,
                             extra_objects = object_files,
                             libraries = libs)],
)
