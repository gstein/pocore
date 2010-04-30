#!/usr/bin/python

COUNT = 500

import sys
import subprocess

def runseries(progname):
  total = 0.0
  for i in range(COUNT):
    p = subprocess.Popen([progname], stdout=subprocess.PIPE)
    stdout, unused = p.communicate()
    p.wait()

    # parse output that looks like: elapsed=18179.211 usec
    value = stdout[8:stdout.index(' ')]
    total += float(value)

  print COUNT, 'runs. average: %.03f' % (total/COUNT), 'usec'


if __name__ == '__main__':
  runseries(sys.argv[1])
