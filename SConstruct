# -*- python -*-

import sys
import os

opts = Variables()
opts.Add(PathVariable('PREFIX',
                      'Directory to install under',
                      '/usr/local',
                      PathVariable.PathIsDir))

### do some searching or a param or something. future.
LIBEV_DIR = '../../libev-4.04'

env = Environment(variables=opts,
                  CCFLAGS=['-g', '-O2', '-Wall', ],
                  CPPPATH=['include', LIBEV_DIR, ])

Help(opts.GenerateHelpText(env))

# Bring in libev.
### sigh. libtool.
env.Append(LIBPATH=os.path.join(LIBEV_DIR, '.libs'), LIBS='ev')

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

TEST_PROGRAMS = [
  'tests/test_hash.c',
  'tests/test_lookup.c',
  'tests/test_pools.c',
  'tests/test_wget.c',
]
if sys.platform == 'darwin':
  TEST_PROGRAMS.append('tests/test_memtree.c')

for proggie in TEST_PROGRAMS:
  tenv.Program(proggie,
               LIBS=['libpc-0', 'libev', ],
               LIBPATH=['.', os.path.join(LIBEV_DIR, '.libs'), ])
