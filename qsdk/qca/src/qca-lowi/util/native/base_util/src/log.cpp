/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

 Log utility

 GENERAL DESCRIPTION
 This component provides logging service to all client side components

 Copyright (c) 2012 Qualcomm Atheros, Inc.
 All Rights Reserved.
 Qualcomm Atheros Confidential and Proprietary.

 Copyright (c) 2014 - 2016 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
 =============================================================================*/
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <new>

//#include <base_util/log.h>
#include <sync.h>
#include <list.h>
#include <string_routines.h>
#include <time_routines.h>

#ifdef __ANDROID__

// if we're compiling for Android, use android log
// makefile can decide if we want to enable stdout as well
#define USE_ANDROID_LOG 1

#ifndef USE_STDOUT_LOG
// default if this is not defined by makefile
#define USE_STDOUT_LOG 1
#endif // #ifndef USE_STDOUT_LOG
#else //#ifdef __ANDROID__
// if we're not compiling for Android, use only stdout
#define USE_STDOUT_LOG 1
#define USE_ANDROID_LOG 0
#endif // #ifndef __ANDROID__
#if USE_ANDROID_LOG
#define LOG_NDEBUG 0

// these 2 symbols are needed on ICS for DEBUG- and INFO-level
// log messages to be processed. They are not neeeded on JB
#define LOG_NDDEBUG 0
#define LOG_NIDEBUG 0

#define LOG_TAG NULL

#ifdef __ANDROID_NDK__
#include <android/log.h>
#define ANDROID_LOG __android_log_print
#define LOG_VERBOSE ANDROID_LOG_VERBOSE
#define LOG_DEBUG ANDROID_LOG_DEBUG
#define LOG_INFO ANDROID_LOG_INFO
#define LOG_WARN ANDROID_LOG_WARN
#define LOG_ERROR ANDROID_LOG_ERROR
#else // #ifdef __ANDROID_NDK__
#include <utils/Log.h>
#ifdef ALOG
#define ANDROID_LOG ALOG
#else // #ifdef ALOG
#define ANDROID_LOG LOG
#endif // #ifdef ALOG
#endif // #ifdef __ANDROID_NDK__
#endif // #if USE_ANDROID_LOG
#define BREAK_IF_ZERO(ERR,X) if(0==(X)) {result = (ERR); break;}
#define BREAK_IF_NON_ZERO(ERR,X) if(0!=(X)) {result = (ERR); break;}
#define BREAK_IF_NON_ZERO_RC(ERR,RC,X) if(0!=(RC=(X))) {result = (ERR); break;}

namespace qc_loc_fw
{

static char global_log_tag[64] = { 'Q', 'C', 'A', 'L', 'O', 'G', '\0' };
static ERROR_LEVEL global_log_level = EL_LOG_OFF;
static const char * const private_log_tag = "LOG_UTIL";
static Mutex * global_log_mutex = 0;

class LocalLogLevelItem
{
private:
  char * local_tag;
  ERROR_LEVEL local_log_level;

  void setTag(const char * const tag)
  {
    if(0 != tag)
    {
      local_tag = strdup(tag);
    }

    local_log_level = EL_LOG_ALL;
  }

public:

  const char * getTag() const
  {
    return local_tag;
  }

  void setLevel(const ERROR_LEVEL level)
  {
    local_log_level = level;
  }

  ERROR_LEVEL getLevel() const
  {
    return local_log_level;
  }

  LocalLogLevelItem(const char * const tag)
  {
    setTag(tag);
    local_log_level = EL_LOG_ALL;
  }

  LocalLogLevelItem(const LocalLogLevelItem & rhs)
  {
    if(&rhs == this)
    {
      // no operation for assigning to self
      return;
    }

    setTag(rhs.local_tag);
    local_log_level = rhs.local_log_level;
  }

  ~LocalLogLevelItem()
  {
    if(0 != local_tag)
    {
      // we got this from strdup, which uses malloc.
      free(local_tag);
      local_tag = 0;
    }
    local_tag = 0;
  }

  bool equals(const char * const tag)
  {
    if((0 == tag) || (0 == local_tag))
    {
      return false;
    }
    if(0 == strcmp(tag, local_tag))
    {
      return true;
    }
    return false;
  }
};

static List<LocalLogLevelItem> * local_log_level_list = 0;

class StaticLogSetup
{
public:
  StaticLogSetup();
  ~StaticLogSetup();
};

StaticLogSetup::StaticLogSetup()
{
  global_log_mutex = Mutex::createInstance();
  local_log_level_list = new (std::nothrow) List<LocalLogLevelItem>();
}

StaticLogSetup::~StaticLogSetup()
{
  delete global_log_mutex;
  global_log_mutex = 0;

  delete local_log_level_list;
  local_log_level_list = 0;
}

StaticLogSetup staticLogSetup;

static LocalLogLevelItem * findLocalLevelItemLocked(const char * const tag)
{
  LocalLogLevelItem * p_item = 0;

  if(0 != local_log_level_list)
  {
    for (ListIterator<LocalLogLevelItem> it = local_log_level_list->begin(); it != local_log_level_list->end(); ++it)
    {
      if(it->equals(tag))
      {
        p_item = it.ptr();
        break;
      }
    }
  }
  return p_item;
}

static bool is_log_enabled(const char * const local_log_tag, const ERROR_LEVEL log_level)
{
  bool ok_to_log = false;

  const LocalLogLevelItem * const pLogLevelRec = findLocalLevelItemLocked(local_log_tag);
  if(0 != pLogLevelRec)
  {
    if(log_level <= pLogLevelRec->getLevel())
    {
      ok_to_log = true;
    }
  }
  else
  {
    if(log_level <= global_log_level)
    {
      ok_to_log = true;
    }
  }

  return ok_to_log;
}

bool is_log_verbose_enabled(const char * const local_log_tag)
{
  return is_log_enabled(local_log_tag, EL_VERBOSE);
}

static void vlog(const char * const local_log_tag, const ERROR_LEVEL log_level, const char * const format, va_list args)
{
  bool ok_to_log = false;
  const LocalLogLevelItem * const pLogLevelRec = findLocalLevelItemLocked(local_log_tag);
  if(0 != pLogLevelRec)
  {
    if(log_level <= pLogLevelRec->getLevel())
    {
      ok_to_log = true;
    }
  }
  else
  {
    if(log_level <= global_log_level)
    {
      ok_to_log = true;
    }
  }

  if(ok_to_log)
  {
    int format_result = -1;
    char buffer1[512];
    char buffer2[512];
    format_result = vsnprintf(buffer1, sizeof(buffer1), format, args);

#if USE_ANDROID_LOG
    if(format_result > 0)
    {
      if(0 != local_log_tag)
      {
        format_result = snprintf(buffer2, sizeof(buffer2), "[%s] %s", local_log_tag, buffer1);
      }
      else
      {
        format_result = snprintf(buffer2, sizeof(buffer2), "%s", buffer1);
      }
    }
    else
    {
      if(0 != local_log_tag)
      {
        format_result = snprintf(buffer2, sizeof(buffer2), "[%s] log subsystem message format error", local_log_tag);
      }
      else
      {
        format_result = snprintf(buffer2, sizeof(buffer2), "log subsystem message format error");
      }
    }

    if(format_result > 0)
    {
      switch (log_level)
      {
        case EL_ERROR:
        ANDROID_LOG(LOG_ERROR, global_log_tag, "%s", buffer2);
        break;
        case EL_WARNING:
        ANDROID_LOG(LOG_WARN, global_log_tag, "%s", buffer2);
        break;
        case EL_INFO:
        ANDROID_LOG(LOG_INFO, global_log_tag, "%s", buffer2);
        break;
        case EL_DEBUG:
        ANDROID_LOG(LOG_DEBUG, global_log_tag, "%s", buffer2);
        break;
        case EL_VERBOSE:
        ANDROID_LOG(LOG_VERBOSE, global_log_tag, "%s", buffer2);
        break;
        default:
        ANDROID_LOG(LOG_ERROR, global_log_tag, "Internal error in log subsystem: unknown log level");
        break;
      }
    }
    else
    {
      ANDROID_LOG(LOG_ERROR, global_log_tag, "Internal error in log subsystem: format error");
    }
#endif // #if USE_ANDROID_LOG
#if USE_STDOUT_LOG

    if(format_result > 0)
    {
      if(0 != local_log_tag)
      {
        format_result = snprintf(buffer2, sizeof(buffer2), "[%.2f][%s-%s] %s",
                                 get_time_monotonic_ms() / 1000.0,
                                 global_log_tag, local_log_tag,
                                 buffer1);
      }
      else
      {
        format_result = snprintf(buffer2, sizeof(buffer2), "[%s] %s", global_log_tag, buffer1);
      }
    }
    else
    {
      if(0 != local_log_tag)
      {
        format_result = snprintf(buffer2, sizeof(buffer2), "[%s-%s] log subsystem message format error", global_log_tag,
            local_log_tag);
      }
      else
      {
        format_result = snprintf(buffer2, sizeof(buffer2), "[%s] log subsystem message format error", global_log_tag);
      }
    }

    if(format_result > 0)
    {
      puts(buffer2);
    }
    else
    {
      puts("Internal error in log subsystem: format error");
    }
#endif // #if USE_PUTS_LOG
  }
}

int log_set_global_level(const ERROR_LEVEL level)
{
  int result = 1;
  do
  {
    BREAK_IF_ZERO(2, global_log_mutex);

    AutoLock latch(global_log_mutex);

    BREAK_IF_NON_ZERO(3, latch.ZeroIfLocked());

    global_log_level = level;

    result = 0;
  } while (0);

  if(0 != result)
  {
    log_error(private_log_tag, "log_set_global_level failed %d", result);
  }
  return result;
}

int log_set_global_tag(const char * const tag)
{
  int result = 1;
  do
  {
    BREAK_IF_ZERO(2, global_log_mutex);

    AutoLock latch(global_log_mutex);

    BREAK_IF_NON_ZERO(3, latch.ZeroIfLocked());

    size_t copy_length = strlcpy(global_log_tag, tag, sizeof(global_log_tag));
    if(copy_length >= sizeof(global_log_tag))
    {
      result = 4;
      break;
    }

    result = 0;
  } while (0);

  if(0 != result)
  {
    log_error(private_log_tag, "log_set_global_tag failed %d", result);
  }
  return result;
}

int log_set_local_level_for_tag(const char * const tag, const ERROR_LEVEL level)
{
  int result = 1;
  do
  {
    BREAK_IF_ZERO(2, tag);
    BREAK_IF_ZERO(3, global_log_mutex);
    BREAK_IF_ZERO(4, local_log_level_list);

    AutoLock latch(global_log_mutex);

    BREAK_IF_NON_ZERO(5, latch.ZeroIfLocked());

    LocalLogLevelItem * const p_item = findLocalLevelItemLocked(tag);

    if(0 != p_item)
    {
      p_item->setLevel(level);
    }
    else
    {
      LocalLogLevelItem item(tag);
      item.setLevel(level);
      BREAK_IF_ZERO(6, item.getTag());
      BREAK_IF_NON_ZERO(7, local_log_level_list->add(item));
    }

    result = 0;
  } while (0);

  if(0 != result)
  {
    log_error(private_log_tag, "log_set_local_level_for_tag failed %d", result);
  }
  return result;
}

int log_flush_all_local_level()
{
  return log_flush_local_level_for_tag(0);
}

int log_flush_local_level_for_tag(const char * const tag)
{
  int result = 1;
  do
  {
    BREAK_IF_ZERO(2, global_log_mutex);
    BREAK_IF_ZERO(3, local_log_level_list)

    AutoLock latch(global_log_mutex);

    BREAK_IF_NON_ZERO(4, latch.ZeroIfLocked());

    for (ListIterator<LocalLogLevelItem> it = local_log_level_list->begin(); it != local_log_level_list->end();)
    {
      if((0 == tag) || it->equals(tag))
      {
        it = local_log_level_list->erase(it);
      }
      else
      {
        ++it;
      }
    }

    result = 0;
  } while (0);

  if(0 != result)
  {
    log_error(private_log_tag, "log_flush_local_level_for_tag failed %d", result);
  }
  return result;
}

void log_error_no_lock(const char * const local_log_tag, const char * const format, ...)
{
  // note there is NO AUTO LOCK for this function
  // this function is designed to report WITHOUT any locks
  va_list args;
  va_start(args, format);
  // don't lock the global mutex, for we are reporting a bug in threading code
  vlog(local_log_tag, EL_ERROR, format, args);
  va_end(args);
}

void log_error(const char * const local_log_tag, const char * const format, ...)
{
  // ignore null checking for global_log_mutex. let it fail and continue
  // there is something very wrong, but we still want to log message to be sent out
  AutoLock latch(global_log_mutex);
  va_list args;
  va_start(args, format);
  vlog(local_log_tag, EL_ERROR, format, args);
  va_end(args);
}

void log_warning(const char * const local_log_tag, const char * const format, ...)
{
  // ignore null checking for global_log_mutex. let it fail and continue
  // there is something very wrong, but we still want to log message to be sent out
  AutoLock latch(global_log_mutex);
  va_list args;
  va_start(args, format);
  vlog(local_log_tag, EL_WARNING, format, args);
  va_end(args);
}

void log_info(const char * const local_log_tag, const char * const format, ...)
{
  // ignore null checking for global_log_mutex. let it fail and continue
  // there is something very wrong, but we still want to log message to be sent out
  AutoLock latch(global_log_mutex);
  va_list args;
  va_start(args, format);
  vlog(local_log_tag, EL_INFO, format, args);
  va_end(args);
}

void log_debug(const char * const local_log_tag, const char * const format, ...)
{
  // ignore null checking for global_log_mutex. let it fail and continue
  // there is something very wrong, but we still want to log message to be sent out
  AutoLock latch(global_log_mutex);
  va_list args;
  va_start(args, format);
  vlog(local_log_tag, EL_DEBUG, format, args);
  va_end(args);
}

void log_verbose(const char * const local_log_tag, const char * const format, ...)
{
  // ignore null checking for global_log_mutex. let it fail and continue
  // there is something very wrong, but we still want to log message to be sent out
  AutoLock latch(global_log_mutex);
  va_list args;
  va_start(args, format);
  vlog(local_log_tag, EL_VERBOSE, format, args);
  va_end(args);
}

} // namespace qc_loc_fw

