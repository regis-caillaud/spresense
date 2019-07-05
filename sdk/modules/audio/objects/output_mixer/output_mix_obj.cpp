/****************************************************************************
 * modules/audio/objects/output_mixer/output_mix_obj.cpp
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <nuttx/arch.h>
#include <arch/chip/cxd56_audio.h>
#include "output_mix_obj.h"
#include "debug/dbg_log.h"

__USING_WIEN2
using namespace MemMgrLite;

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

static pid_t    s_omix_pid = -1;
static AsOutputMixMsgQueId_t s_msgq_id;
static AsOutputMixPoolId_t   s_pool_id;
static OutputMixObjectTask *s_omix_ojb = NULL;

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

OutputMixObjectTask::OutputMixObjectTask(AsOutputMixMsgQueId_t msgq_id,
                                         AsOutputMixPoolId_t pool_id) :
  m_msgq_id(msgq_id),
  m_output_device(HPOutputDevice)
  {
    for (int i = 0; i < HPI2SoutChNum; i++)
      {
        m_output_mix_to_hpi2s[i].set_self_dtq(msgq_id.mixer);
      }

    m_output_mix_to_hpi2s[0].set_apu_dtq(msgq_id.render_path0_filter_dsp);
    m_output_mix_to_hpi2s[1].set_apu_dtq(msgq_id.render_path1_filter_dsp);

    m_output_mix_to_hpi2s[0].set_pcm_pool_id(pool_id.render_path0_filter_pcm);
    m_output_mix_to_hpi2s[1].set_pcm_pool_id(pool_id.render_path1_filter_pcm);

    m_output_mix_to_hpi2s[0].set_apu_pool_id(pool_id.render_path0_filter_dsp);
    m_output_mix_to_hpi2s[1].set_apu_pool_id(pool_id.render_path1_filter_dsp);
  }

/*--------------------------------------------------------------------------*/
int OutputMixObjectTask::getHandle(MsgPacket* msg)
{
  int handle = 0;
  MsgType msgtype = msg->getType();

  switch (msgtype)
    {
      case MSG_AUD_MIX_CMD_ACT:
      case MSG_AUD_MIX_CMD_DEACT:
      case MSG_AUD_MIX_CMD_CLKRECOVERY:
      case MSG_AUD_MIX_CMD_INITMPP:
      case MSG_AUD_MIX_CMD_SETMPP:
        handle = msg->peekParam<OutputMixerCommand>().handle;
        break;

      case MSG_AUD_MIX_CMD_DATA:
        handle = msg->peekParam<AsPcmDataParam>().identifier;
        break;

      default:
        handle = msg->peekParam<OutputMixObjParam>().handle;
        break;
    }

  return handle;
}

/*--------------------------------------------------------------------------*/
void OutputMixObjectTask::enableOutputDevice()
{
  CXD56_AUDIO_ECODE error_code;

  switch (m_output_device)
    {
      case A2dpSrcOutputDevice:
        cxd56_audio_dis_i2s_io();
        error_code = cxd56_audio_dis_output();
        break;

      case HPOutputDevice:
        cxd56_audio_dis_i2s_io();
        cxd56_audio_set_spout(true);
        error_code = cxd56_audio_en_output();
        break;

      case I2SOutputDevice:
        {
          cxd56_audio_en_i2s_io();
          cxd56_audio_set_spout(false);
          error_code = cxd56_audio_en_output();
          if (error_code != CXD56_AUDIO_ECODE_OK)
            {
              break;
            }

          /* Set Path, Mixer to I2S0 */

          cxd56_audio_signal_t sig_id = CXD56_AUDIO_SIG_MIX;
          cxd56_audio_sel_t    sel_info;
          sel_info.au_dat_sel1 = false;
          sel_info.au_dat_sel2 = false;
          sel_info.cod_insel2  = false;
          sel_info.cod_insel3  = false;
          sel_info.src1in_sel  = true;
          sel_info.src2in_sel  = false;

          error_code = cxd56_audio_set_datapath(sig_id, sel_info);
        }
        break;

      default:
        OUTPUT_MIX_ERR(AS_ATTENTION_SUB_CODE_UNEXPECTED_PARAM);
        return;;
    }

  if (error_code != CXD56_AUDIO_ECODE_OK)
    {
      OUTPUT_MIX_ERR(AS_ATTENTION_CODE_FATAL);
    }
}

/*--------------------------------------------------------------------------*/
void OutputMixObjectTask::disableOutputDevice()
{
  CXD56_AUDIO_ECODE error_code;

  switch (m_output_device)
    {
      case A2dpSrcOutputDevice:
        /* Do nothing */

        error_code = CXD56_AUDIO_ECODE_OK;
        break;

      case HPOutputDevice:
        cxd56_audio_set_spout(false);
        error_code = cxd56_audio_dis_output();
        break;

      case I2SOutputDevice:
        {
          cxd56_audio_dis_i2s_io();
          error_code = cxd56_audio_dis_output();
          if (error_code != CXD56_AUDIO_ECODE_OK)
            {
              break;
            }
        }
        break;

      default:
        OUTPUT_MIX_ERR(AS_ATTENTION_SUB_CODE_UNEXPECTED_PARAM);
        return;
    }

  if (error_code != CXD56_AUDIO_ECODE_OK)
    {
      OUTPUT_MIX_ERR(AS_ATTENTION_CODE_FATAL);
    }
}

/*--------------------------------------------------------------------------*/
void OutputMixObjectTask::run()
{
  err_t        err_code;
  MsgQueBlock* que;
  MsgPacket*   msg;

  err_code = MsgLib::referMsgQueBlock(m_msgq_id.mixer, &que);
  F_ASSERT(err_code == ERR_OK);

  while (1)
    {
      err_code = que->recv(TIME_FOREVER, &msg);
      F_ASSERT(err_code == ERR_OK);

      parse(msg);

      err_code = que->pop();
      F_ASSERT(err_code == ERR_OK);
    }
}

/*--------------------------------------------------------------------------*/
void OutputMixObjectTask::parse(MsgPacket* msg)
{
  MsgType msg_type = msg->getType();

  if (MSG_GET_CATEGORY(msg_type) == MSG_CAT_AUD_MIX)
    {
      switch(msg_type)
        {
          case MSG_AUD_MIX_CMD_ACT:
            {
              m_output_device = static_cast<AsOutputMixDevice>
                (msg->peekParam<OutputMixerCommand>().act_param.output_device);

              /* Enable output device */

              enableOutputDevice();
            }
            break;

          case MSG_AUD_MIX_CMD_DEACT:
            {
              /* Disable output device */

              disableOutputDevice();
            }
            break;

          default:
            /* Do nothing */

            break;
        }

      switch(m_output_device)
        {
          case HPOutputDevice:
          case I2SOutputDevice:
            m_output_mix_to_hpi2s[getHandle(msg)].parse(msg);
            break;

          case A2dpSrcOutputDevice:
            break;

          default:
            OUTPUT_MIX_ERR(AS_ATTENTION_SUB_CODE_UNEXPECTED_PARAM);
            break;
        }
    }
  else
    {
      F_ASSERT(0);
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/*--------------------------------------------------------------------------*/
int AS_OutputMixObjEntry(int argc, char *argv[])
{
  OutputMixObjectTask::create(s_msgq_id, s_pool_id);
  return 0;
}

/*--------------------------------------------------------------------------*/
bool AS_CreateOutputMixer(FAR AsCreateOutputMixParam_t *param)
{
  return AS_CreateOutputMixer(param, NULL);
}

/*--------------------------------------------------------------------------*/
bool AS_CreateOutputMixer(FAR AsCreateOutputMixParam_t *param, AudioAttentionCb attcb)
{
  /* Register attention callback */

  OUTPUT_MIX_REG_ATTCB(attcb);

  /* Parameter check */

  if (param == NULL)
    {
      OUTPUT_MIX_ERR(AS_ATTENTION_SUB_CODE_UNEXPECTED_PARAM);
      return false;
    }

  /* Create */

  s_msgq_id = param->msgq_id;
  s_pool_id = param->pool_id;

  /* Reset Message queue. */

  FAR MsgQueBlock *que;
  err_t err_code = MsgLib::referMsgQueBlock(s_msgq_id.mixer, &que);
  F_ASSERT(err_code == ERR_OK);
  que->reset();

  s_omix_pid = task_create("OMIX_OBJ",
                           150, 1024 * 3,
                           AS_OutputMixObjEntry,
                           NULL);
  if (s_omix_pid < 0)
    {
      OUTPUT_MIX_ERR(AS_ATTENTION_SUB_CODE_TASK_CREATE_ERROR);
      return false;
    }

  return true;
}

/*--------------------------------------------------------------------------*/
bool AS_ActivateOutputMixer(uint8_t handle, FAR AsActivateOutputMixer *actparam)
{
  /* Parameter check */

  if (actparam == NULL)
    {
      return false;
    }

  /* Activate */

  OutputMixerCommand cmd;

  cmd.handle    = handle;
  cmd.act_param = *actparam;

  err_t er = MsgLib::send<OutputMixerCommand>(s_msgq_id.mixer,
                                              MsgPriNormal,
                                              MSG_AUD_MIX_CMD_ACT,
                                              s_msgq_id.mng,
                                              cmd);
  F_ASSERT(er == ERR_OK);

  return true;
}

/*--------------------------------------------------------------------------*/
bool AS_SendDataOutputMixer(FAR AsSendDataOutputMixer *sendparam)
{
  /* Parameter check */

  if (sendparam == NULL)
    {
      return false;
    }

  /* Send data, reload parameters */

  sendparam->pcm.identifier = sendparam->handle;
  sendparam->pcm.callback   = sendparam->callback;

  err_t er = MsgLib::send<AsPcmDataParam>(s_msgq_id.mixer,
                                          MsgPriNormal,
                                          MSG_AUD_MIX_CMD_DATA,
                                          s_msgq_id.mng,
                                          sendparam->pcm);
  F_ASSERT(er == ERR_OK);

  return true;
}

/*--------------------------------------------------------------------------*/
bool AS_FrameTermFineControlOutputMixer(uint8_t handle, FAR AsFrameTermFineControl *ftermparam)
{
  /* Parameter check */

  if (ftermparam == NULL)
    {
      return false;
    }

  /* Set frame term */

  OutputMixerCommand cmd;

  cmd.handle      = handle;
  cmd.fterm_param = *ftermparam;

  err_t er = MsgLib::send<OutputMixerCommand>(s_msgq_id.mixer,
                                              MsgPriNormal,
                                              MSG_AUD_MIX_CMD_CLKRECOVERY,
                                              s_msgq_id.mng,
                                              cmd);
  F_ASSERT(er == ERR_OK);

  return true;
}

/*--------------------------------------------------------------------------*/
bool AS_InitPostprocOutputMixer(uint8_t handle, FAR AsInitPostProc *initppparam)
{
  /* Parameter check */

  if (initppparam == NULL)
    {
      return false;
    }

  /* Set Postfilter command param */

  OutputMixerCommand cmd;

  cmd.handle       = handle;
  cmd.initpp_param = *initppparam;

  err_t er = MsgLib::send<OutputMixerCommand>(s_msgq_id.mixer,
                                              MsgPriNormal,
                                              MSG_AUD_MIX_CMD_INITMPP,
                                              s_msgq_id.mng,
                                              cmd);
  F_ASSERT(er == ERR_OK);

  return true;
}

/*--------------------------------------------------------------------------*/
bool AS_SetPostprocOutputMixer(uint8_t handle, FAR AsSetPostProc *setppparam)
{
  /* Parameter check */

  if (setppparam == NULL)
    {
      return false;
    }

  /* Set Postfilter command param */

  OutputMixerCommand cmd;

  cmd.handle       = handle;
  cmd.setpp_param = *setppparam;

  err_t er = MsgLib::send<OutputMixerCommand>(s_msgq_id.mixer,
                                              MsgPriNormal,
                                              MSG_AUD_MIX_CMD_SETMPP,
                                              s_msgq_id.mng,
                                              cmd);
  F_ASSERT(er == ERR_OK);

  return true;
}

/*--------------------------------------------------------------------------*/
bool AS_DeactivateOutputMixer(uint8_t handle, FAR AsDeactivateOutputMixer *deactparam)
{
  /* Parameter check */

  if (deactparam == NULL)
    {
      return false;
    }

  /* Deactivate */

  OutputMixerCommand cmd;

  cmd.handle      = handle;
  cmd.deact_param = *deactparam;

  err_t er = MsgLib::send<OutputMixerCommand>(s_msgq_id.mixer,
                                              MsgPriNormal,
                                              MSG_AUD_MIX_CMD_DEACT,
                                              s_msgq_id.mng,
                                              cmd);
  F_ASSERT(er == ERR_OK);

  return true;
}

/*--------------------------------------------------------------------------*/
bool AS_DeleteOutputMix(void)
{
  if (s_omix_pid < 0)
    {
      return false;
    }

  task_delete(s_omix_pid);

  if (s_omix_ojb != NULL)
    {
      delete s_omix_ojb;
      s_omix_ojb = NULL;

      /* Unregister attention callback */

      OUTPUT_MIX_UNREG_ATTCB();
    }
  return true;
}

/*--------------------------------------------------------------------------*/
bool AS_checkAvailabilityOutputMixer(void)
{
  return (s_omix_ojb != NULL);
}

/*--------------------------------------------------------------------------*/
void OutputMixObjectTask::create(AsOutputMixMsgQueId_t msgq_id,
                                 AsOutputMixPoolId_t pool_id)
{
  if (s_omix_ojb == NULL)
    {
      s_omix_ojb = new OutputMixObjectTask(msgq_id, pool_id);
      if (s_omix_ojb == NULL)
        {
          OUTPUT_MIX_ERR(AS_ATTENTION_SUB_CODE_RESOURCE_ERROR);
          return;
        }
      s_omix_ojb->run();
    }
  else
    {
      OUTPUT_MIX_ERR(AS_ATTENTION_SUB_CODE_RESOURCE_ERROR);
    }
}


