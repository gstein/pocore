
env = Environment(CCFLAGS=['-g', '-O2', '-Wall', '-Wno-strict-aliasing'],
                  CPPPATH=['include', '../libev-4.04'])

env.Library('libpc-0', Glob('src/*.c'))

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
  'tests/test_memtree.c',
  'tests/test_pools.c',
  'tests/test_wget.c',
]

env.Prepend(LIBS=['libpc-0', 'libev'],
            LIBPATH=['.'])

for proggie in TEST_PROGRAMS:
  env.Program(proggie)
