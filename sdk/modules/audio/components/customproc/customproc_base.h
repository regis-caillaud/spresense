/****************************************************************************
 * modules/audio/components/customproc/customproc_base.h
 *
 *   Copyright 2018 Sony Semiconductor Solutions Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Sony Semiconductor Solutions Corporation nor
 *    the names of its contributors may be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#ifndef _CUSTOMPROC_BASE_H_
#define _CUSTOMPROC_BASE_H_

#include <audio/dsp_framework/customproc_command_base.h>
#include "audio/audio_common_defs.h"
#include "memutils/os_utils/chateau_osal.h"
#include "memutils/memory_manager/MemHandle.h"

enum CustomProcEventType
{
  CustomProcInit = 0,
  CustomProcExec,
  CustomProcFlush,
  CustomProcSet,
};

struct CustomProcCbParam
{
  CustomProcEventType event_type;
  bool                result;
};

typedef bool (*CustomProcCallback)(CustomProcCbParam*, void*);

struct CustomProcPacket
{
  uint8_t  *addr;
  uint32_t size;
};

struct InitCustomProcParam
{
  uint8_t          cmd_type;
  bool             is_userdraw;
  CustomProcPacket packet;
};
typedef InitCustomProcParam SetCustomProcParam;

struct ExecCustomProcParam
{
  AsPcmDataParam        input;
  MemMgrLite::MemHandle output_mh;
};

struct FlushCustomProcParam
{
  CustomProcPacket      packet;
  MemMgrLite::MemHandle output_mh;
};

struct CustomProcCmpltParam
{
  bool                 result;
  AsPcmDataParam       output;
};

class CustomProcBase
{
public:
  CustomProcBase() {}
  virtual ~CustomProcBase() {}

  virtual uint32_t init(const InitCustomProcParam& param) = 0;
  virtual bool exec(const ExecCustomProcParam& param) = 0;
  virtual bool flush(const FlushCustomProcParam& param) = 0;
  virtual bool set(const SetCustomProcParam& param) = 0;
  virtual bool recv_done(CustomProcCmpltParam *cmplt) = 0;
  virtual bool recv_done(void) = 0;
  virtual uint32_t activate(CustomProcCallback callback,
                            const char *image_name,
                            void *p_requester,
                            uint32_t *dsp_inf) = 0;
  virtual bool deactivate() = 0;

protected:
  CustomProcCallback m_callback;

  void *m_p_requester;
};

#endif /* _CUSTOMPROC_BASE_H_ */

