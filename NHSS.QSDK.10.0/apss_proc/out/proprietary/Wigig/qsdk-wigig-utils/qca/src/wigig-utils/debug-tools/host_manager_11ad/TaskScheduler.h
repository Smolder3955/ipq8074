/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _TASK_SCHEDULER_H
#define _TASK_SCHEDULER_H
#pragma once

#include <ScheduledTask.h>

#include <queue>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <thread>
#include <atomic>

class TaskScheduler final
{

public:

    TaskScheduler() :
        m_nextAvailableId(0U),
        m_continueScheduling(true)
    {}

    ~TaskScheduler() { Stop(); }

    /* Schedule tasks handlers */
    unsigned RegisterTask(const ScheduledTask& task, std::chrono::milliseconds pollingInterval, bool isPeriodic);

    // removes all occurences of action
    bool UnregisterTask(unsigned id);

    bool ChangePollingInterval(unsigned id, std::chrono::milliseconds newPollingInterval);

    bool ChangeIsPeriodic(unsigned id, bool newIsPeriodic);
    /* end of Schedule tasks handlers */

    /* scheduler functions */
    void Start();
    void Stop();
    /* end of scheduler functions */

    // Non-copyable
    TaskScheduler(const TaskScheduler&) = delete;
    TaskScheduler& operator=(const TaskScheduler&) = delete;

private:
    unsigned GenerateId() { return m_nextAvailableId++; }
    void AddTaskRecord(std::shared_ptr<ScheduledTaskRecord> task);
    void UnsafeAddTaskRecord(std::shared_ptr<ScheduledTaskRecord> task);
    std::shared_ptr<ScheduledTaskRecord> PopById(unsigned id);
    std::shared_ptr<ScheduledTaskRecord> UnsafePopById(unsigned id);
    void SchedulerLoop();
    std::shared_ptr<ScheduledTaskRecord> TaskToRun();

    std::deque<std::shared_ptr<ScheduledTaskRecord>> m_tasks;
    unsigned m_nextAvailableId;
    std::mutex m_tasksMutex;
    std::atomic<bool>  m_continueScheduling;
    std::condition_variable m_condition;
    std::thread m_taskSchedulerThread;
};
#endif //_TASK_SCHEDULER_H
