/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include <algorithm>
#include <future>

#include "TaskScheduler.h"

using namespace std;

void TaskScheduler::UnsafeAddTaskRecord(shared_ptr<ScheduledTaskRecord> task)
{
    m_tasks.push_back(task);

    std::sort(m_tasks.begin(), m_tasks.end(),
        [](shared_ptr<ScheduledTaskRecord> lhs, shared_ptr<ScheduledTaskRecord> rhs)
    {
        return lhs->GetExpireTimePoint() < rhs->GetExpireTimePoint();
    });

    m_condition.notify_one();
}

void TaskScheduler::AddTaskRecord(shared_ptr<ScheduledTaskRecord> task)
{
    lock_guard<mutex> lock(m_tasksMutex);
    UnsafeAddTaskRecord(task);
}

shared_ptr<ScheduledTaskRecord> TaskScheduler::UnsafePopById(unsigned id)
{
    auto it = std::find_if(m_tasks.begin(), m_tasks.end(), [id](const shared_ptr<ScheduledTaskRecord>& spTask)
    {
        return spTask && (spTask->Id() == id);
    });

    if (it == m_tasks.end())
    {
        return shared_ptr<ScheduledTaskRecord>();
    }

    auto task = *it;
    m_tasks.erase(it);
    return task;
}

std::shared_ptr<ScheduledTaskRecord> TaskScheduler::PopById(unsigned id)
{
    lock_guard<mutex> lock(m_tasksMutex);
    return UnsafePopById(id);
}

unsigned TaskScheduler::RegisterTask(const ScheduledTask& task, std::chrono::milliseconds pollingInterval, bool isPeriodic)
{
    auto id = GenerateId();
    AddTaskRecord(make_shared<ScheduledTaskRecord>(task, pollingInterval, isPeriodic, id));
    return id;
}

bool TaskScheduler::UnregisterTask(unsigned id)
{
    return nullptr != PopById(id);
}

bool TaskScheduler::ChangePollingInterval(unsigned id, std::chrono::milliseconds newPollingInterval)
{
    auto task = PopById(id);
    if (!task)
    {
        return false;
    }

    task->SetPollingInterval(newPollingInterval);
    AddTaskRecord(task);
    return true;
}

bool TaskScheduler::ChangeIsPeriodic(unsigned id, bool newIsPeriodic)
{
    auto task = PopById(id);
    if (!task)
    {
        return false;
    }

    task->SetIsPeriodic(newIsPeriodic);
    AddTaskRecord(task);
    return true;
}

void TaskScheduler::Start()
{
    m_taskSchedulerThread = thread(&TaskScheduler::SchedulerLoop, this);
}

void TaskScheduler::Stop()
{
    m_continueScheduling = false;
    m_condition.notify_one();
    if (m_taskSchedulerThread.joinable())
    {
        m_taskSchedulerThread.join();
    }
}

shared_ptr<ScheduledTaskRecord> TaskScheduler::TaskToRun()
{
    unique_lock<mutex> lock(m_tasksMutex);

    // check if there are tasks to schedule
    m_condition.wait(lock, [&]() { return !m_tasks.empty(); });

    // check if there is a task with expired timeout
    auto now = chrono::system_clock::now();
    auto firstTask = m_tasks.front();
    auto firstExpire = firstTask ? firstTask->GetExpireTimePoint() : now; // protection against empty task, shouldn't happen
    if (firstExpire > now)
    {
        auto napDuration = firstExpire - now;
        m_condition.wait_for(lock, napDuration);
        return shared_ptr<ScheduledTaskRecord>();
    }
    else
    {
        return firstTask;
    }
}

void TaskScheduler::SchedulerLoop()
{
    while (m_continueScheduling)
    {
        auto task = TaskToRun();
        if (task)
        {
            // remove from task list
            PopById(task->Id());
            // asynchronously run the task's function and then add it again to the task list
            std::future<void> result(async(launch::async, [&]() {
                task->RunTask();
                if (task->IsPeriodic())
                {
                    AddTaskRecord(task);
                }
            }));
        }
    }
}
