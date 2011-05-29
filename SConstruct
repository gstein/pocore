
env = Environment(CCFLAGS=['-g', '-O2', '-Wall'],
                  CPPPATH=['include', '../../libev-4.04'])

env.Library('libpc-0', Glob('src/*.c'))

print "Calling Program('Building test programs')"
env.Program('tests/test_hash.c', LIBS=['libpc-0', 'libev'], LIBPATH='.')
env.Program('tests/test_lookup.c', LIBS=['libpc-0', 'libev'], LIBPATH='.')
env.Program('tests/test_memtree.c', LIBS=['libpc-0', 'libev'], LIBPATH='.')
env.Program('tests/test_pools.c', LIBS=['libpc-0', 'libev'], LIBPATH='.')
env.Program('tests/test_wget.c', LIBS=['libpc-0', 'libev'], LIBPATH='.')
