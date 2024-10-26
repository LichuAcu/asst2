#include "tasksys.h"
#include <thread>
#include <iostream>

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
            }
        }

        if (has_task)
        {
            task.runnable->runTask(task.task_id, task.num_total_tasks);
            std::string log_message = "Executing task " + std::to_string(task.task_id) + " of bulk " + std::to_string(task.bulk_id) + "\nTotal bulks: " + std::to_string(bulks.size()) + "\n";
            Bulk &bulk = bulks[task.bulk_id];
            {
                std::lock_guard<std::mutex> lock(bulk_mutexes[task.bulk_id]);
                bulk.num_finished_tasks++;
                if (bulk.num_finished_tasks == bulk.num_total_tasks)
                {
                    bulk.finished = true;
                    bulk_finished_cv.notify_all();
                }
            }
        }

        // check if any of the waiting bulks are now ready
        {
            std::lock_guard<std::mutex> lock(waiting_mutex);
            for (Bulk *waiting_bulk : waiting)
            {
                if (bulk_is_ready(waiting_bulk))
                {
                    IRunnable *runnable = waiting_bulk->runnable;
                    int num_total_tasks = waiting_bulk->num_total_tasks;
                    for (int i = 0; i < num_total_tasks; i++)
                    {
                        ready.push(Task{runnable, i, num_total_tasks, waiting_bulk->bulk_id});
                    }
                    waiting.erase(waiting_bulk);
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
    //
    // TODO: CS149 students will implement this method in Part B.
    //

    std::lock_guard<std::mutex> lock(waiting_mutex);
    Bulk bulk{runnable, deps, num_total_tasks, 0, static_cast<int>(bulks.size()), false};
    bulks.push_back(bulk);
    waiting.insert(&bulks.back());

    // dependencies.push_back(std::set<int>());
    // dependants.push_back(std::vector<int>());

    // for (int dep : deps)
    // {
    // dependants[dep].push_back(bulk_id);
    // dependencies[bulk_id].insert(dep);
    // }

    int bulk_id = bulks.size() - 1;

    // Log the creation of a new bulk task
    std::string log_message = "Created bulk task " + std::to_string(bulk_id) + " with " + std::to_string(num_total_tasks) + " tasks and " + std::to_string(deps.size()) + " dependencies\n";
    std::cout << log_message << std::flush;

    return bulk_id;
}

void TaskSystemParallelThreadPoolSleeping::sync()
{

    //
    // TODO: CS149 students will modify the implementation of this method in Part B.
    //
    std::cout << "Syncing" << std::endl;
    std::unique_lock<std::mutex> lock(waiting_mutex);
    bulk_finished_cv.wait(lock, [this]()
                          { return waiting.empty() && ready.empty(); });

    return;
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
