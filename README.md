# TheRaceCondition

This standalone CMake repository demonstrates race conditions and simple
mutex-based fixes with POSIX threads.

Every scenario calls three functions:

- `sequential()` performs the calculation on the calling thread
- `unsafeParallel()` performs the calculation with an intentional race
- `threadSafeParallel()` protects the shared operation with a mutex

Each function receives the same logical workload. Both parallel functions use
the same number of pthreads and divide the intended work evenly.

## Scenarios

### `scenario_1.cpp`

Pthreads increment one shared integer. The unsafe read, increment, and write
loses updates. The fix locks a mutex for each increment.

### `scenario_2.cpp`

Pthreads place values into shared histogram buckets. The unsafe version loses
bucket updates. The fix gives each bucket its own mutex.

### `scenario_3.cpp`

Pthreads update a shared maximum with a check-then-write operation. The unsafe
version can overwrite a larger value after reading an old maximum. The fix
locks the complete check and update.

### `scenario_4.cpp`

Pthreads claim jobs from a shared next-job index. The unsafe version can claim
jobs more than once. The fix locks only the job claim, while job processing
remains parallel.

### `scenario_5.cpp`

Pthreads transfer integer amounts among four shared accounts. The unsafe
version loses account updates and changes the total balance. The fix locks the
complete transfer.

## Build

The default build is Debug so the demonstrations are not optimized.

```sh
cmake -S . -B build
cmake --build build
```

## Run

Each scenario accepts `TOTAL_OPERATIONS` and `THREAD_COUNT`. The operation
count can be at most 1,000,000,000. The thread count can be at most 64 and cannot
exceed the operation count.

This workload takes a few seconds per scenario on the development machine:

```sh
./build/example/scenario_1 100000000 8
./build/example/scenario_2 100000000 8
./build/example/scenario_3 100000000 8
./build/example/scenario_4 100000000 8
./build/example/scenario_5 100000000 8
```

Each run prints three integer results and their wall-clock times:

```text
Sequential value: 100000000, time: 0.051 seconds
Unsafe pthreads value: 35188206, time: 0.133 seconds
Thread-safe pthreads value: 100000000, time: 2.197 seconds
```

## Profile results

These results use 1,000,000,000 operations, 8 pthreads, and the Debug build
without optimization. They are from one run on the development computer.

| Scenario | Sequential value | Unsafe value | Thread-safe value |
|---|---:|---:|---:|
| `scenario_1` | 1,000,000,000 | 265,443,501 | 1,000,000,000 |
| `scenario_2` | 1,000,000,000 | 305,948,989 | 1,000,000,000 |
| `scenario_3` | 1,000,000,000 | 999,969,215 | 1,000,000,000 |
| `scenario_4` | 48,999,998,929 | 295,536,741,497 | 48,999,998,929 |
| `scenario_5` | 4,000,000,000 | 4,017,125,674 | 4,000,000,000 |

| Scenario | Sequential seconds | Unsafe seconds | Thread-safe seconds |
|---|---:|---:|---:|
| `scenario_1` | 0.524 | 0.903 | 21.858 |
| `scenario_2` | 1.624 | 2.170 | 42.332 |
| `scenario_3` | 1.304 | 0.380 | 18.095 |
| `scenario_4` | 1.329 | 36.217 | 18.389 |
| `scenario_5` | 4.072 | 9.004 | 30.555 |

## Correct by chance

The following experiment selected a small workload for each race, ran it 100
times, and compared only the sequential and unsafe results.

| Scenario | Operations | Pthreads | Expected value | Correct | Incorrect |
|---|---:|---:|---:|---:|---:|
| `scenario_1` | 1,000 | 2 | 1,000 | 45 | 55 |
| `scenario_2` | 1,000 | 2 | 1,000 | 16 | 84 |
| `scenario_3` | 100 | 2 | 100 | 3 | 97 |
| `scenario_4` | 100 | 2 | 4,759 | 13 | 87 |
| `scenario_5` | 100 | 8 | 4,000,000,000 | 59 | 41 |

These counts will change between computers and runs. An unsafe result matching
the sequential value does not prove that the program is thread-safe.

Times vary by machine. Increase or decrease `TOTAL_OPERATIONS` as needed.
