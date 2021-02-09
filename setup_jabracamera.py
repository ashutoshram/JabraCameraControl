#!/usr/bin/python
import platform

from distutils.core import Extension, setup
from distutils import unixccompiler

import numpy as np

import os

compile_extra_args = []
link_extra_args = []

if platform.system() == "Windows":
    compile_extra_args = ["/std:c++latest", "/EHsc"]
elif platform.system() == "Darwin":
    if not '.mm' in unixccompiler.UnixCCompiler.src_extensions:
        print('appending .mm to src_extensions')
        unixccompiler.UnixCCompiler.src_extensions.append('.mm')
    compile_extra_args = ["-O3", "-mmacosx-version-min=10.9", "-std=c++11", "-stdlib=libc++", "-Wno-c++11-compat-deprecated-writable-strings", "-I%s" % os.getcwd(), "-I%s/Mac" % os.getcwd()]
    #link_extra_args = ["-stdlib=libc++", "-mmacosx-version-min=10.9",  "-lpthread",  "-framework CoreFoundation",  "-framework IOKit"]
    link_extra_args = ["-stdlib=libc++", "-mmacosx-version-min=10.9",  "-lpthread" ]
    os.environ['LDFLAGS'] = '-framework CoreFoundation -framework IOKit -framework AVFoundation -framework CoreMedia -framework CoreVideo -framework Foundation'

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
        Extension("jabracamera", ["Mac/MacCameraDevice.cpp", "JabraCameraPyWrapper.cpp", "utils.cpp", "Mac/AVFoundationCapture.mm", "Mac/MacFrameCapture.mm"], 
            extra_compile_args = compile_extra_args,
            extra_link_args = link_extra_args)])
