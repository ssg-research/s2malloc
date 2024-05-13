# S2malloc: Statistically Secure Allocator for Use-After-Free Protection And More


## Build Memory Allocator Only

```
mkdir build && cd build
cmake -DGLIBC_OVERRIDE=ON -DDEBUG=OFF ..
make 
```

## Build PARSEC Test Environment

### If you are using an aarch machine
Please comment out everywhere using `Guarder`
* `Dockerfile.parsec`
* `monitor/parsec_run.py`

You also need to comment out everywhere relevant to `raytrace (ray)` 
test in `monitor/parsec_run.py`.

You can build the tests and all memory allocators:
```
docker build -t parsec -f Dockerfile.parsec .
```

You can run the tests:
```
docker run --name parsec -dt parsec .
docker exec -it parsec /bin/bash 
cd /app/flmalloc/monitor
python3 parsec_run.py {N_THREAD}
```

## Build SPEC Test Environment

***You need to obtain the SPEC benchmark first***

***Replace line 86 of Dockerfile.spec with a link to your spec copy***


### If you are using an aarch machine
Please comment out everywhere using `Guarder`
* `Dockerfile.spec`
* `monitor/spec_run.py`

You can build the tests and all memory allocators:
```
docker build -t parsec -f Dockerfile.spec .
```

You can run the tests:
```
docker run --name spec -dt spec .
docker exec -it spec /bin/bash 
cd /app/flmalloc/monitor
python3 spec_run.py 
```