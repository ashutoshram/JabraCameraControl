#!/usr/bin/python
#
# python-v4l2capture
#
# 2009, 2010, 2011 Fredrik Portstrom
#
# I, the copyright holder of this file, hereby release it into the
# public domain. This applies worldwide. In case this is not legally
# possible: I grant anyone the right to use this work for any
# purpose, without any conditions, unless such conditions are
# required by law.
import platform

from distutils.core import Extension, setup
import numpy as np

import os
os.environ['LDFLAGS'] = '-framework CoreFoundation -framework IOKit'

compile_extra_args = []
link_extra_args = []

if platform.system() == "Windows":
    compile_extra_args = ["/std:c++latest", "/EHsc"]
elif platform.system() == "Darwin":
    compile_extra_args = ["-O3", "-mmacosx-version-min=10.9", "-std=c++11", "-stdlib=libc++", "-Wno-c++11-compat-deprecated-writable-strings"]
    #link_extra_args = ["-stdlib=libc++", "-mmacosx-version-min=10.9",  "-lpthread",  "-framework CoreFoundation",  "-framework IOKit"]
    link_extra_args = ["-stdlib=libc++", "-mmacosx-version-min=10.9",  "-lpthread" ]

compile_extra_args.append("-I"+np.get_include())

setup(
    name = "JabraCamera",
    version = "1.0",
    author = "Ashutosh Ram",
    author_email = "aram@jabra.com",
    description = "Jabra Camera API",
    long_description = "Control Jabra Camera Properties with Python",
    license = "Public Domain",
    classifiers = [
        "License :: Public Domain",
        "Programming Language :: C++"],
    ext_modules = [
        Extension("jabracamera", ["MacCameraDevice.cpp", "JabraCameraPyWrapper.cpp"], 
            extra_compile_args = compile_extra_args,
            extra_link_args = link_extra_args)])
