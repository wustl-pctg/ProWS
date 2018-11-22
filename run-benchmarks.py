#!/usr/bin/python

import sys
import argparse
import subprocess
import benchargs
import math
import calendar
import time

search_dirs = "ferret future-bench futurerd-bench"

def write_grouped_data(groups):
    output = ""
    for g in groups:
        output += groups[g] + '\n\n'

    with open('all_results.csv', 'w') as f:
        f.write(output)

def get_nruns_str(bench, nruns):
    if "hw" in bench:
        return ""
    elif "bst" in bench:
        return ""
    elif "ferret" in bench:
        return ""
    elif "fib" in bench:
        return " " + str(nruns)

    return " -nruns " + str(nruns)


def main():
    parser = argparse.ArgumentParser(description='Run benchmarks and output their results to a file.')
    parser.add_argument('benchmarks', metavar='bench', default=[bench for bench in benchargs.bench_args if True], type=str, nargs='*', help='Which benchmarks to run.')
    parser.add_argument('-l', '--list-targets', dest='list_targets', action='store_const', const=True, default=False, help='List all available benchmarks and exit.')

    args = parser.parse_args()

    bench_list = [ bench for bench in benchargs.bench_args if True ]
    # Sort in list order to save bintree for last; it is SLOW
    bench_list.sort(reverse=True)

    if args.list_targets:
        print bench_list
        exit(0)

    bench_groups = dict()
    for bench in bench_list:
        # get the benchmark name
        # (they are named in the format <bench>-<implementation>)
        group_key = bench.split('-')[0] 
        group_key += benchargs.bench_args[bench]['args'].format('<P>', '<P*4>')
        # Create the group's header
        group_val = benchargs.bench_args[bench]['args'].format('<P>', '<P*4>')
        group_val += '\nrunning times (s):,'
        for p in benchargs.core_counts:
            group_val += 'P='+str(p)+' avg time (s),'+'stdev (s),'+'stdev (%),'
        bench_groups[group_key] = group_val

    #print bench_groups
    #exit(0)

    subprocess.call(['cp', '-r', 'bench-results', 'bkup-'+str(int(calendar.timegm(time.gmtime())))])
    subprocess.call(['mkdir', '-p', 'bench-results'])

    for bench in bench_list:
        progname = bench
        while progname[-1] in "0123456789":
            progname = progname[0:-1]

        findCMD = 'dirname $(realpath $(find ' + search_dirs + ' -name "' + progname + '"))'
        location = subprocess.Popen(findCMD, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdout, stderr) = location.communicate()
        location = stdout.decode().strip() + '/' + progname

        group_key = bench.split('-')[0]
        group_key += benchargs.bench_args[bench]['args'].format('<P>', '<P*4>')

        header = benchargs.bench_args[bench]['args'].format('<P>', '<P*4>') + '\nrunning times (s):,'
        data = bench + ','
        
        for ncores in benchargs.core_counts:
            if ('se' in bench):
                ncores = 1

            times = list()
            avg = None
            stdev = None

            nruns = benchargs.bench_args[bench]['runs']
            nruns_args_str = get_nruns_str(bench, nruns)
            if nruns_args_str != "":
                nruns = 1 # It is configurable in the benchmark

            for i in range(0, nruns):
                cmd = "taskset -c 0-" + str(ncores-1) + " " + location + " " + benchargs.bench_args[bench]['args'] + nruns_args_str
                cmd = cmd.format(ncores, ncores*4)
                print cmd
                res = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                (stdout, stderr) = res.communicate()
                print stdout.strip()

                if ("ferret" in bench):
                    times.append(float(stdout.split()[2]))
                elif ("bst" in bench):
                    times.append(float(stdout.split('\n')[2].split()[2]) / 1000.0)
                elif ("hw" in bench):
                    #times.append(float(stdout.split(':')[2].split()[0]) / 1000.0)
                    lines = stdout.split('\n')
                    for l in lines:
                        if 'time' in l:
                            times.append(float(l.split(':')[1].split()[0]) / 1000.0)
                else:
                    lines = stdout.split('\n')
                    for l in lines:
                        if 'average' in l:
                            avg = float(l.split(':')[1].split()[0])
                        elif 'Std. dev' in l:
                            stdev = float(l.split(':')[1].split()[0])
                            break

            if avg is None:
                if (len(times) == 0):
                    print "Skipping ", bench, " due to lack of timing data. Was it configured to run less than once?"
                    continue
                avg = sum(times) / len(times)
                if (len(times) > 1):
                    diff = [x - avg for x in times]
                    sq_diff = [x**2 for x in diff]
                    var = sum(sq_diff) / (len(times) - 1)
                    stdev = math.sqrt(var)

           
            header += 'P='+str(ncores)+' avg time (s),'+'stdev (s),'+'stdev (%),'
            data += str(avg) + ',' + str(stdev)  + ','
            if stdev is None:
                data += str(stdev)
            else:
                data += str((stdev / avg)*100.0) + ','

            if ('se' in bench):
                break

        bench_groups[group_key] = bench_groups[group_key] + '\n' + data
        write_grouped_data(bench_groups)
        with open('bench-results/'+bench+'_timing.csv', 'w') as f:
            f.write(header)
            f.write('\n')
            f.write(data)
            f.write('\n')


if __name__ == "__main__":
    main()
