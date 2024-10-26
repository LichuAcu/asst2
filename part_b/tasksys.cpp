#include "tasksys.h"
#include <thread>
#include <iostream>
#include <sstream>

IRunnable::~IRunnable() {}

ITaskSystem::ITaskSystem(int num_threads) {}
ITaskSystem::~ITaskSystem() {}

/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */

const char *TaskSystemParallelThreadPoolSleeping::name()
{
    return "Parallel + Thread Pool + Sleep";
}

bool TaskSystemParallelThreadPoolSleeping::bulk_is_ready(Bulk *bulk)
{
    bool is_ready = true;
    for (int dep : bulk->dependencies)
    {
        bool finished = bulks[dep].finished;
        if (!finished)
        {
            is_ready = false;
            break;
        }
    }
    return is_ready;
}

void TaskSystemParallelThreadPoolSleeping::thread_func()
{
    while (!terminate)
    {
        Task task;
        bool has_task = false;

        {
            std::lock_guard<std::mutex> lock(ready_mutex);
            if (!ready.empty())
            {
                task = ready.front();
                ready.pop();
                has_task = true;
                std::stringstream ss;
                ss << "Thread " << std::this_thread::get_id() << " picked up task " << task.task_id << " of bulk " << task.bulk_id << "\n";
                std::cout << ss.str();
            }
        }

        if (has_task)
        {
            std::stringstream ss;
            ss << "Thread " << std::this_thread::get_id() << " executing task " << task.task_id << " of bulk " << task.bulk_id << "\n";
            std::cout << ss.str();
            task.runnable->runTask(task.task_id, task.num_total_tasks);
            Bulk &bulk = bulks[task.bulk_id];
            {
                std::stringstream ss_lock;
                ss_lock << "Thread " << std::this_thread::get_id() << " locking bulk " << task.bulk_id << "\n";
                std::cout << ss_lock.str();
                std::stringstream ss_size;
                ss_size << "Size of bulk_mutexes: " << bulk_mutexes.size() << "\n";
                std::cout << ss_size.str();
                std::lock_guard<std::mutex> lock(*bulk_mutexes[task.bulk_id]);
                std::stringstream ss_lock_after;
                ss_lock_after << "Thread " << std::this_thread::get_id() << " acquired lock for bulk " << task.bulk_id << "\n";
                std::cout << ss_lock_after.str();
                bulk.num_finished_tasks++;
                std::stringstream ss;
                ss << "Bulk " << task.bulk_id << ": " << bulk.num_finished_tasks << "/" << bulk.num_total_tasks << " tasks completed\n";
                std::cout << ss.str();
                if (bulk.num_finished_tasks == bulk.num_total_tasks)
                {
                    bulk.finished = true;
                    std::stringstream ss;
                    ss << "Bulk " << task.bulk_id << " finished";
                    std::cout << ss.str() << std::endl;
                    bulk_finished_cv.notify_all();
                }
            }
        }

        // check if any of the waiting bulks are now ready
        {
            std::lock_guard<std::mutex> lock(waiting_mutex);
            for (auto it = waiting.begin(); it != waiting.end();)
            {
                Bulk *waiting_bulk = *it;
                if (bulk_is_ready(waiting_bulk))
                {
                    std::stringstream ss;
                    ss << "Bulk " << waiting_bulk->bulk_id << " is now ready\n";
                    std::cout << ss.str();
                    IRunnable *runnable = waiting_bulk->runnable;
                    int num_total_tasks = waiting_bulk->num_total_tasks;
                    for (int i = 0; i < num_total_tasks; i++)
                    {
                        ready.push(Task{runnable, i, num_total_tasks, waiting_bulk->bulk_id});
                    }
                    it = waiting.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
    }
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads) : ITaskSystem(num_threads)
{
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    bulks = std::vector<Bulk>();
    for (int i = 0; i < num_threads; i++)
    {
        thread_pool.push_back(std::thread(&TaskSystemParallelThreadPoolSleeping::thread_func, this));
    }
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping()
{
    //
    // TODO: CS149 student implementations may decide to perform cleanup
    // operations (such as thread pool shutdown construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    terminate = true;
    for (std::thread &thread : thread_pool)
    {
        thread.join();
    }
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable *runnable, int num_total_tasks)
{
    // Implement run() using runAsyncWithDeps() and sync()
    std::vector<TaskID> empty_deps;
    runAsyncWithDeps(runnable, num_total_tasks, empty_deps);
    sync();
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                                                              const std::vector<TaskID> &deps)
{
    std::lock_guard<std::mutex> lock(waiting_mutex);
    int bulk_id = bulks.size();
    Bulk bulk{runnable, deps, num_total_tasks, 0, bulk_id, false};
    bulk_mutexes.push_back(std::unique_ptr<std::mutex>(new std::mutex()));
    bulks.push_back(bulk);
    waiting.insert(&bulks.back());

    std::stringstream ss;
    ss << "Created bulk task " << bulk_id << " with " << num_total_tasks << " tasks and " << deps.size() << " dependencies. Dependencies: ";
    for (const auto &dep : deps)
    {
        ss << dep << " ";
    }
    ss << "\n";
    std::cout << ss.str();

    return bulk_id;
}

void TaskSystemParallelThreadPoolSleeping::sync()
{
    std::stringstream ss;
    ss << "Syncing...";
    std::cout << ss.str() << std::endl;
    std::unique_lock<std::mutex> lock(waiting_mutex);
    bulk_finished_cv.wait(lock, [this]()
                          { 
                              bool all_done = waiting.empty() && ready.empty();
                              std::stringstream ss;
                              ss << "Sync check: waiting size = " << waiting.size() << ", ready size = " << ready.size() << "\n";
                              std::cout << ss.str();
                              return all_done; });

    std::stringstream ss2;
    ss2 << "Sync complete";
    std::cout << ss2.str() << std::endl;
}

/*
 * ================================================================
 * Serial task system implementation
 * ================================================================
 */

const char *TaskSystemSerial::name()
{
    return "Serial";
}

TaskSystemSerial::TaskSystemSerial(int num_threads) : ITaskSystem(num_threads)
{
}

TaskSystemSerial::~TaskSystemSerial() {}

void TaskSystemSerial::run(IRunnable *runnable, int num_total_tasks)
{
    for (int i = 0; i < num_total_tasks; i++)
    {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemSerial::runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                                          const std::vector<TaskID> &deps)
{
    for (int i = 0; i < num_total_tasks; i++)
    {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemSerial::sync()
{
    return;
}

/*
 * ================================================================
 * Parallel Task System Implementation
 * ================================================================
 */

const char *TaskSystemParallelSpawn::name()
{
    return "Parallel + Always Spawn";
}

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads) : ITaskSystem(num_threads)
{
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable *runnable, int num_total_tasks)
{
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    for (int i = 0; i < num_total_tasks; i++)
    {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                                                 const std::vector<TaskID> &deps)
{
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    for (int i = 0; i < num_total_tasks; i++)
    {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemParallelSpawn::sync()
{
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Spinning Task System Implementation
 * ================================================================
 */

const char *TaskSystemParallelThreadPoolSpinning::name()
{
    return "Parallel + Thread Pool + Spin";
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads) : ITaskSystem(num_threads)
{
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable *runnable, int num_total_tasks)
{
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
    for (int i = 0; i < num_total_tasks; i++)
    {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                                                              const std::vector<TaskID> &deps)
{
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
    for (int i = 0; i < num_total_tasks; i++)
    {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync()
{
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
    return;
}
