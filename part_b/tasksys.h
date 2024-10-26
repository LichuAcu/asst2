#ifndef _TASKSYS_H
#define _TASKSYS_H

#include "itasksys.h"
#include <condition_variable>
#include <set>
#include <queue>
/*
 * TaskSystemSerial: This class is the student's implementation of a
 * serial task execution engine.  See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemSerial : public ITaskSystem
{
public:
    TaskSystemSerial(int num_threads);
    ~TaskSystemSerial();
    const char *name();
    void run(IRunnable *runnable, int num_total_tasks);
    TaskID runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                            const std::vector<TaskID> &deps);
    void sync();
};

/*
 * TaskSystemParallelSpawn: This class is the student's implementation of a
 * parallel task execution engine that spawns threads in every run()
 * call.  See definition of ITaskSystem in itasksys.h for documentation
 * of the ITaskSystem interface.
 */
class TaskSystemParallelSpawn : public ITaskSystem
{
public:
    TaskSystemParallelSpawn(int num_threads);
    ~TaskSystemParallelSpawn();
    const char *name();
    void run(IRunnable *runnable, int num_total_tasks);
    TaskID runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                            const std::vector<TaskID> &deps);
    void sync();
};

/*
 * TaskSystemParallelThreadPoolSpinning: This class is the student's
 * implementation of a parallel task execution engine that uses a
 * thread pool. See definition of ITaskSystem in itasksys.h for
 * documentation of the ITaskSystem interface.
 */
class TaskSystemParallelThreadPoolSpinning : public ITaskSystem
{
public:
    TaskSystemParallelThreadPoolSpinning(int num_threads);
    ~TaskSystemParallelThreadPoolSpinning();
    const char *name();
    void run(IRunnable *runnable, int num_total_tasks);
    TaskID runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                            const std::vector<TaskID> &deps);
    void sync();
};

struct Task
{
    IRunnable *runnable;
    int task_id;
    int num_total_tasks;
    int bulk_id;
};

struct Bulk
{
    IRunnable *runnable;
    std::vector<int> dependencies;
    int num_total_tasks;
    int num_finished_tasks;
    int bulk_id;
    bool finished;
};

/*
 * TaskSystemParallelThreadPoolSleeping: This class is the student's
 * optimized implementation of a parallel task execution engine that uses
 * a thread pool. See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemParallelThreadPoolSleeping : public ITaskSystem
{
private:
    std::vector<std::thread> thread_pool;
    std::queue<Task> ready; // tasks that are ready to be executed
    std::mutex ready_mutex;
    bool terminate;

    // std::vector<std::vector<int>> dependants; // dependants[i] is the bulks that depend on bulk i
    // std::vector<std::set<int>> dependencies;  // dependencies[i] is the bulks that bulk i depends on

    // std::vector<std::condition_variable> cvs; // cvs[i] is the cv for bulk i
    std::vector<Bulk> bulks;
    std::vector<std::unique_ptr<std::mutex>> bulk_mutexes; // bulk_mutexes[i] is the lock for bulk i
    std::set<Bulk *> waiting;                              // bulks that are waiting for completion of their dependencies
    std::mutex waiting_mutex;                              // waiting_mutex is the lock for the waiting set

    std::condition_variable bulk_finished_cv;

    void thread_func();
    bool bulk_is_ready(Bulk *bulk);

public:
    TaskSystemParallelThreadPoolSleeping(int num_threads);
    ~TaskSystemParallelThreadPoolSleeping();
    const char *name();
    void run(IRunnable *runnable, int num_total_tasks);
    TaskID runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                            const std::vector<TaskID> &deps);
    void sync();
};

#endif
