/****************************************************************************
 * modules/audio/objects/front_end/front_end_obj.cpp
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

#include <stdlib.h>
#include <nuttx/arch.h>
#include <stdlib.h>
#include <arch/chip/cxd56_audio.h>
#include "front_end_obj.h"
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

static pid_t s_mfe_pid;
static AsMicFrontendMsgQueId_t s_msgq_id;
static AsMicFrontendPoolId_t   s_pool_id;

static MicFrontEndObject *s_mfe_obj = NULL;

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void capture_done_callback(CaptureDataParam param)
{
  err_t er;

  er = MsgLib::send<CaptureDataParam>(s_msgq_id.micfrontend,
                                      MsgPriNormal,
                                      MSG_AUD_MFE_RST_CAPTURE_DONE,
                                      NULL,
                                      param);
  F_ASSERT(er == ERR_OK);
}

/*--------------------------------------------------------------------------*/
static void capture_error_callback(CaptureErrorParam param)
{
  err_t er;

  er = MsgLib::send<CaptureErrorParam>(s_msgq_id.micfrontend,
                                       MsgPriNormal,
                                       MSG_AUD_MFE_RST_CAPTURE_ERR,
                                       NULL,
                                       param);
  F_ASSERT(er == ERR_OK);
}

/*--------------------------------------------------------------------------*/
static bool preproc_done_callback(CustomProcCbParam *cmplt, void* p_requester)
{
  MicFrontendObjPreProcDoneCmd param;

  param.event_type = cmplt->event_type;
  param.result     = cmplt->result;

  err_t er = MsgLib::send<MicFrontendObjPreProcDoneCmd>(s_msgq_id.micfrontend,
                                                        MsgPriNormal,
                                                        MSG_AUD_MFE_RST_PREPROC,
                                                        NULL,
                                                        param);

  F_ASSERT(er == ERR_OK);

  return true;
}

/*--------------------------------------------------------------------------*/
static void pcm_send_done_callback(int32_t identifier, bool is_end)
{
}

/*--------------------------------------------------------------------------*/
int AS_MicFrontendObjEntry(int argc, char *argv[])
{
  MicFrontEndObject::create(s_msgq_id,
                            s_pool_id);
  return 0;
}

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::run(void)
{
  err_t        err_code;
  MsgQueBlock* que;
  MsgPacket*   msg;

  err_code = MsgLib::referMsgQueBlock(m_msgq_id.micfrontend, &que);
  F_ASSERT(err_code == ERR_OK);

  while(1)
    {
      err_code = que->recv(TIME_FOREVER, &msg);
      F_ASSERT(err_code == ERR_OK);

      parse(msg);

      err_code = que->pop();
      F_ASSERT(err_code == ERR_OK);
    }
}

/*--------------------------------------------------------------------------*/
MicFrontEndObject::MsgProc
  MicFrontEndObject::MsgProcTbl[AUD_MFE_MSG_NUM][MicFrontendStateNum] =
{
  /* Message Type: MSG_AUD_MFE_CMD_ACT */

  {                                  /* MicFrontend status: */
    &MicFrontEndObject::activate,       /*   Inactive.      */
    &MicFrontEndObject::illegal,        /*   Ready.         */
    &MicFrontEndObject::illegal,        /*   Active.        */
    &MicFrontEndObject::illegal,        /*   Stopping.      */
    &MicFrontEndObject::illegal,        /*   ErrorStopping. */
    &MicFrontEndObject::illegal         /*   WaitStop.      */
  },

  /* Message Type: MSG_AUD_MFE_CMD_DEACT */

  {                                  /* MicFrontend status: */
    &MicFrontEndObject::illegal,        /*   Inactive.      */
    &MicFrontEndObject::deactivate,     /*   Ready.         */
    &MicFrontEndObject::illegal,        /*   Active.        */
    &MicFrontEndObject::illegal,        /*   Stopping.      */
    &MicFrontEndObject::illegal,        /*   ErrorStopping. */
    &MicFrontEndObject::illegal         /*   WaitStop.      */
  },

  /* Message Type: MSG_AUD_MFE_CMD_INIT */

  {                                  /* MicFrontend status: */
    &MicFrontEndObject::illegal,        /*   Inactive.      */
    &MicFrontEndObject::init,           /*   Ready.         */
    &MicFrontEndObject::illegal,        /*   Active.        */
    &MicFrontEndObject::illegal,        /*   Stopping.      */
    &MicFrontEndObject::illegal,        /*   ErrorStopping. */
    &MicFrontEndObject::illegal         /*   WaitStop.      */
  },

  /* Message Type: MSG_AUD_MFE_CMD_START */

  {                                  /* MicFrontend status: */
    &MicFrontEndObject::illegal,        /*   Inactive.      */
    &MicFrontEndObject::startOnReady,   /*   Ready.         */
    &MicFrontEndObject::illegal,        /*   Active.        */
    &MicFrontEndObject::illegal,        /*   Stopping.      */
    &MicFrontEndObject::illegal,        /*   ErrorStopping. */
    &MicFrontEndObject::illegal         /*   WaitStop.      */
  },

  /* Message Type: MSG_AUD_MFE_CMD_STOP */

  {                                   /* MicFrontend status: */
    &MicFrontEndObject::illegal,         /*   Inactive.      */
    &MicFrontEndObject::illegal,         /*   Ready.         */
    &MicFrontEndObject::stopOnActive,    /*   Active.        */
    &MicFrontEndObject::illegal,         /*   Stopping.      */
    &MicFrontEndObject::stopOnErrorStop, /*   ErrorStopping. */
    &MicFrontEndObject::stopOnWait       /*   WaitStop.      */
  },

  /* Message Type: MSG_AUD_MFE_CMD_INITPREPROC */

  {                                   /* MicFrontend status: */
    &MicFrontEndObject::illegal,         /*   Inactive.      */
    &MicFrontEndObject::initPreproc,     /*   Ready.         */
    &MicFrontEndObject::illegal,         /*   Active.        */
    &MicFrontEndObject::illegal,         /*   Stopping.      */
    &MicFrontEndObject::illegal,         /*   ErrorStopping. */
    &MicFrontEndObject::illegal          /*   WaitStop.      */
  },

  /* Message Type: MSG_AUD_MFE_CMD_SETPREPROC */

  {                                   /* MicFrontend status: */
    &MicFrontEndObject::illegal,         /*   Inactive.      */
    &MicFrontEndObject::setPreproc,      /*   Ready.         */
    &MicFrontEndObject::setPreproc,      /*   Active.        */
    &MicFrontEndObject::illegal,         /*   Stopping.      */
    &MicFrontEndObject::illegal,         /*   ErrorStopping. */
    &MicFrontEndObject::illegal          /*   WaitStop.      */
  },

  /* Message Type: MSG_AUD_MFE_CMD_SET_MICGAIN. */

  {                                   /* MicFrontend status: */
    &MicFrontEndObject::illegal,         /*   Inactive.      */
    &MicFrontEndObject::setMicGain,      /*   Ready.         */
    &MicFrontEndObject::setMicGain,      /*   Active.        */
    &MicFrontEndObject::illegal,         /*   Stopping.      */
    &MicFrontEndObject::illegal,         /*   ErrorStopping. */
    &MicFrontEndObject::illegal          /*   WaitStop.      */
  }
};

/*--------------------------------------------------------------------------*/
MicFrontEndObject::MsgProc
  MicFrontEndObject::RsltProcTbl[AUD_MFE_RST_MSG_NUM][MicFrontendStateNum] =
{
  /* Message Type: MSG_AUD_MFE_RST_CAPTURE_DONE. */

  {                                          /* MicFrontend status: */
    &MicFrontEndObject::illegalCaptureDone,     /*   Inactive.      */
    &MicFrontEndObject::illegalCaptureDone,     /*   Ready.         */
    &MicFrontEndObject::captureDoneOnActive,    /*   Active.        */
    &MicFrontEndObject::captureDoneOnStop,      /*   Stopping.      */
    &MicFrontEndObject::captureDoneOnErrorStop, /*   ErrorStopping. */
    &MicFrontEndObject::captureDoneOnWaitStop   /*   WaitStop.      */
  },

  /* Message Type: MSG_AUD_MFE_RST_CAPTURE_ERR. */

  {                                           /* MicFrontend status: */
    &MicFrontEndObject::illegalCaptureError,     /*   Inactive.      */
    &MicFrontEndObject::illegalCaptureError,     /*   Ready.         */
    &MicFrontEndObject::captureErrorOnActive,    /*   Active.        */
    &MicFrontEndObject::captureErrorOnStop,      /*   Stopping.      */
    &MicFrontEndObject::captureErrorOnErrorStop, /*   ErrorStopping. */
    &MicFrontEndObject::captureErrorOnWaitStop   /*   WaitStop.      */
  },

  /* Message Type: MSG_AUD_MFE_RST_PREPROC. */

  {                                           /* MicFrontend status: */
    &MicFrontEndObject::illegalPreprocDone,      /*   Inactive.      */
    &MicFrontEndObject::illegalPreprocDone,      /*   Ready.         */
    &MicFrontEndObject::preprocDoneOnActive,     /*   Active.        */
    &MicFrontEndObject::preprocDoneOnStop,       /*   Stopping.      */
    &MicFrontEndObject::preprocDoneOnErrorStop,  /*   ErrorStopping. */
    &MicFrontEndObject::preprocDoneOnWaitStop    /*   WaitStop.      */
  }
};

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::parse(MsgPacket *msg)
{
  uint32_t event;

  if (MSG_IS_REQUEST(msg->getType()) == 0)
    {
      event = MSG_GET_SUBTYPE(msg->getType());
      F_ASSERT((event < AUD_MFE_RST_MSG_NUM));

      (this->*RsltProcTbl[event][m_state.get()])(msg);
    }
  else
    {
      event = MSG_GET_SUBTYPE(msg->getType());
      F_ASSERT((event < AUD_MFE_MSG_NUM));

      (this->*MsgProcTbl[event][m_state.get()])(msg);
    }
}

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::reply(AsMicFrontendEvent evtype,
                              MsgType msg_type,
                              uint32_t result)
{
  if (m_callback != NULL)
    {
      m_callback(evtype, result, 0);
    }
  else if (m_msgq_id.mng != MSG_QUE_NULL)
    {
      AudioObjReply cmplt((uint32_t)msg_type,
                           AS_OBJ_REPLY_TYPE_REQ,
                           AS_MODULE_ID_MIC_FRONTEND_OBJ,
                           result);
      err_t er = MsgLib::send<AudioObjReply>(m_msgq_id.mng,
                                             MsgPriNormal,
                                             MSG_TYPE_AUD_RES,
                                             m_msgq_id.micfrontend,
                                             cmplt);
      if (ERR_OK != er)
        {
          F_ASSERT(0);
        }
    }
}

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::illegal(MsgPacket *msg)
{
  uint msgtype = msg->getType();
  msg->moveParam<MicFrontendCommand>();

  uint32_t idx = msgtype - MSG_AUD_MFE_CMD_ACT;

  AsMicFrontendEvent table[] =
  {
    AsMicFrontendEventAct,
    AsMicFrontendEventDeact,
    AsMicFrontendEventInit,
    AsMicFrontendEventStart,
    AsMicFrontendEventStop,
    AsMicFrontendEventInitPreProc,
    AsMicFrontendEventSetPreProc,
    AsMicFrontendEventSetMicGain
  };

  reply(table[idx], msg->getType(), AS_ECODE_STATE_VIOLATION);
}

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::activate(MsgPacket *msg)
{
  uint32_t rst;
  AsActivateMicFrontend act = msg->moveParam<MicFrontendCommand>().act_param;

  MIC_FRONTEND_DBG("ACT: indev %d\n", act.param.input_device);

  /* Set event callback */

  m_callback = act.cb;

  /* Parameter check */

  if (!checkAndSetMemPool())
    {
      reply(AsMicFrontendEventAct,
            msg->getType(),
            AS_ECODE_CHECK_MEMORY_POOL_ERROR);
      return;
    }

  rst = activateParamCheck(act.param);

  if (rst != AS_ECODE_OK)
    {
      reply(AsMicFrontendEventAct, msg->getType(), rst);
      return;
    }

  /* Select Proc type and Activate PreProcess */

  uint32_t dsp_inf = 0;

  switch (act.param.preproc_type)
    {
      case AsMicFrontendPreProcUserCustom:
        m_p_preproc_instance = new UserCustomComponent(m_pool_id.dsp,
                                                       m_msgq_id.dsp);
        break;

      default:
        m_p_preproc_instance = new ThruProcComponent();
        break;
    }

  uint32_t ret = m_p_preproc_instance->activate(preproc_done_callback,
                                                "PREPROC",
                                                static_cast<void *>(this),
                                                &dsp_inf);
  if (ret != AS_ECODE_OK)
    {
      delete m_p_preproc_instance;

      /* Error reply */

      reply(AsMicFrontendEventAct, msg->getType(), ret);

      return;
    }

  /* Hold preproc type */

  m_preproc_type = static_cast<AsMicFrontendPreProcType>(act.param.preproc_type);

  /* State transit */

  m_state = MicFrontendStateReady;

  /* Reply */

  reply(AsMicFrontendEventAct, msg->getType(), AS_ECODE_OK);
}

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::deactivate(MsgPacket *msg)
{
  msg->moveParam<MicFrontendCommand>();

  MIC_FRONTEND_DBG("DEACT:\n");

  if (!delInputDeviceHdlr())
    {
      reply(AsMicFrontendEventDeact,
            msg->getType(),
            AS_ECODE_CLEAR_AUDIO_DATA_PATH_ERROR);
      return;
    }

  /* Deactivate PreProcess */

  bool ret = m_p_preproc_instance->deactivate();

  if (!ret)
    {
      /* Error reply */

      reply(AsMicFrontendEventDeact, msg->getType(), AS_ECODE_DSP_UNLOAD_ERROR);

      return;
    }

  delete m_p_preproc_instance;

  /* State transit */

  m_state = MicFrontendStateInactive;

  /* Reply */

  reply(AsMicFrontendEventDeact, msg->getType(), AS_ECODE_OK);
}

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::init(MsgPacket *msg)
{
  uint32_t rst = AS_ECODE_OK;
  MicFrontendCommand cmd = msg->moveParam<MicFrontendCommand>();

  MIC_FRONTEND_DBG("INIT: ch num %d, bit len %d, samples/frame %d\n",
                 cmd.init_param.channel_number,
                 cmd.init_param.bit_length,
                 cmd.init_param.samples_per_frame);

  /* Check parameters */

  rst = initParamCheck(cmd);
  if (rst != AS_ECODE_OK)
    {
      reply(AsMicFrontendEventInit, msg->getType(), rst);
      return;
    }

  /* Hold parameters */

  m_channel_num       = cmd.init_param.channel_number;
  m_pcm_bit_width     =
    ((cmd.init_param.bit_length == AS_BITLENGTH_16)
      ? AudPcm16Bit : (cmd.init_param.bit_length == AS_BITLENGTH_24)
                        ? AudPcm24Bit : AudPcm32Bit);
  m_cap_bytes         = ((m_pcm_bit_width == AudPcm16Bit) ? 2 : 4);
  m_samples_per_frame = cmd.init_param.samples_per_frame;
  m_pcm_data_path     = static_cast<AsMicFrontendDataPath>(cmd.init_param.data_path);
  m_pcm_data_dest     = cmd.init_param.dest;

  /* Release and get capture (Input device) handler */

  if (!delInputDeviceHdlr())
    {
      reply(AsMicFrontendEventInit, msg->getType(), AS_ECODE_CLEAR_AUDIO_DATA_PATH_ERROR);
      return;
    }

  if (!getInputDeviceHdlr())
    {
      reply(AsMicFrontendEventInit, msg->getType(), AS_ECODE_SET_AUDIO_DATA_PATH_ERROR);
      return;
    }

  /* Init capture */

  CaptureComponentParam cap_comp_param;

  cap_comp_param.init_param.capture_ch_num    = m_channel_num;
  cap_comp_param.init_param.capture_bit_width = m_pcm_bit_width;
  cap_comp_param.init_param.preset_num        = CAPTURE_PRESET_NUM;
  cap_comp_param.init_param.callback          = capture_done_callback;
  cap_comp_param.init_param.err_callback      = capture_error_callback;
  cap_comp_param.handle                       = m_capture_hdlr;

  if (!AS_init_capture(&cap_comp_param))
    {
      reply(AsMicFrontendEventStart, msg->getType(), AS_ECODE_DMAC_INITIALIZE_ERROR);
      return;
    }

  /* Reply */

  reply(AsMicFrontendEventInit, msg->getType(), AS_ECODE_OK);
}

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::startOnReady(MsgPacket *msg)
{
  msg->moveParam<MicFrontendCommand>();

  MIC_FRONTEND_DBG("START:\n");

  /* Start capture */

  if (!startCapture())
    {
      reply(AsMicFrontendEventStart, msg->getType(), AS_ECODE_DMAC_READ_ERROR);
      return;
    }

  /* State transit */

  m_state = MicFrontendStateActive;

  /* Reply*/

  reply(AsMicFrontendEventStart, msg->getType(), AS_ECODE_OK);
}

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::stopOnActive(MsgPacket *msg)
{
  msg->moveParam<MicFrontendCommand>();

  MIC_FRONTEND_DBG("STOP:\n");

  if (!setExternalCmd(AsMicFrontendEventStop))
    {
      reply(AsMicFrontendEventStop, msg->getType(), AS_ECODE_QUEUE_OPERATION_ERROR);
      return;
    }

  /* Stop DMA transfer. */

  CaptureComponentParam cap_comp_param;

  cap_comp_param.handle          = m_capture_hdlr;
  cap_comp_param.stop_param.mode = AS_DMASTOPMODE_NORMAL;

  AS_stop_capture(&cap_comp_param);

  /* State transit */

  m_state = MicFrontendStateStopping;
}

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::stopOnErrorStop(MsgPacket *msg)
{
  msg->moveParam<MicFrontendCommand>();

  MIC_FRONTEND_DBG("STOP:\n");

  /* If STOP command received during internal error stop sequence, enque command.
   * It will be trigger of transition to Ready when all of preproc is done.
   */

   if (!setExternalCmd(AsMicFrontendEventStop))
     {
       reply(AsMicFrontendEventStop, msg->getType(), AS_ECODE_QUEUE_OPERATION_ERROR);
       return;
     }
}

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::stopOnWait(MsgPacket *msg)
{
  msg->moveParam<MicFrontendCommand>();

  MIC_FRONTEND_DBG("STOP:\n");

  if (!m_capture_req && !m_preproc_req)
    {
      /* If all of capture and preproc request was returned,
       * reply and transit to Ready state.
       */

      reply(AsMicFrontendEventStop, msg->getType(), AS_ECODE_OK);

      m_state = MicFrontendStateReady;
    }
  else
    {
      /* If capture or preproc request remains, hold command. */

      if (!setExternalCmd(AsMicFrontendEventStop))
        {
          reply(AsMicFrontendEventStop, msg->getType(), AS_ECODE_QUEUE_OPERATION_ERROR);
          return;
        }
    }
}

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::initPreproc(MsgPacket *msg)
{
  AsInitPreProcParam initparam =
    msg->moveParam<MicFrontendCommand>().initpreproc_param;

  MIC_FRONTEND_DBG("Init Pre Proc:\n");

  InitCustomProcParam param;

  param.is_userdraw = true;
  param.packet.addr = initparam.packet_addr;
  param.packet.size = initparam.packet_size;

  /* Init Preproc (Copy packet to MH internally, and wait return from DSP) */

  bool send_result = m_p_preproc_instance->init(param);

  CustomProcCmpltParam cmplt;
  m_p_preproc_instance->recv_done(&cmplt);

  /* Reply */

  reply(AsMicFrontendEventInitPreProc, msg->getType(), send_result);
}

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::setPreproc(MsgPacket *msg)
{
  AsSetPreProcParam setparam =
    msg->moveParam<MicFrontendCommand>().setpreproc_param;

  MIC_FRONTEND_DBG("Set Pre Proc:\n");

  SetCustomProcParam param;

  param.is_userdraw = true;
  param.packet.addr = setparam.packet_addr;
  param.packet.size = setparam.packet_size;

  /* Set Preproc (Copy packet to MH internally) */

  bool send_result = m_p_preproc_instance->set(param);

  /* Reply (Don't wait reply from DSP because it will take long time) */

  reply(AsMicFrontendEventSetPreProc,
        msg->getType(),
        (send_result) ? AS_ECODE_OK : AS_ECODE_DSP_SET_ERROR);
}

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::setMicGain(MsgPacket *msg)
{
  AsMicFrontendMicGainParam recv_micgain =
    msg->moveParam<MicFrontendCommand>().mic_gain_param;

  MIC_FRONTEND_DBG("Set Mic Gain:\n");

  SetMicGainCaptureComponentParam set_micgain;

  for (int i = 0; i < MAX_CAPTURE_MIC_CH; i++)
    {
      if (i < AS_MIC_CHANNEL_MAX)
        {
          set_micgain.mic_gain[i] = recv_micgain.mic_gain[i];
        }
      else
        {
          set_micgain.mic_gain[i] = 0;
        }
    }

  CaptureComponentParam cap_comp_param;
  cap_comp_param.handle = m_capture_hdlr;
  cap_comp_param.set_micgain_param = &set_micgain;

  if (!AS_set_micgain_capture(&cap_comp_param))
    {
      reply(AsMicFrontendEventSetMicGain, msg->getType(), AS_ECODE_SET_MIC_GAIN_ERROR);
    }

  reply(AsMicFrontendEventSetMicGain, msg->getType(), AS_ECODE_OK);
}

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::illegalPreprocDone(MsgPacket *msg)
{
  msg->moveParam<MicFrontendObjPreProcDoneCmd>();

  /* Even if illegal reply, but need to do post handling same as usual.
   * Because allocated areas and queue for encodeing should be free.
   */

  if (m_p_preproc_instance)
    {
      m_p_preproc_instance->recv_done();
    }
}

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::preprocDoneOnActive(MsgPacket *msg)
{
  MicFrontendObjPreProcDoneCmd preproc_result =
    msg->moveParam<MicFrontendObjPreProcDoneCmd>();

  /* Check PreProcess instance exsits */

  if (!m_p_preproc_instance)
    {
      MIC_FRONTEND_ERR(AS_ATTENTION_SUB_CODE_RESOURCE_ERROR);
      return;
    }

  /* If it is not return of Exec of Flush, no need to proprocess. */

  if (!(preproc_result.event_type == CustomProcExec)
   && !(preproc_result.event_type == CustomProcFlush))
    {
      m_p_preproc_instance->recv_done();
      return;
    }

  /* Get prefilter result */

  CustomProcCmpltParam cmplt;

  m_p_preproc_instance->recv_done(&cmplt);

  /* Send to dest */

  sendData(cmplt.output);

  /* Decrement proproc request num */

  m_preproc_req--;
}

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::preprocDoneOnStop(MsgPacket *msg)
{
  MicFrontendObjPreProcDoneCmd preproc_result =
    msg->moveParam<MicFrontendObjPreProcDoneCmd>();

  /* Check PreProcess instance exsits */

  if (!m_p_preproc_instance)
    {
      MIC_FRONTEND_ERR(AS_ATTENTION_SUB_CODE_RESOURCE_ERROR);
      return;
    }

  /* If it is not return of Exec of Flush, no need to rendering. */

  if (!(preproc_result.event_type == CustomProcExec)
   && !(preproc_result.event_type == CustomProcFlush))
    {
      m_p_preproc_instance->recv_done();
      return;
    }

  /* Decrement proproc request num */

  m_preproc_req--;

  /* Get prefilter result */

  CustomProcCmpltParam cmplt;

  m_p_preproc_instance->recv_done(&cmplt);

  if (preproc_result.event_type == CustomProcExec)
    {
      if (cmplt.result && (cmplt.output.size > 0))
        {
          sendData(cmplt.output);
        }
    }
  else if (preproc_result.event_type == CustomProcFlush)
    {
      if (cmplt.result)
        {
          /* Add EndFrame mark to flush result. */

          cmplt.output.is_end = true;

          sendData(cmplt.output);

          /* Check external command */

          if (!m_capture_req && !m_preproc_req)
            {
              /* If all of capture and preproc request was returned,
               * Check exeternal command and transit to Ready if exist.
               */

              if (checkExternalCmd())
                {
                  AsMicFrontendEvent ext_cmd = getExternalCmd();

                  reply(ext_cmd, msg->getType(), AS_ECODE_OK);

                  m_state = MicFrontendStateReady;
                }
              else
                {
                  m_state = MicFrontendStateWaitStop;
                }
            }
          else
            {
              /* If capture or preproc request remains,
               * transit to WaitStop to avoid leak of MH.
               */

              m_state = MicFrontendStateWaitStop;
            }
        }
    }
  else
    {
      MIC_FRONTEND_ERR(AS_ATTENTION_SUB_CODE_UNEXPECTED_PARAM);
      return;
    }
}

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::preprocDoneOnErrorStop(MsgPacket *msg)
{
  preprocDoneOnStop(msg);
}

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::preprocDoneOnWaitStop(MsgPacket *msg)
{
  MicFrontendObjPreProcDoneCmd preproc_result =
    msg->moveParam<MicFrontendObjPreProcDoneCmd>();

  /* Check PreProcess instance exsits */

  if (!m_p_preproc_instance)
    {
      MIC_FRONTEND_ERR(AS_ATTENTION_SUB_CODE_RESOURCE_ERROR);
      return;
    }

  /* If it is not return of Exec of Flush, no need to rendering. */

  if (!(preproc_result.event_type == CustomProcExec)
   && !(preproc_result.event_type == CustomProcFlush))
    {
      m_p_preproc_instance->recv_done();
      return;
    }

  /* Decrement proproc request num */

  m_preproc_req--;

  /* Get prefilter result */

  m_p_preproc_instance->recv_done();

  /* If capture or preproc request remains,
   * don't ransit to Ready to avoid leak of MH.
   */

  if (!m_capture_req && !m_preproc_req)
    {
      if (checkExternalCmd())
        {
          AsMicFrontendEvent ext_cmd = getExternalCmd();
          reply(ext_cmd, msg->getType(), AS_ECODE_OK);

          m_state = MicFrontendStateReady;
        }
    }
}

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::illegalCaptureDone(MsgPacket *msg)
{
  msg->moveParam<CaptureDataParam>();
}

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::captureDoneOnActive(MsgPacket *msg)
{
  CaptureDataParam cap_rslt = msg->moveParam<CaptureDataParam>();
  CaptureComponentParam cap_comp_param;

  /* Decrement capture request num */

  m_capture_req--;

  /* Request next capture */

  cap_comp_param.handle                = m_capture_hdlr;
  cap_comp_param.exec_param.pcm_sample = m_samples_per_frame;

  bool result = AS_exec_capture(&cap_comp_param);
  if (result)
    {
      /* Increment capture request num */

      m_capture_req++;
    }

  /* Exec PreProcess */

  bool exec_result = (!cap_rslt.buf.validity)
                       ? false : execPreProc(cap_rslt.buf.cap_mh,
                                             cap_rslt.buf.sample);

  /* If exec failed, stop capturing */

  if (!exec_result)
    {
      cap_comp_param.handle          = m_capture_hdlr;
      cap_comp_param.stop_param.mode = AS_DMASTOPMODE_NORMAL;

      AS_stop_capture(&cap_comp_param);

      m_state = MicFrontendStateErrorStopping;
    }
}

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::captureDoneOnStop(MsgPacket *msg)
{
  CaptureDataParam cap_rslt = msg->moveParam<CaptureDataParam>();

  /* Decrement capture request num */

  m_capture_req--;

  /* Exec PreProcess */

  bool exec_result = (!cap_rslt.buf.validity)
                       ? false : execPreProc(cap_rslt.buf.cap_mh,
                                             cap_rslt.buf.sample);

  /* If exec failed, transit to ErrorStopping.
   * Here, capture stop request was alreay issued.
   */

  if (!exec_result)
    {
      m_state = MicFrontendStateErrorStopping;
    }

  /* Check endframe of capture */

  if (cap_rslt.end_flag)
    {
      bool stop_result = flushPreProc();

      /* If flush failed, reply of flush request will not be notified.
       * It means that cannot detect last frame of preproc.
       * So, here, transit to Ready state and dispose follow proproc data.
       */

      if (!stop_result)
        {
          /* Send dummy end frame */

          sendDummyEndData();

          if (!m_capture_req && !m_preproc_req)
            {
              /* Reply */

              AsMicFrontendEvent ext_cmd = getExternalCmd();
              reply(ext_cmd, msg->getType(), AS_ECODE_OK);

              /* Transit to Ready */

              m_state = MicFrontendStateReady;
            }
          else
            {
              /* If capture or preproc request remains,
               * transit to WaitStop to avoid leak of MH.
               */

              m_state = MicFrontendStateWaitStop;
            }
        }
    }
}

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::captureDoneOnErrorStop(MsgPacket *msg)
{
  CaptureDataParam cap_rslt = msg->moveParam<CaptureDataParam>();

  /* Decrement capture request num */

  m_capture_req--;

  /* In error stopping sequence, do not pre process because
   * it won't be sequencial data and cause noise at end of data.
   */

  /* Check end of capture */

  if (cap_rslt.end_flag)
    {
      bool stop_result = flushPreProc();

      /* If stop failed, reply of flush request will not be notified.
       * It means that cannot detect last frame of preproc.
       * So, here, transit to WaitStop state and dispose follow proproc data.
       */

      if (!stop_result)
        {
          /* Send dummy end frame */

          sendDummyEndData();

          /* Check exeternal command que, and reply if command exeits. */

          if (!m_capture_req && !m_preproc_req)
            {
              if (checkExternalCmd())
                {
                  AsMicFrontendEvent ext_cmd = getExternalCmd();
                  reply(ext_cmd, msg->getType(), AS_ECODE_OK);

                  m_state = MicFrontendStateReady;
                }
              else
                {
                  m_state = MicFrontendStateWaitStop;
                }
            }
          else
            {
              /* If capture or preproc request remains,
               * transit to WaitStop to avoid leak of MH.
               */

              m_state = MicFrontendStateWaitStop;
            }
        }
    }
}

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::captureDoneOnWaitStop(MsgPacket *msg)
{
  msg->moveParam<CaptureDataParam>();

  /* Decrement capture request num */

  m_capture_req--;

  if (!m_capture_req && !m_preproc_req)
    {
      if (checkExternalCmd())
        {
          AsMicFrontendEvent ext_cmd = getExternalCmd();
          reply(ext_cmd, msg->getType(), AS_ECODE_OK);

          m_state = MicFrontendStateReady;
        }
    }
}

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::illegalCaptureError(MsgPacket *msg)
{
  msg->moveParam<CaptureErrorParam>();
}

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::captureErrorOnActive(MsgPacket *msg)
{
  CaptureErrorParam param = msg->moveParam<CaptureErrorParam>();

  /* Stop capture input */

  CaptureComponentParam cap_comp_param;
  cap_comp_param.handle          = m_capture_hdlr;
  cap_comp_param.stop_param.mode = AS_DMASTOPMODE_NORMAL;

  AS_stop_capture(&cap_comp_param);

  if (param.error_type == CaptureErrorErrInt)
    {
      /* Flush PreProcess.
       * Because, the continuity of captured audio data will be lost when
       * ERRINT was occured. Therefore, do not excute subsequent encodings.
       * Instead of them, flush(stop) encoding right now.
       */

      bool stop_result = flushPreProc();

      if (!stop_result)
        {
          /* If flush failed, return from preprocess never returns.
           * So, Send dummy end frame, and tantist WaitStop state.
           */

          sendDummyEndData();

          m_state = MicFrontendStateWaitStop;
        }
      else
        {
          m_state = MicFrontendStateErrorStopping;
        }
    }
  else
    {
      /* Transit to ErrorStopping state. */

      m_state = MicFrontendStateErrorStopping;
    }
}

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::captureErrorOnStop(MsgPacket *msg)
{
  CaptureErrorParam param = msg->moveParam<CaptureErrorParam>();

  if (param.error_type == CaptureErrorErrInt)
    {
      /* Occurrence of ERRINT means that the end flame has not arrived yet
       * and it will never arrive. Therefore, flush preproc is not executed
       * yet and there is no trigger to execute in future.
       * Request flush preproc here.
       */

      bool stop_result = flushPreProc();

      if (!stop_result)
        {
          /* If flush failed, return from preprocess never returns.
           * So, Send dummy end frame, and tantist WaitStop or Ready state.
           */

          sendDummyEndData();

          if (!m_capture_req && !m_preproc_req)
            {
              if (checkExternalCmd())
                {
                  AsMicFrontendEvent ext_cmd = getExternalCmd();
                  reply(ext_cmd, msg->getType(), AS_ECODE_OK);

                  m_state = MicFrontendStateReady;
                }
              else
                {
                  m_state = MicFrontendStateWaitStop;
                }
            }
          else
            {
              /* If capture or preproc request remains,
               * transit to WaitStop to avoid leak of MH.
               */

              m_state = MicFrontendStateWaitStop;
            }
        }
      else
        {
          m_state = MicFrontendStateErrorStopping;
        }
    }
  else
    {
      /* Transit to ErrorStopping state. */

      m_state = MicFrontendStateErrorStopping;
    }
}

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::captureErrorOnErrorStop(MsgPacket *msg)
{
  /* Same as case of Stopping. */

  captureErrorOnStop(msg);
}

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::captureErrorOnWaitStop(MsgPacket *msg)
{
  msg->moveParam<CaptureErrorParam>();

  /* If already in wait stop sequence, there are nothing to do */
}

/*--------------------------------------------------------------------------*/
bool MicFrontEndObject::startCapture()
{
  bool result = true;

  /* Pre stock capture request to avoid underflow. */

  for (int i = 0; i < CAPTURE_PRESET_NUM; i++)
    {
      CaptureComponentParam cap_comp_param;

      cap_comp_param.handle                = m_capture_hdlr;
      cap_comp_param.exec_param.pcm_sample = m_samples_per_frame;

      result = AS_exec_capture(&cap_comp_param);
      if (!result)
        {
          break;
        }

      /* Increment capture request num */

      m_capture_req++;
    }

  return result;
}

/*--------------------------------------------------------------------------*/
bool MicFrontEndObject::setExternalCmd(AsMicFrontendEvent ext_event)
{
  if (!m_external_cmd_que.push(ext_event))
    {
      MIC_FRONTEND_ERR(AS_ATTENTION_SUB_CODE_QUEUE_PUSH_ERROR);
      return false;
    }

  return true;
}

/*--------------------------------------------------------------------------*/
AsMicFrontendEvent MicFrontEndObject::getExternalCmd(void)
{
  AsMicFrontendEvent ext_cmd = AsMicFrontendEventAct;

  if (m_external_cmd_que.empty())
    {
      MIC_FRONTEND_ERR(AS_ATTENTION_SUB_CODE_QUEUE_MISSING_ERROR);
    }
  else
    {
      ext_cmd = m_external_cmd_que.top();

      if (!m_external_cmd_que.pop())
        {
          MIC_FRONTEND_ERR(AS_ATTENTION_SUB_CODE_QUEUE_POP_ERROR);
        }
    }

  return ext_cmd;
}

/*--------------------------------------------------------------------------*/
uint32_t MicFrontEndObject::checkExternalCmd(void)
{
  return m_external_cmd_que.size();
}

/*--------------------------------------------------------------------------*/
MemMgrLite::MemHandle MicFrontEndObject::getOutputBufAddr()
{
  MemMgrLite::MemHandle mh;
  if (mh.allocSeg(m_pool_id.output, m_max_output_size) != ERR_OK)
    {
      MIC_FRONTEND_WARN(AS_ATTENTION_SUB_CODE_MEMHANDLE_ALLOC_ERROR);
    }

  return mh;
}

/*--------------------------------------------------------------------------*/
uint32_t MicFrontEndObject::activateParamCheck(
  const AsActivateFrontendParam &param)
{
  switch (param.input_device)
    {
      case AsMicFrontendDeviceMic:
        {
          cxd56_audio_micdev_t micdev = cxd56_audio_get_micdev();
          if (micdev == CXD56_AUDIO_MIC_DEV_ANALOG)
            {
              m_input_device = CaptureDeviceAnalogMic;
            }
          else
            {
              m_input_device = CaptureDeviceDigitalMic;
            }
        }
        break;

      default:
        MIC_FRONTEND_ERR(AS_ATTENTION_SUB_CODE_UNEXPECTED_PARAM);
        return AS_ECODE_COMMAND_PARAM_INPUT_DEVICE;
    }

  return AS_ECODE_OK;
}

/*--------------------------------------------------------------------------*/
uint32_t MicFrontEndObject::initParamCheck(const MicFrontendCommand& cmd)
{
  uint32_t rst = AS_ECODE_OK;

  /* Check number of channels */

  switch(cmd.init_param.channel_number)
    {
      case AS_CHANNEL_MONO:
      case AS_CHANNEL_STEREO:
      case AS_CHANNEL_4CH:
          break;

      case AS_CHANNEL_6CH:
      case AS_CHANNEL_8CH:
        if (m_input_device != CaptureDeviceDigitalMic)
          {
            MIC_FRONTEND_ERR(AS_ATTENTION_SUB_CODE_UNEXPECTED_PARAM);
            return AS_ECODE_COMMAND_PARAM_CHANNEL_NUMBER;
          }
          break;

      default:
        MIC_FRONTEND_ERR(AS_ATTENTION_SUB_CODE_UNEXPECTED_PARAM);
        return AS_ECODE_COMMAND_PARAM_CHANNEL_NUMBER;
    }

  /* Check bit length */

  switch(cmd.init_param.bit_length)
    {
      case AS_BITLENGTH_16:
      case AS_BITLENGTH_24:
      case AS_BITLENGTH_32:
        break;

      default:
        MIC_FRONTEND_ERR(AS_ATTENTION_SUB_CODE_UNEXPECTED_PARAM);
        return AS_ECODE_COMMAND_PARAM_BIT_LENGTH;
    }

  return rst;
}

/*--------------------------------------------------------------------------*/
bool MicFrontEndObject::execPreProc(MemMgrLite::MemHandle inmh, uint32_t sample)
{
  ExecCustomProcParam exec;

  exec.input.identifier = 0;
  exec.input.callback   = NULL;
  exec.input.mh         = inmh;
  exec.input.sample     = sample;
  exec.input.size       = m_channel_num * m_cap_bytes * sample;
  exec.input.is_end     = false;
  exec.input.is_valid   = true;

  /* If preprocess is not active, don't alloc output area. */

  if (m_preproc_type != AsMicFrontendPreProcThrough)
    {

      if (m_pool_id.output == MemMgrLite::NullPoolId)
        {
          /* For compatibility.
           * If null pool id is set, use same area as input.
           */

          exec.output_mh = exec.input.mh;
        }
      else
        {
          if (ERR_OK != exec.output_mh.allocSeg(m_pool_id.output,
                                                m_max_output_size))
            {
              MIC_FRONTEND_ERR(AS_ATTENTION_SUB_CODE_MEMHANDLE_ALLOC_ERROR);
              return false;
            }
        }
    }

  /* Exec PreProcess */

  if (!m_p_preproc_instance->exec(exec))
    {
      return false;
    }

  /* Increment proproc request num */

  m_preproc_req++;

  return true;
}

/*--------------------------------------------------------------------------*/
bool MicFrontEndObject::flushPreProc(void)
{
  FlushCustomProcParam flush;

  /* Set preprocess flush output area */

  if (m_preproc_type != AsMicFrontendPreProcThrough)
    {
      if (m_pool_id.output == MemMgrLite::NullPoolId)
        {
          /* For compatibility.
           * If null pool id is set, allocate on input area.
           */

          if (ERR_OK != flush.output_mh.allocSeg(m_pool_id.input,
                                                 m_max_capture_size))
            {
              MIC_FRONTEND_ERR(AS_ATTENTION_SUB_CODE_MEMHANDLE_ALLOC_ERROR);
              return false;
            }
        }
      else
        {
          if (ERR_OK != flush.output_mh.allocSeg(m_pool_id.output,
                                                 m_max_capture_size))
            {
              MIC_FRONTEND_ERR(AS_ATTENTION_SUB_CODE_MEMHANDLE_ALLOC_ERROR);
              return false;
            }
        }
    }

  if (!m_p_preproc_instance->flush(flush))
    {
      return false;
    }

  /* Increment proproc request num */

  m_preproc_req++;

  return true;
}

/*--------------------------------------------------------------------------*/
bool MicFrontEndObject::sendData(AsPcmDataParam& data)
{
  data.identifier = 0;
  data.callback   = pcm_send_done_callback;

  if (m_pcm_data_path == AsDataPathCallback)
    {
      /* Call callback function for PCM data notify */

      m_pcm_data_dest.cb(data);
    }
  else
    {
      /* Send message for PCM data notify */

      err_t er = MsgLib::send<AsPcmDataParam>(m_pcm_data_dest.msg.msgqid,
                                              MsgPriNormal,
                                              m_pcm_data_dest.msg.msgtype,
                                              m_msgq_id.micfrontend,
                                              data);
      F_ASSERT(er == ERR_OK);
    }

  return true;
}

/*--------------------------------------------------------------------------*/
bool MicFrontEndObject::sendDummyEndData(void)
{
  /* Send dummy end frame */

  AsPcmDataParam dmypcm = { 0 };
  dmypcm.size   = 0;
  dmypcm.is_end = true;

  return sendData(dmypcm);
}

/*--------------------------------------------------------------------------*/
bool MicFrontEndObject::getInputDeviceHdlr(void)
{
  if (m_capture_hdlr != MAX_CAPTURE_COMP_INSTANCE_NUM)
    {
      return false;
    }

  if (!AS_get_capture_comp_handler(&m_capture_hdlr,
                                   m_input_device,
                                   m_pool_id.input))
    {
      return false;
    }

  return true;
}

/*--------------------------------------------------------------------------*/
bool MicFrontEndObject::delInputDeviceHdlr(void)
{
  if (m_capture_hdlr != MAX_CAPTURE_COMP_INSTANCE_NUM)
    {
      if(!AS_release_capture_comp_handler(m_capture_hdlr))
        {
          return false;
        }

      m_capture_hdlr = MAX_CAPTURE_COMP_INSTANCE_NUM;
    }
  return true;
}

/*--------------------------------------------------------------------------*/
bool MicFrontEndObject::checkAndSetMemPool(void)
{
  /* Check capture in buffer pool */

  if (!MemMgrLite::Manager::isPoolAvailable(m_pool_id.input))
    {
      MIC_FRONTEND_ERR(AS_ATTENTION_SUB_CODE_MEMHANDLE_ALLOC_ERROR);
      return false;
    }

  m_max_capture_size = (MemMgrLite::Manager::getPoolSize(m_pool_id.input)) /
                       (MemMgrLite::Manager::getPoolNumSegs(m_pool_id.input));

  /* check output buffer pool */

  if (m_pool_id.output != MemMgrLite::NullPoolId)
    {
      if (!MemMgrLite::Manager::isPoolAvailable(m_pool_id.output))
        {
          MIC_FRONTEND_ERR(AS_ATTENTION_SUB_CODE_MEMHANDLE_ALLOC_ERROR);
          return false;
        }

      m_max_output_size = (MemMgrLite::Manager::getPoolSize(m_pool_id.output)) /
        (MemMgrLite::Manager::getPoolNumSegs(m_pool_id.output));
    }

  /* check DSP command buffer pool */

  if (!MemMgrLite::Manager::isPoolAvailable(m_pool_id.dsp))
    {
      MIC_FRONTEND_ERR(AS_ATTENTION_SUB_CODE_MEMHANDLE_ALLOC_ERROR);
      return false;
    }

  if ((int)(sizeof(Apu::Wien2ApuCmd)) >
      (MemMgrLite::Manager::getPoolSize(m_pool_id.dsp))/
      (MemMgrLite::Manager::getPoolNumSegs(m_pool_id.dsp)))
    {
      MIC_FRONTEND_ERR(AS_ATTENTION_SUB_CODE_MEMHANDLE_ALLOC_ERROR);
      return false;
    }

  return true;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/*--------------------------------------------------------------------------*/
bool AS_CreateMicFrontend(FAR AsCreateMicFrontendParam_t *param, AudioAttentionCb attcb)
{
  /* Register attention callback */

  MIC_FRONTEND_REG_ATTCB(attcb);

  /* Parameter check */

  if (param == NULL)
    {
      return false;
    }

  /* Create */

  s_msgq_id = param->msgq_id;
  s_pool_id = param->pool_id;

  /* Reset Message queue. */

  FAR MsgQueBlock *que;
  err_t err_code = MsgLib::referMsgQueBlock(s_msgq_id.micfrontend, &que);
  F_ASSERT(err_code == ERR_OK);
  que->reset();

  s_mfe_pid = task_create("FED_OBJ",
                          150, 1024 * 2,
                          AS_MicFrontendObjEntry,
                          NULL);
  if (s_mfe_pid < 0)
    {
      MIC_FRONTEND_ERR(AS_ATTENTION_SUB_CODE_TASK_CREATE_ERROR);
      return false;
    }

  return true;
}

/*--------------------------------------------------------------------------*/
bool AS_ActivateMicFrontend(FAR AsActivateMicFrontend *actparam)
{
  /* Parameter check */

  if (actparam == NULL)
    {
      return false;
    }

  /* Activate */

  MicFrontendCommand cmd;

  cmd.act_param = *actparam;

  err_t er = MsgLib::send<MicFrontendCommand>(s_msgq_id.micfrontend,
                                              MsgPriNormal,
                                              MSG_AUD_MFE_CMD_ACT,
                                              NULL,
                                              cmd);
  F_ASSERT(er == ERR_OK);

  return true;
}

/*--------------------------------------------------------------------------*/
bool AS_InitMicFrontend(FAR AsInitMicFrontendParam *initparam)
{
  /* Parameter check */

  if (initparam == NULL)
    {
      return false;
    }

  /* Init */

  MicFrontendCommand cmd;

  cmd.init_param = *initparam;

  err_t er = MsgLib::send<MicFrontendCommand>(s_msgq_id.micfrontend,
                                              MsgPriNormal,
                                              MSG_AUD_MFE_CMD_INIT,
                                              NULL,
                                              cmd);
  F_ASSERT(er == ERR_OK);

  return true;
}

/*--------------------------------------------------------------------------*/
bool AS_StartMicFrontend(FAR AsStartMicFrontendParam *startparam)
{
  /* Parameter check */

  if (startparam == NULL)
    {
      return false;
    }

  /* Start */

  MicFrontendCommand cmd;

  cmd.start_param = *startparam;

  err_t er = MsgLib::send<MicFrontendCommand>(s_msgq_id.micfrontend,
                                              MsgPriNormal,
                                              MSG_AUD_MFE_CMD_START,
                                              NULL,
                                              cmd);
  F_ASSERT(er == ERR_OK);

  return true;
}

/*--------------------------------------------------------------------------*/
bool AS_StopMicFrontend(FAR AsStopMicFrontendParam *stopparam)
{
  /* Parameter check */

  if (stopparam == NULL)
    {
      return false;
    }

  /* Stop */

  MicFrontendCommand cmd;

  cmd.stop_param = *stopparam;

  err_t er = MsgLib::send<MicFrontendCommand>(s_msgq_id.micfrontend,
                                              MsgPriNormal,
                                              MSG_AUD_MFE_CMD_STOP,
                                              NULL,
                                              cmd);
  F_ASSERT(er == ERR_OK);

  return true;
}

/*--------------------------------------------------------------------------*/
bool AS_InitPreprocFrontend(FAR AsInitPreProcParam *initpreparam)
{
  /* Parameter check */

  if (initpreparam == NULL)
    {
      return false;
    }

  /* Init PreProcess */

  MicFrontendCommand cmd;

  cmd.initpreproc_param = *initpreparam;

  err_t er = MsgLib::send<MicFrontendCommand>(s_msgq_id.micfrontend,
                                              MsgPriNormal,
                                              MSG_AUD_MFE_CMD_INITPREPROC,
                                              NULL,
                                              cmd);
  F_ASSERT(er == ERR_OK);

  return true;
}

/*--------------------------------------------------------------------------*/
bool AS_SetPreprocMicFrontend(FAR AsSetPreProcParam *setpreparam)
{
  /* Parameter check */

  if (setpreparam == NULL)
    {
      return false;
    }

  /* Set PreProcess */

  MicFrontendCommand cmd;

  cmd.setpreproc_param = *setpreparam;

  err_t er = MsgLib::send<MicFrontendCommand>(s_msgq_id.micfrontend,
                                              MsgPriNormal,
                                              MSG_AUD_MFE_CMD_SETPREPROC,
                                              NULL,
                                              cmd);

  F_ASSERT(er == ERR_OK);

  return true;
}

/*--------------------------------------------------------------------------*/
bool AS_SetMicGainMicFrontend(FAR AsMicFrontendMicGainParam *micgain_param)
{
  if (micgain_param == NULL)
    {
      return false;
    }

  MicFrontendCommand cmd;

  cmd.mic_gain_param = *micgain_param;

  err_t er = MsgLib::send<MicFrontendCommand>(s_msgq_id.micfrontend,
                                              MsgPriNormal,
                                              MSG_AUD_MFE_CMD_SETMICGAIN,
                                              NULL,
                                              cmd);
  F_ASSERT(er == ERR_OK);

  return true;
}

/*--------------------------------------------------------------------------*/
bool AS_DeactivateMicFrontend(FAR AsDeactivateMicFrontendParam *deactparam)
{
  MicFrontendCommand cmd;

  err_t er = MsgLib::send<MicFrontendCommand>(s_msgq_id.micfrontend,
                                              MsgPriNormal,
                                              MSG_AUD_MFE_CMD_DEACT,
                                              NULL,
                                              cmd);
  F_ASSERT(er == ERR_OK);

  return true;
}

/*--------------------------------------------------------------------------*/
bool AS_DeleteMicFrontend(void)
{
  if (s_mfe_obj == NULL)
    {
      MIC_FRONTEND_ERR(AS_ATTENTION_SUB_CODE_TASK_CREATE_ERROR);
      return false;
    }

  task_delete(s_mfe_pid);
  delete s_mfe_obj;
  s_mfe_obj = NULL;

  /* Unregister attention callback */

  MIC_FRONTEND_UNREG_ATTCB();

  return true;
}

/*--------------------------------------------------------------------------*/
bool AS_checkAvailabilityMicFrontend(void)
{
  return (s_mfe_obj != NULL);
}

/*--------------------------------------------------------------------------*/
void MicFrontEndObject::create(AsMicFrontendMsgQueId_t msgq_id,
                               AsMicFrontendPoolId_t pool_id)
{
  if (s_mfe_obj == NULL)
    {
      s_mfe_obj = new MicFrontEndObject(msgq_id,
                                        pool_id);
      s_mfe_obj->run();
    }
  else
    {
      MIC_FRONTEND_ERR(AS_ATTENTION_SUB_CODE_RESOURCE_ERROR);
      return;
    }
}

