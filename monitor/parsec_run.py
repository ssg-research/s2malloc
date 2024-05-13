import subprocess
import os
from datetime import datetime
import time
import sys


def profile(nthreads='5'):

    tests = [
        'blackscholes',
        'bodytrack',
        'canneal',
        'dedup',
        'facesim',
        'ferret',
        'fluidanimate',
        'freqmine',
        'raytrace',
        'streamcluster',
        'swaptions',
        'vips',
        # 'x264'
    ]

    cmds = {
        'blackscholes': '/app/parsec-benchmark/pkgs/apps/blackscholes/inst/amd64-linux.gcc/bin/blackscholes {nthreads} in_10M.txt prices.txt'.format(nthreads=nthreads),
        'bodytrack': '/app/parsec-benchmark/pkgs/apps/bodytrack/inst/amd64-linux.gcc/bin/bodytrack sequenceB_261 4 261 4000 5 0 {nthreads}'.format(nthreads=nthreads),
        'canneal': '/app/parsec-benchmark/pkgs/kernels/canneal/inst/amd64-linux.gcc/bin/canneal {nthreads} 15000 2000 2500000.nets 6000'.format(nthreads=nthreads),
        'dedup': '/app/parsec-benchmark/pkgs/kernels/dedup/inst/amd64-linux.gcc/bin/dedup -c -p -v -t {nthreads} -i FC-6-x86_64-disc1.iso -o output.dat.ddp'.format(nthreads=nthreads),
        'facesim': '/app/parsec-benchmark/pkgs/apps/facesim/inst/amd64-linux.gcc/bin/facesim -timing -threads {nthreads} -lastframe 100'.format(nthreads=nthreads),
        'ferret': '/app/parsec-benchmark/pkgs/apps/ferret/inst/amd64-linux.gcc/bin/ferret corel lsh queries 50 20 {nthreads} output.txt'.format(nthreads=nthreads),
        'fluidanimate': '/app/parsec-benchmark/pkgs/apps/fluidanimate/inst/amd64-linux.gcc/bin/fluidanimate {nthreads} 500 in_500K.fluid out.fluid'.format(nthreads=nthreads),
        'freqmine': '/app/parsec-benchmark/pkgs/apps/freqmine/inst/amd64-linux.gcc/bin/freqmine webdocs_250k.dat 11000',
        'raytrace': '/app/parsec-benchmark/pkgs/apps/raytrace/inst/amd64-linux.gcc/bin/rtview thai_statue.obj -automove -nthreads {nthreads} -frames 200 -res 1920 1080'.format(nthreads=nthreads),
        'streamcluster': '/app/parsec-benchmark/pkgs/kernels/streamcluster/inst/amd64-linux.gcc/bin/streamcluster 10 20 128 1000000 200000 5000 none output.txt {nthreads}'.format(nthreads=nthreads),
        'swaptions': '/app/parsec-benchmark/pkgs/apps/swaptions/inst/amd64-linux.gcc/bin/swaptions -ns 128 -sm 1000000 -nt {nthreads}'.format(nthreads=nthreads),
        'vips': '/app/parsec-benchmark/pkgs/apps/vips/inst/amd64-linux.gcc/bin/vips im_benchmark orion_18000x18000.v output.v',
    }

    env_base = os.environ.copy()

    env_freqmine = env_base.copy()
    env_freqmine['OMP_NUM_THREADS'] = nthreads

    env_vips = env_base.copy()
    env_vips['IM_CONCURRENCY'] = nthreads

    envs = {
        'blackscholes': env_base,
        'bodytrack': env_base,
        'canneal': env_base,
        'dedup': env_base,
        'facesim': env_base,
        'ferret': env_base,
        'fluidanimate': env_base,
        'freqmine': env_freqmine,
        'raytrace': env_base,
        'streamcluster': env_base,
        'swaptions': env_base,
        'vips': env_vips,
    }

    submit = '/usr/bin/time -v '

    preloads = {
        'glibc': '  ',
        'guarder': '/app/unialloc/lib/libguarder.so ',
        'flmalloc': '/app/unialloc/lib/libflmalloc.so ',
        'slimguard': '/app/unialloc/lib/libSlimGuard.so ',
        'dieharder': '/app/unialloc/lib/libdieharder.so '
    }

    now = datetime.now()
    output_base = '/app/unialloc/output/'
    cur_time = now.strftime("%m-%d-%H-%M-%S")

    run_dir = '/app/unialloc/monitor/parsecio/'

    try:
        os.makedirs(output_base + cur_time)
    except FileExistsError:
        pass

    for test in tests:
        for preload in preloads.keys():
            cmd = 'LD_PRELOAD=' + preloads[preload] + submit + cmds[test]
            print(cmd)
            process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True, cwd=run_dir + test, env=envs[test])
            out, err = process.communicate()
            out = out.decode('utf-8')
            err = err.decode('utf-8')

            output = out + '\n\n*****************************\n\n' + err
            print(output)

            filename = test + '-' + preload + '.log'
            with open(output_base + cur_time + '/' + filename, 'w+') as file:
                file.write(output)

            time.sleep(1)


if __name__ == '__main__':
    for i in range(5):
        profile(sys.argv[1])
