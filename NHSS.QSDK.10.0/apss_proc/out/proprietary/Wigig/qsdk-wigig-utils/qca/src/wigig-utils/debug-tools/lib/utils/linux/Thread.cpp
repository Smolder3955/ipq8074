/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Thread.h"
#include "wlct_os.h"


void *CWlctThread::GlobalThreadMain(void *p)
{
  CWlctThread *threadObj = (CWlctThread *)p;

  threadObj->ThreadMain();

  pthread_exit(&threadObj->threadResult);
}

void CWlctThread::ThreadMain(void)
{
  threadProc(threadCtx);
}

CWlctThread::CWlctThread(IWlctThreadThreadProc tProc, void *tCtx)
  : IWlctThread(tProc, tCtx),
    threadResult(0)
{
  memset(&threadObj, 0, sizeof(threadObj));
}

CWlctThread::~CWlctThread()
{
  Stop();
}

wlct_os_err_t CWlctThread::Start()
{
  shouldStop = false;

  pthread_attr_t attr;

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  threadResult = pthread_create(&threadObj,
                                &attr,
                                GlobalThreadMain,
                                this);

  pthread_attr_destroy(&attr);

  return threadResult;
}

void CWlctThread::Stop()
{
  void *tres = NULL;
  shouldStop = true;
  /* Wait for the notification thread to process the signal */
  pthread_join(threadObj, &tres);
}

uint32_t CWlctThread::getCurrentThreadID()
{
  return (uint32_t)pthread_self();
}

uint32_t CWlctThread::getCurrentProcessID()
{
  return (uint32_t)getpid();
}

