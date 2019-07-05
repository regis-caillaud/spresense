/****************************************************************************
 * modules/lte/altcom/api/lte/lte_activate_pdn.c
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "lte/lte_api.h"
#include "buffpoolwrapper.h"
#include "evthdlbs.h"
#include "altcom_callbacks.h"
#include "altcombs.h"
#include "apicmdhdlrbs.h"
#include "apiutil.h"
#include "apicmd_dataallow.h"
#include "lte_dataallow.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define DATAALLOW_DATA_LEN (sizeof(struct apicmd_cmddat_dataallow_s))

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: dataallow_status_chg_cb
 *
 * Description:
 *   Notification status change in processing data allow.
 *
 * Input Parameters:
 *  new_stat    Current status.
 *  old_stat    Preview status.
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static int32_t dataallow_status_chg_cb(int32_t new_stat, int32_t old_stat)
{
  if (new_stat < ALTCOM_STATUS_POWER_ON)
    {
      DBGIF_LOG2_INFO("dataallow_status_chg_cb(%d -> %d)\n",
        old_stat, new_stat);
      altcomcallbacks_unreg_cb(APICMDID_DATA_ALLOW);

      return ALTCOM_STATUS_REG_CLR;
    }

  return ALTCOM_STATUS_REG_KEEP;
}

/****************************************************************************
 * Name: dataallow_check_param
 *
 * Description:
 *   Check data allow input parameter.
 *
 * Input Parameters:
 *   session_id      Target connection id.
 *   allow           General data communication allow.
 *   roamallow       Roaming data communication allow.
 *
 * Returned Value:
 *   On success, 0 is returned.
 *   On failure, negative value is returned.
 *
 ****************************************************************************/

static int32_t dataallow_check_param(uint8_t session_id,
  uint8_t allow, uint8_t roamallow)
{
  if (LTE_PDN_SESSIONID_MIN > session_id ||
     LTE_PDN_SESSIONID_MAX < session_id)
    {
      return -EINVAL;
    }

  if (LTE_DATA_ALLOW != allow &&
     LTE_DATA_DISALLOW != allow)
    {
      return -EINVAL;
    }

  if (LTE_DATA_ALLOW != roamallow &&
     LTE_DATA_DISALLOW != roamallow)
    {
      return -EINVAL;
    }

  return 0;
}

/****************************************************************************
 * Name: dataallow_job
 *
 * Description:
 *   This function is an API callback for data allow.
 *
 * Input Parameters:
 *  arg    Pointer to received event.
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void dataallow_job(FAR void *arg)
{
  int32_t                                 ret;
  FAR struct apicmd_cmddat_dataallowres_s *data;
  data_allow_cb_t                         callback;
  uint32_t                                result;

  data = (FAR struct apicmd_cmddat_dataallowres_s *)arg;
  ret = altcomcallbacks_get_unreg_cb(APICMDID_DATA_ALLOW,
    (void **)&callback);

  if ((0 != ret) || (!callback))
    {
      DBGIF_LOG_ERROR("Unexpected!! callback is NULL.\n");
    }
  else
    {
      result =
        data->result == APICMD_DATAALLOW_RES_OK ? LTE_RESULT_OK :
        LTE_RESULT_ERROR;

      callback(result);
    }

  /* In order to reduce the number of copies of the receive buffer,
   * bring a pointer to the receive buffer to the worker thread.
   * Therefore, the receive buffer needs to be released here. */

  altcom_free_cmd((FAR uint8_t *)arg);

  /* Unregistration status change callback. */

  altcomstatus_unreg_statchgcb(dataallow_status_chg_cb);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: lte_data_allow
 *
 * Description:
 *   Change configration of data communication allow.
 *
 * Input Parameters:
 *   session_id        Target connection id.
 *   allow             General data communication allow.
 *   roaming_allow     Roaming data communication allow.
 *   callback          Callback function to notify that
 *                     change data allow completed.
 *
 * Returned Value:
 *   On success, 0 is returned.
 *   On failure, negative value is returned.
 *
 ****************************************************************************/

int32_t lte_data_allow(uint8_t session_id, uint8_t allow,
                       uint8_t roaming_allow, data_allow_cb_t callback)
{
  int32_t                                ret;
  FAR struct apicmd_cmddat_dataallow_s *cmdbuff;

  /* Return error if callback is NULL */

  if (!callback)
    {
      DBGIF_LOG_ERROR("Input argument is NULL.\n");
      return -EINVAL;
    }

  /* Check input parameter */

  ret = dataallow_check_param(session_id, allow, roaming_allow);
  if (0 > ret)
    {
      return ret;
    }

  /* Check Lte library status */

  ret = altcombs_check_poweron_status();
  if (0 > ret)
    {
      return ret;
    }

  /* Register API callback */

  ret = altcomcallbacks_chk_reg_cb((void *)callback, APICMDID_DATA_ALLOW);
  if (0 > ret)
    {
      DBGIF_LOG_ERROR("Currently API is busy.\n");
      return -EINPROGRESS;
    }

  ret = altcomstatus_reg_statchgcb(dataallow_status_chg_cb);
  if (0 > ret)
    {
      DBGIF_LOG_ERROR("Failed to registration status change callback.\n");
      altcomcallbacks_unreg_cb(APICMDID_DATA_ALLOW);
      return ret;
    }

  /* Accept the API
   * Allocate API command buffer to send */

  cmdbuff = (FAR struct apicmd_cmddat_dataallow_s *)
    apicmdgw_cmd_allocbuff(APICMDID_DATA_ALLOW, DATAALLOW_DATA_LEN);
  if (!cmdbuff)
    {
      DBGIF_LOG_ERROR("Failed to allocate command buffer.\n");
      ret = -ENOMEM;
    }
  else
    {
      /* Fill parameter. */

      cmdbuff->session_id = session_id;
      cmdbuff->data_allow = allow == LTE_DATA_ALLOW ?
        APICMD_DATAALLOW_DATAALLOW_ALLOW :
        APICMD_DATAALLOW_DATAALLOW_DISALLOW;
      cmdbuff->dataroam_allow = allow == LTE_DATA_ALLOW ?
        APICMD_DATAALLOW_DATAROAMALLOW_ALLOW :
        APICMD_DATAALLOW_DATAROAMALLOW_DISALLOW;

      /* Send API command to modem */

      ret = altcom_send_and_free((uint8_t *)cmdbuff);
    }

  /* If fail, there is no opportunity to execute the callback,
   * so clear it here. */

  if (0 > ret)
    {
      /* Clear registered callback */

      altcomcallbacks_unreg_cb(APICMDID_DATA_ALLOW);
      altcomstatus_unreg_statchgcb(dataallow_status_chg_cb);
    }
  else
    {
      ret = 0;
    }

  return ret;
}

/****************************************************************************
 * Name: apicmdhdlr_dataallow
 *
 * Description:
 *   This function is an API command handler for data allow result.
 *
 * Input Parameters:
 *  evt    Pointer to received event.
 *  evlen  Length of received event.
 *
 * Returned Value:
 *   If the API command ID matches APICMDID_DATA_ALLOW_RES,
 *   EVTHDLRC_STARTHANDLE is returned.
 *   Otherwise it returns EVTHDLRC_UNSUPPORTEDEVENT. If an internal error is
 *   detected, EVTHDLRC_INTERNALERROR is returned.
 *
 ****************************************************************************/

enum evthdlrc_e apicmdhdlr_dataallow(FAR uint8_t *evt, uint32_t evlen)
{
  return apicmdhdlrbs_do_runjob(evt,
    APICMDID_CONVERT_RES(APICMDID_DATA_ALLOW), dataallow_job);
}
