#include <pthread.h>
#include <sched.h>

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

struct StartGate {
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t condition = PTHREAD_COND_INITIALIZER;
    std::size_t ready{0};
    std::size_t total{0};
    bool open{false};
};

struct Work {
    StartGate* gate{nullptr};
    pthread_mutex_t* job_mutex{nullptr};
    std::size_t* next_job{nullptr};
    std::size_t job_count{0};
    long long subtotal{0};
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

long long jobValue(std::size_t job) {
    return static_cast<long long>(job % 97 + 1);
}

void* processJobs(void* argument) {
    auto& work = *static_cast<Work*>(argument);
    waitForStart(*work.gate);
    for (;;) {
        std::size_t job = 0;
        if (work.job_mutex != nullptr) {
            pthread_mutex_lock(work.job_mutex);
            job = *work.next_job;
            ++*work.next_job;
            pthread_mutex_unlock(work.job_mutex);
        } else {
            job = *work.next_job;
            if (job % 16 == 0) {
                sched_yield();
            }
            *work.next_job = job + 1;
        }
        if (job >= work.job_count) {
            break;
        }
        work.subtotal += jobValue(job);
    }
    return nullptr;
}

long long sequential(std::size_t total_operations) {
    long long total = 0;
    for (std::size_t job = 0; job < total_operations; ++job) {
        total += jobValue(job);
    }
    return total;
}

long long runParallel(
    std::size_t total_operations,
    std::size_t thread_count,
    pthread_mutex_t* job_mutex
) {
    std::size_t next_job = 0;
    StartGate gate;
    gate.total = thread_count;
    std::vector<pthread_t> threads(thread_count);
    std::vector<Work> work(thread_count);

    for (std::size_t index = 0; index < thread_count; ++index) {
        work[index] = {&gate, job_mutex, &next_job, total_operations, 0};
        if (pthread_create(&threads[index], nullptr, processJobs, &work[index]) != 0) {
            std::exit(1);
        }
    }
    for (pthread_t thread : threads) {
        pthread_join(thread, nullptr);
    }
    pthread_cond_destroy(&gate.condition);
    pthread_mutex_destroy(&gate.mutex);

    long long total = 0;
    for (const Work& item : work) {
        total += item.subtotal;
    }
    return total;
}

long long unsafeParallel(std::size_t total_operations, std::size_t thread_count) {
    return runParallel(total_operations, thread_count, nullptr);
}

long long threadSafeParallel(std::size_t total_operations, std::size_t thread_count) {
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    const long long value = runParallel(total_operations, thread_count, &mutex);
    pthread_mutex_destroy(&mutex);
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
