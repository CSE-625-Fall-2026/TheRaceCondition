#include <pthread.h>
#include <sched.h>

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

constexpr std::size_t bucket_count = 8;

struct StartGate {
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t condition = PTHREAD_COND_INITIALIZER;
    std::size_t ready{0};
    std::size_t total{0};
    bool open{false};
};

struct Work {
    StartGate* gate{nullptr};
    std::array<pthread_mutex_t, bucket_count>* bucket_mutexes{nullptr};
    std::array<long long, bucket_count>* histogram{nullptr};
    std::size_t start{0};
    std::size_t operations{0};
};

bool readPositiveNumber(const char* text, std::size_t& result) {
    errno = 0;
    char* end = nullptr;
    const unsigned long long value = std::strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value == 0 ||
        value > static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max())) {
        return false;
    }
    result = static_cast<std::size_t>(value);
    return true;
}

bool readInputs(
    int argument_count,
    char** arguments,
    std::size_t& total_operations,
    std::size_t& thread_count
) {
    return argument_count == 3 &&
        readPositiveNumber(arguments[1], total_operations) &&
        readPositiveNumber(arguments[2], thread_count) &&
        total_operations <= 1000000000 &&
        thread_count <= 64 &&
        thread_count <= total_operations;
}

std::size_t operationsForThread(
    std::size_t total_operations,
    std::size_t thread_index,
    std::size_t thread_count
) {
    return total_operations / thread_count +
        (thread_index < total_operations % thread_count ? 1 : 0);
}

std::size_t startingOperation(
    std::size_t total_operations,
    std::size_t thread_index,
    std::size_t thread_count
) {
    return thread_index * (total_operations / thread_count) +
        (thread_index < total_operations % thread_count
            ? thread_index
            : total_operations % thread_count);
}

void waitForStart(StartGate& gate) {
    pthread_mutex_lock(&gate.mutex);
    ++gate.ready;
    if (gate.ready == gate.total) {
        gate.open = true;
        pthread_cond_broadcast(&gate.condition);
    }
    while (!gate.open) {
        pthread_cond_wait(&gate.condition, &gate.mutex);
    }
    pthread_mutex_unlock(&gate.mutex);
}

std::size_t bucketFor(std::size_t index) {
    return (index * 17 + index / 11) % bucket_count;
}

void* fillHistogram(void* argument) {
    auto& work = *static_cast<Work*>(argument);
    waitForStart(*work.gate);
    for (std::size_t offset = 0; offset < work.operations; ++offset) {
        const std::size_t bucket = bucketFor(work.start + offset);
        if (work.bucket_mutexes != nullptr) {
            pthread_mutex_lock(&(*work.bucket_mutexes)[bucket]);
        }
        ++(*work.histogram)[bucket];
        if (work.bucket_mutexes != nullptr) {
            pthread_mutex_unlock(&(*work.bucket_mutexes)[bucket]);
        }
        if (offset % 256 == 0) {
            sched_yield();
        }
    }
    return nullptr;
}

long long histogramTotal(const std::array<long long, bucket_count>& histogram) {
    long long result = 0;
    for (long long value : histogram) {
        result += value;
    }
    return result;
}

long long sequential(std::size_t total_operations) {
    std::array<long long, bucket_count> histogram{};
    for (std::size_t index = 0; index < total_operations; ++index) {
        ++histogram[bucketFor(index)];
    }
    return histogramTotal(histogram);
}

long long runParallel(
    std::size_t total_operations,
    std::size_t thread_count,
    std::array<pthread_mutex_t, bucket_count>* bucket_mutexes
) {
    std::array<long long, bucket_count> histogram{};
    StartGate gate;
    gate.total = thread_count;
    std::vector<pthread_t> threads(thread_count);
    std::vector<Work> work(thread_count);

    for (std::size_t index = 0; index < thread_count; ++index) {
        work[index] = {
            &gate,
            bucket_mutexes,
            &histogram,
            startingOperation(total_operations, index, thread_count),
            operationsForThread(total_operations, index, thread_count)
        };
        if (pthread_create(&threads[index], nullptr, fillHistogram, &work[index]) != 0) {
            std::exit(1);
        }
    }
    for (pthread_t thread : threads) {
        pthread_join(thread, nullptr);
    }
    pthread_cond_destroy(&gate.condition);
    pthread_mutex_destroy(&gate.mutex);
    return histogramTotal(histogram);
}

long long unsafeParallel(std::size_t total_operations, std::size_t thread_count) {
    return runParallel(total_operations, thread_count, nullptr);
}

long long threadSafeParallel(std::size_t total_operations, std::size_t thread_count) {
    std::array<pthread_mutex_t, bucket_count> mutexes;
    for (pthread_mutex_t& mutex : mutexes) {
        pthread_mutex_init(&mutex, nullptr);
    }
    const long long value = runParallel(total_operations, thread_count, &mutexes);
    for (pthread_mutex_t& mutex : mutexes) {
        pthread_mutex_destroy(&mutex);
    }
    return value;
}

template <typename Function>
void printTimed(const char* name, Function function) {
    const auto start = std::chrono::steady_clock::now();
    const long long value = function();
    const auto finish = std::chrono::steady_clock::now();
    const std::chrono::duration<double> elapsed = finish - start;
    std::printf("%s value: %lld, time: %.3f seconds\n", name, value, elapsed.count());
}

int main(int argument_count, char** arguments) {
    std::size_t total_operations = 0;
    std::size_t thread_count = 0;
    if (!readInputs(argument_count, arguments, total_operations, thread_count)) {
        std::fprintf(stderr, "usage: %s TOTAL_OPERATIONS THREAD_COUNT\n", arguments[0]);
        return 1;
    }

    printTimed("Sequential", [=] { return sequential(total_operations); });
    printTimed("Unsafe pthreads", [=] {
        return unsafeParallel(total_operations, thread_count);
    });
    printTimed("Thread-safe pthreads", [=] {
        return threadSafeParallel(total_operations, thread_count);
    });
    return 0;
}
