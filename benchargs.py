
# Update this list to change on how many cores
# each program is run. This is a global setting.
#core_counts = [16]
core_counts = [ 1, 2, 4, 8, 16 ]

"""
NOTE: For EVERY argument string, the first {} in the format string is ALWAYS
      replaced with the number of processors (threads) being used, P. Likewise,
      For EVERY argument string, the second {} in the format string is ALWAYS
      replaced with the P * 4. This is because it was easier than making special
      cases in the python code. This only really affects hw-* and ferret-*

NOTE: For EVERY application except hw-*, ferret-*, and bst-*, the number of times
      the application should run is appended to the arguments list in the appropriate
      format (see benchmarks_list.txt for details on what is appropriate for each).
      The binaries that do not have it appended are simply run 'runs' number of times.

NOTE: Any benchmark that has 'se' in the name will be run on ONE core, and will not
      run more than `runs` times [as opposed to `len(core_counts)*runs` times].
"""

# A map of the benchmark to run to its arguments and
# the number of times the benchmark should be run.
# See the NOTE sections above for important details.
#
# The same benchmark can be run with different arguments
# in the same execution of the script by adding another
# entry with a number appended to the binary name.
# For an example, see the lcs-*2 entries in the map.
bench_args = {
  'mm-se' : {'args': '-n 4096', 'runs': 10},
  'mm-fj' : {'args': '-n 4096', 'runs': 10},
  'mm-sf' : {'args': '-n 4096', 'runs': 10},
  'smm-se' : {'args': '-n 4096', 'runs': 10},
  'smm-fj' : {'args': '-n 4096', 'runs': 10},
  'smm-sf' : {'args': '-n 4096', 'runs': 10},
  'sort-se' : {'args': '-n 100000000', 'runs': 10},
  'sort-fj' : {'args': '-n 100000000', 'runs': 10},
  'sort-sf' : {'args': '-n 100000000', 'runs': 10},
  'hw-se' : {'args': '$(find futurerd-bench -name test.avi) 104 {}', 'runs': 10},
  'hw-fj' : {'args': '$(find futurerd-bench -name test.avi) 104 {}', 'runs': 10},
  'hw-sf' : {'args': '$(find futurerd-bench -name test.avi) 104 {}', 'runs': 10},
  'hw-gf' : {'args': '$(find futurerd-bench -name test.avi) 104 {}', 'runs': 10},
  'lcs-se' : {'args': '-n 32768 -b 512', 'runs': 10},
  'lcs-fj' : {'args': '-n 32768 -b 512', 'runs': 10},
  'lcs-sf' : {'args': '-n 32768 -b 512', 'runs': 10},
  'lcs-gf' : {'args': '-n 32768 -b 512', 'runs': 10},
  'lcs-se2' : {'args': '-n 32768 -b 1024', 'runs': 10},
  'lcs-fj2' : {'args': '-n 32768 -b 1024', 'runs': 10},
  'lcs-sf2' : {'args': '-n 32768 -b 1024', 'runs': 10},
  'lcs-gf2' : {'args': '-n 32768 -b 1024', 'runs': 10},
  'sw-se' : {'args': '-n 2048 -b 32', 'runs': 10},
  'sw-fj' : {'args': '-n 2048 -b 32', 'runs': 10},
  'sw-sf' : {'args': '-n 2048 -b 32', 'runs': 10},
  'sw-gf' : {'args': '-n 2048 -b 32', 'runs': 10},
  'bst-se' : {'args': '-s1 8000000 -s2 4000000', 'runs': 5},
  'bst-fj' : {'args': '-s1 8000000 -s2 4000000', 'runs': 5},
  'bst-gf' : {'args': '-s1 8000000 -s2 4000000', 'runs': 5},
  'ferret-serial' : {'args': './ferret/data/native/corel/ lsh ./ferret/data/native/queries/ 10 {} fpiper.out {}', 'runs': 10},
  'ferret-piper' : {'args': './ferret/data/native/corel/ lsh ./ferret/data/native/queries/ 10 {} fpiper.out {}', 'runs': 10},
  'ferret-cilk-future' : {'args': './ferret/data/native/corel/ lsh ./ferret/data/native/queries/ 10 {} fcilk.out {}', 'runs': 10},
  'fib-se' : {'args': '42', 'runs': 10},
  'fib-fj' : {'args': '42', 'runs': 10},
  'fib-sf' : {'args': '42', 'runs': 10},
  'fib-sf-stack' : {'args': '42', 'runs': 10},
}
