import sys
import os

### do some searching or a param or something. future.
LIBEV_DIR = '../libev-4.04'

env = Environment(CCFLAGS=['-g', '-O2', '-Wall', ],
                  CPPPATH=['include', LIBEV_DIR, ])

SOURCES = Glob('src/*.c')
if sys.platform.startswith('linux'):
  SOURCES.append('src/platform/linux.c')

env.Library('libpc-0', SOURCES)

if not env.GetOption('clean'):
  conf = Configure(env)

  # FreeBSD puts the ceil() funcion into -lm. This function is used by
  # libev (but libev has no way to tell us to haul in -lm). If we don't
  # see ceil() in the default build, then look in -lm.
  if not conf.CheckFunc('ceil'):
    conf.CheckLib(['m'], 'ceil')

  env = conf.Finish()


TEST_PROGRAMS = [
  'tests/test_hash.c',
  'tests/test_lookup.c',
  'tests/test_pools.c',
  'tests/test_wget.c',
]
if sys.platform == 'darwin':
  TEST_PROGRAMS.append('tests/test_memtree.c')

env.Prepend(LIBS=['libpc-0', 'libev', ],
            LIBPATH=['.', os.path.join(LIBEV_DIR, '.libs'), ])

for proggie in TEST_PROGRAMS:
  env.Program(proggie)
