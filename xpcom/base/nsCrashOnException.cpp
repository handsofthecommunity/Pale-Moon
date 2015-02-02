/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCrashOnException.h"
#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {

static int ReportException(EXCEPTION_POINTERS *aExceptionInfo)
{
  return EXCEPTION_EXECUTE_HANDLER;
}

XPCOM_API(LRESULT)
CallWindowProcCrashProtected(WNDPROC wndProc, HWND hWnd, UINT msg,
                            WPARAM wParam, LPARAM lParam)
{
  MOZ_SEH_TRY {
    return wndProc(hWnd, msg, wParam, lParam);
  }
  MOZ_SEH_EXCEPT(ReportException(GetExceptionInformation())) {
    ::TerminateProcess(::GetCurrentProcess(), 253);
  }
  return 0; // not reached
}

}

