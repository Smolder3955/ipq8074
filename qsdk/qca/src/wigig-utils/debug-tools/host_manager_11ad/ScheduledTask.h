/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _SCHEDULED_TASK_H
#define _SCHEDULED_TASK_H
#pragma once

#include <functional>
#include <chrono>


typedef std::function<void(void)> ScheduledTask;

class ScheduledTaskRecord final
{
public:

    ScheduledTaskRecord(const ScheduledTask& task, std::chrono::milliseconds pollingInterval, bool isPeriodic, unsigned id)
        : m_task(task)
        , m_id(id)
        , m_pollingInterval(pollingInterval)
        , m_isPeriodic(isPeriodic)
    {
         Reschedule();
    }

    ScheduledTaskRecord(const ScheduledTaskRecord& task) = delete;
    ScheduledTaskRecord& operator=(const ScheduledTaskRecord& task) = delete;

    void RunTask()
    {
        Reschedule(); // set the time for the new iteration to begin in
                      //(should be before running task in order to comply with a strict polling interval)
        m_task(); // Run the function of the task
    }
    unsigned Id() const { return m_id; }
    std::chrono::system_clock::time_point GetExpireTimePoint() const { return m_expireRunTimePoint; }
    bool IsPeriodic() const { return m_isPeriodic; }

    void SetPollingInterval(std::chrono::milliseconds pollingIntervalMs) { m_pollingInterval = pollingIntervalMs; SetExpire(); }
    void SetIsPeriodic(bool isPeriodic) { m_isPeriodic = isPeriodic; }

private:

    void SetExpire() { m_expireRunTimePoint = m_lastRunTimePoint + m_pollingInterval; }
    void Reschedule() { m_lastRunTimePoint = std::chrono::system_clock::now(); SetExpire();
    //std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(m_expireRunTimePoint - m_lastRunTimePoint).count() << std::endl;
    }

    const ScheduledTask m_task;
    const unsigned m_id;
    std::chrono::milliseconds m_pollingInterval;
    bool m_isPeriodic;
    std::chrono::system_clock::time_point m_lastRunTimePoint;
    std::chrono::system_clock::time_point m_expireRunTimePoint;
};
#endif //_SCHEDULED_TASK_H
