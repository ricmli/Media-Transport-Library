# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022 Intel Corporation

project('linux-mtl', 'cpp', default_options: ['buildtype=release'],
        version: run_command(find_program('cat', 'more'), files('../../../VERSION'), check: true).stdout().strip(),)

cppc = meson.get_compiler('cpp')

# deps
libobs = dependency('libobs', required : true)
mtl = dependency('mtl', required : true)
pthread_dep = cppc.find_library('pthread', required : true)
qt5 = import('qt5')
qt5_dep = dependency('qt5', modules: ['Core', 'Gui'])

libobs_libdir = libobs.get_variable(pkgconfig: 'libdir')
libobs_prefix = libobs.get_variable(pkgconfig: 'prefix')

# sources
sources = files(
	'linux-mtl.cpp',
	'mtl-input.cpp',
  'mtl-output.cpp',
)

# args
linux_mtl_cpp_args = []
linux_mtl_cpp_args += ['-Wno-stringop-truncation']

# build library
shared_library(meson.project_name(), sources,
  dependencies: [libobs, mtl, pthread_dep, qt5_dep],
  cpp_args : linux_mtl_cpp_args,
  install: true,
  install_dir : libobs_libdir + '/obs-plugins/'
)

# copy locale files
install_subdir('data/locale', install_dir : libobs_prefix + '/share/obs/obs-plugins/linux-mtl/')
