import subprocess
import os
from datetime import datetime
import time


def profile():
    spec_cmd = 'runcpu --config test --action run --tune base '
    submit = '/usr/bin/time -v '

    preloads = {
        'glibc': '  ',
        'guarder': '/app/unialloc/lib/libguarder.so ',
        'flmalloc': '/app/unialloc/lib/libflmalloc.so ',
        'slimguard': '/app/unialloc/lib/libSlimGuard.so ',
        #'dieharder': '/app/unialloc/lib/libdieharder.so '
    }

    tests = [
        '600',
        '602',
        '605',
        '619',
        '620',
        '623',
        '625',
        '631',
        '638',
        '641',
        '644',
        '657'
    ]

    now = datetime.now()
    output_base = '/app/unialloc/output/'
    cur_time = now.strftime("%m-%d-%H-%M")
    try:
        os.makedirs(output_base + cur_time)
    except FileExistsError:
        pass

    for test in tests:
        # build
        process = subprocess.Popen('runcpu --config test --action build --tune base ' + test, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
        process.communicate()

        # run
        for preload in preloads.keys():
            cmd = 'LD_PRELOAD=' + preloads[preload] + submit + spec_cmd + test
            print(cmd)
            process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
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
        profile()
