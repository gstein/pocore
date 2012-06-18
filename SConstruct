# -*- python -*-

import sys
import os

# where we save the configuration variables
SAVED_CONFIG = '.saved_config'

opts = Variables(files=[SAVED_CONFIG])
opts.AddVariables(
  PathVariable('PREFIX',
               'Directory to install under',
               '/usr/local',
               PathVariable.PathIsDir),
  PathVariable('EV',
               "Path to libev's install area, or build area",
               '/usr',
               PathVariable.PathIsDir),
  )

env = Environment(variables=opts,
                  CCFLAGS=['-g', '-O2', '-Wall', ],
                  CPPPATH=['include', ])

BUILDING = not (env.GetOption('clean') or env.GetOption('help'))

Help(opts.GenerateHelpText(env))
opts.Save(SAVED_CONFIG, env)

# Bring in libev.
### SCons probably has a good way to look for a library, and one that
### doesn't have to test the BUILDING flag. (we check whether we're
### truly building so that we don't error out when libev.a is not found).
### we may want to look in the standard system areas and adjust the
### (saved) EV variable, or simply set it to None or something to say
### "libev is normally installed; nothing special is needed".
if BUILDING:
  libev_dir = str(env['EV'])
  if os.path.isfile(os.path.join(libev_dir, '.libs', 'libev.a')):
    # Pointing to the libev build area. Grab libev.a from its .libs/ subdir.
    env.Append(LIBPATH=os.path.join(libev_dir, '.libs'),
               LIBS='ev',
               CPPPATH=libev_dir)
  else:
    if os.path.isfile(os.path.join(libev_dir, 'lib', 'libev.a')):
      env.Append(LIBPATH=os.path.join(libev_dir, 'lib'),
                 LIBS='ev',
                 CPPPATH=os.path.join(libev_dir, 'include'))
    else:
      ### does SCons have a better way to issue error messages?
      print 'ERROR: libev.a not found'
      Exit(1)


thisdir = os.getcwd()

SOURCES = Glob('src/*.c')
if sys.platform.startswith('linux'):
  SOURCES.append('src/platform/linux.c')

lib_static = env.StaticLibrary('libpc-0', SOURCES)
lib_shared = env.SharedLibrary('libpc-0', SOURCES)
env.Default(lib_static, lib_shared)

if not (env.GetOption('clean') or env.GetOption('help')):
  conf = Configure(env)

  # FreeBSD puts the ceil() funcion into -lm. This function is used by
  # libev (but libev has no way to tell us to haul in -lm). If we don't
  # see ceil() in the default build, then look in -lm.
  if not conf.CheckFunc('ceil'):
    conf.CheckLib(['m'], 'ceil')

  env = conf.Finish()


# TESTS

tenv = env.Clone()
tenv.Prepend(LIBS=['libpc-0', ],
             LIBPATH=[thisdir, ]
             )

TEST_PROGRAMS = [
  'tests/test_lookup.c',
  'tests/test_pools.c',
  'tests/test_wget.c',
]
if sys.platform == 'darwin':
  TEST_PROGRAMS.append('tests/test_memtree.c')

for proggie in TEST_PROGRAMS:
  tenv.Program(proggie)

# One program to run all the test suites.
tenv.Program('tests/suite',
             ['tests/ctest/main.c', ] + Glob('tests/suite_*.c'))

env.AlwaysBuild(env.Alias('check', 'tests/suite', 'tests/suite'))
