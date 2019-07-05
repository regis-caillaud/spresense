/****************************************************************************
 * modules/lte/altcom/api/altcombs.c
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

#include <errno.h>
#include <string.h>

#include "lte/lte_api.h"
#include "dbg_if.h"
#include "buffpoolwrapper.h"
#include "altcombs.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define ALTCOMBS_EDRX_ACTTYPE_INVALID_VAL (0xFF)
#define ALTCOMBS_EDRX_ACTTYPE_MIN         (APICMD_EDRX_ACTTYPE_WBS1)
#define ALTCOMBS_EDRX_ACTTYPE_MAX         (APICMD_EDRX_ACTTYPE_WBS1)
#define ALTCOMBS_EDRX_CYCLE_MIN           (APICMD_EDRX_CYC_512)
#define ALTCOMBS_EDRX_CYCLE_MAX           (APICMD_EDRX_CYC_262144)
#define ALTCOMBS_EDRX_PTW_MIN             (APICMD_EDRX_PTW_128)
#define ALTCOMBS_EDRX_PTW_MAX             (APICMD_EDRX_PTW_2048)
#define ALTCOMBS_PSM_UNIT_T3324_MIN       (APICMD_PSM_RAT_UNIT_2SEC)
#define ALTCOMBS_PSM_UNIT_T3324_MAX       (APICMD_PSM_RAT_UNIT_6MIN)
#define ALTCOMBS_PSM_UNIT_T3412_MIN       (APICMD_PSM_TAU_UNIT_2SEC)
#define ALTCOMBS_PSM_UNIT_T3412_MAX       (APICMD_PSM_TAU_UNIT_320HOUR)

/****************************************************************************
 * Private Data
 ****************************************************************************/

static lte_errinfo_t g_errinfo = { 0, 0, 0, ""};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: altcombs_alloc_cbblock
 *
 * Description:
 *   Allocation callback block.
 *
 * Input Parameters:
 *   id  Callback block id.
 *   cb  Pointer to callback.
 *
 * Returned Value:
 *   If the process succeeds, it returns allocated callback block.
 *   Otherwise NULL is returned.
 *
 ****************************************************************************/

static FAR struct altcombs_cb_block *altcombs_alloc_cbblock(int32_t id,
                                                            FAR void *cb)
{
  FAR struct altcombs_cb_block *block;

  if (!cb)
    {
      return NULL;
    }

  block = (FAR struct altcombs_cb_block *)BUFFPOOL_ALLOC(
            sizeof(struct altcombs_cb_block));
  if (!block)
    {
      DBGIF_LOG_ERROR("Failed to allocate memory\n");
      return NULL;
    }

  block->id      = id;
  block->cb      = cb;
  block->removal = false;
  block->prev    = NULL;
  block->next    = NULL;

  return block;
}

/****************************************************************************
 * Name: altcombs_free_cbblock
 *
 * Description:
 *   Free callback block.
 *
 * Input Parameters:
 *   cb_block   Pointer to callback block.
 *
 * Returned Value:
 *   If the process succeeds, it returns 0.
 *   Otherwise negative value is returned.
 *
 ****************************************************************************/

static int32_t altcombs_free_cbblock(FAR struct altcombs_cb_block *cb_block)
{
  if (!cb_block)
    {
      return -EINVAL;
    }

  (void)BUFFPOOL_FREE(cb_block);

  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: altcombs_add_cbblock
 *
 * Description:
 *   Add callback block to the end of list.
 *
 * Input Parameters:
 *   head  Pointer of callback block list head.
 *   id    Callback block id.
 *   cb    Pointer to callback.
 *
 * Returned Value:
 *   If the process succeeds, it returns 0.
 *   Otherwise negative value is returned.
 *
 ****************************************************************************/

int32_t altcombs_add_cbblock(FAR struct altcombs_cb_block **head,
                             int32_t                        id,
                             FAR void                      *cb)
{
  FAR struct altcombs_cb_block *block;
  FAR struct altcombs_cb_block *curr_block;

  if ((!cb) || (!head))
    {
      return -EINVAL;
    }

  block = altcombs_alloc_cbblock(id, cb);
  if (!block)
    {
      return -ENOMEM;
    }

  curr_block = *head;

  if (*head)
    {
      /* Search end of block */

      while(curr_block->next)
        {
          curr_block = curr_block->next;
        }
      curr_block->next = block;
      block->prev      = curr_block;
    }
  else
    {
      *head = block;
    }

  return 0;
}

/****************************************************************************
 * Name: altcombs_remove_cbblock
 *
 * Description:
 *   Delete callback block from callback block list.
 *
 * Input Parameters:
 *   head   Pointer to callback block list head.
 *   block  Pointer to callback block.
 *
 * Returned Value:
 *   If the process succeeds, it returns 0.
 *   Otherwise negative value is returned.
 *
 ****************************************************************************/

int32_t altcombs_remove_cbblock(FAR struct altcombs_cb_block **head,
                                FAR struct altcombs_cb_block  *block)
{
  FAR struct altcombs_cb_block *curr_block = *head;

  if (!block)
    {
      return -EINVAL;
    }

  while (curr_block)
    {
      if (curr_block->cb == block->cb)
        {
          /* Check begining of the list */

          if (curr_block->prev)
            {
              curr_block->prev->next = curr_block->next;
              if(curr_block->next)
                {
                  curr_block->next->prev = curr_block->prev;
                }
            }
          else
            {
              *head = curr_block->next;
              if(curr_block->next)
                {
                  curr_block->next->prev = NULL;
                }
            }
          altcombs_free_cbblock(curr_block);

          return 0;
        }
      curr_block = curr_block->next;
    }

  return -EINVAL;
}

/****************************************************************************
 * Name: altcombs_search_cbblock
 *
 * Description:
 *   Search callback block by callback id.
 *
 * Input Parameters:
 *   head  Pointer to callback block list head.
 *   id    Target callback block id.
 *
 * Returned Value:
 *   If list is found, return that pointer.
 *   Otherwise NULL is returned.
 *
 ****************************************************************************/

FAR struct altcombs_cb_block *altcombs_search_cbblock(
  FAR struct altcombs_cb_block *head, int32_t id)
{
  FAR struct altcombs_cb_block *curr_block = head;

  while (curr_block)
    {
      if (curr_block->id == id)
        {
          return curr_block;
        }
      curr_block = curr_block->next;
    }

  return NULL;
}

/****************************************************************************
 * Name: altcombs_search_cbblock_bycb
 *
 * Description:
 *   Search callback block by callback pointer.
 *
 * Input Parameters:
 *   head  Pointer to callback block list head.
 *   cb    Pointer to callback.
 *
 * Returned Value:
 *   If list is found, return that pointer.
 *   Otherwise NULL is returned.
 *
 ****************************************************************************/

FAR struct altcombs_cb_block *altcombs_search_cbblock_bycb(
  FAR struct altcombs_cb_block *head, FAR void *cb)
{
  FAR struct altcombs_cb_block *curr_block = head;

  while (curr_block)
    {
      if (curr_block->cb == cb)
        {
          return curr_block;
        }
      curr_block = curr_block->next;
    }

  return NULL;
}

/****************************************************************************
 * Name: altcombs_mark_removal_cbblock
 *
 * Description:
 *   Mark as removal callback block.
 *
 * Input Parameters:
 *   block  Pointer to callback block.
 *
 * Returned Value:
 *   If the process succeeds, it returns 0.
 *   Otherwise negative value is returned.
 *
 ****************************************************************************/

int32_t altcombs_mark_removal_cbblock(FAR struct altcombs_cb_block *block)
{
  if (!block)
    {
      return -EINVAL;
    }

  block->removal = true;

  return 0;
}

/****************************************************************************
 * Name: altcombs_remove_removal_cbblock
 *
 * Description:
 *   Delete callback block marked for removal.
 *
 * Input Parameters:
 *   head   Pointer to callback block list head.
 *
 * Returned Value:
 *   If the process succeeds, it returns 0.
 *   Otherwise negative value is returned.
 *
 ****************************************************************************/

int32_t altcombs_remove_removal_cbblock(FAR struct altcombs_cb_block **head)
{
  FAR struct altcombs_cb_block *curr_block = *head;

  while (curr_block)
    {
      if (curr_block->removal)
        {
          /* Check begining of the list */

          if (curr_block->prev)
            {
              curr_block->prev->next = curr_block->next;
              if(curr_block->next)
                {
                  curr_block->next->prev = curr_block->prev;
                }
            }
          else
            {
              *head = curr_block->next;
              if(curr_block->next)
                {
                  curr_block->next->prev = NULL;
                }
            }
          altcombs_free_cbblock(curr_block);
        }
      curr_block = curr_block->next;
    }

  return 0;
}

/****************************************************************************
 * Name: altcombs_get_next_cbblock
 *
 * Description:
 *   Get pointer to next callback block.
 *
 * Input Parameters:
 *   block  Pointer to callback block.
 *
 * Returned Value:
 *   If next is found, return that pointer.
 *   Otherwise NULL is returned.
 *
 ****************************************************************************/

FAR struct altcombs_cb_block *altcombs_get_next_cbblock(
  FAR struct altcombs_cb_block *block)
{
  if (!block)
    {
      return NULL;
    }

  return block->next;
}

/****************************************************************************
 * Name: altcombs_set_errinfo
 *
 * Description:
 *   Get LTE API last error information.
 *
 * Input Parameters:
 *   info    Pointer of LTE error information.
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

void altcombs_set_errinfo(FAR lte_errinfo_t *info)
{
  if (info)
    {
      memcpy(&g_errinfo, info, sizeof(lte_errinfo_t));
    }
}

/****************************************************************************
 * Name: altcombs_get_errinfo
 *
 * Description:
 *   Get LTE API last error information.
 *
 * Input Parameters:
 *   info    Pointer of LTE error information.
 *
 * Returned Value:
 *   When get success is returned 0.
 *   When get failed return negative value.
 *
 ****************************************************************************/

int32_t altcombs_get_errinfo(FAR lte_errinfo_t *info)
{
  if (!info)
    {
      return -EINVAL;
    }

  memcpy(info, &g_errinfo, sizeof(lte_errinfo_t));
  return 0;
}

/****************************************************************************
 * Name: altcombs_set_pdninfo
 *
 * Description:
 *   Set lte_pdn_t param.
 *
 * Input Parameters:
 *   cmd_pdn    Pointer of api command pdn struct.
 *   lte_pdn    Pointer of lte_pdn_t.
 *
 * Returned Value:
 *   When convert success is returned 0.
 *   When convert failed return negative value.
 *
 ****************************************************************************/

int32_t altcombs_set_pdninfo(struct apicmd_pdnset_s *cmd_pdn,
  lte_pdn_t *lte_pdn)
{
  int32_t i;

  if (!cmd_pdn || !lte_pdn)
    {
      return -EINVAL;
    }

  lte_pdn->session_id = cmd_pdn->session_id;
  lte_pdn->active = cmd_pdn->activate;
  lte_pdn->apn_type = htonl(cmd_pdn->apntype);
  lte_pdn->ipaddr_num = cmd_pdn->ipaddr_num;
  for (i = 0; i < lte_pdn->ipaddr_num; i++)
    {
      lte_pdn->address[i].ip_type = cmd_pdn->ip_address[i].iptype;
      strncpy((FAR char *)lte_pdn->address[i].address,
              (FAR char *)cmd_pdn->ip_address[i].address,
              LTE_IPADDR_MAX_LEN - 1);
    }

  lte_pdn->ims_register = cmd_pdn->imsregister == APICMD_PDN_IMS_REG ?
    LTE_IMS_REGISTERED : LTE_IMS_NOT_REGISTERED;
  lte_pdn->data_allow = cmd_pdn->dataallow ==
    APICMD_PDN_DATAALLOW_ALLOW ?
    LTE_DATA_ALLOW : LTE_DATA_DISALLOW;
  lte_pdn->data_roaming_allow = cmd_pdn->dararoamingallow ==
    APICMD_PDN_DATAROAMALLOW_ALLOW ?
    LTE_DATA_ALLOW : LTE_DATA_DISALLOW;

  return 0;
}

/****************************************************************************
 * Name: altcombs_check_edrx
 *
 * Description:
 *   Check api comand eDRX param.
 *
 * Input Parameters:
 *   set    Pointer of api command eDRX struct.
 *
 * Returned Value:
 *   When check success is returned 0.
 *   When check failed return negative value.
 *
 ****************************************************************************/

int32_t altcombs_check_edrx(struct apicmd_edrxset_s *set)
{
  if (!set)
    {
      DBGIF_LOG_ERROR("null param\n");
      return -EINVAL;
    }

  if (set->enable < APICMD_EDRX_DISABLE ||
      set->enable > APICMD_EDRX_ENABLE)
    {
      DBGIF_LOG1_ERROR("Invalid enable :%d\n", set->enable);
      return -EINVAL;
    }

  if (APICMD_EDRX_ENABLE == set->enable)
    {
      if (set->acttype < APICMD_EDRX_ACTTYPE_NOTUSE ||
          set->acttype > APICMD_EDRX_ACTTYPE_NBS1)
        {
          DBGIF_LOG1_ERROR("Invalid acttype :%d\n", set->acttype);
          return -EINVAL;
        }

      if (set->edrx_cycle < APICMD_EDRX_CYC_512 ||
          set->edrx_cycle > APICMD_EDRX_CYC_262144)
        {
          DBGIF_LOG1_ERROR("Invalid cycle :%d\n", set->edrx_cycle);
          return -EINVAL;
        }

      if (set->ptw_val < APICMD_EDRX_PTW_128 ||
          set->ptw_val > APICMD_EDRX_PTW_2048)
        {
          DBGIF_LOG1_ERROR("Invalid PTW :%d\n", set->ptw_val);
          return -EINVAL;
        }
    }

  return 0;
}

/****************************************************************************
 * Name: altcombs_set_edrx
 *
 * Description:
 *   Set lte_edrx_setting_t param.
 *
 * Input Parameters:
 *   cmd_edrx    Pointer of api command edrx struct.
 *   lte_edrx    Pointer of lte_edrx_setting_t.
 *
 * Returned Value:
 *   When set success is returned 0.
 *   When set failed return negative value.
 *
 ****************************************************************************/

int32_t altcombs_set_edrx(struct apicmd_edrxset_s *cmd_edrx,
  lte_edrx_setting_t *lte_edrx)
{
  uint8_t edrx_acttype_table[] =
    {
      ALTCOMBS_EDRX_ACTTYPE_INVALID_VAL,
      ALTCOMBS_EDRX_ACTTYPE_INVALID_VAL,
      ALTCOMBS_EDRX_ACTTYPE_INVALID_VAL,
      ALTCOMBS_EDRX_ACTTYPE_INVALID_VAL,
      LTE_EDRX_ACTTYPE_WBS1,
    };

  uint8_t edrx_cycle_table[] =
    {
      LTE_EDRX_CYC_512,
      LTE_EDRX_CYC_1024,
      LTE_EDRX_CYC_2048,
      LTE_EDRX_CYC_4096,
      LTE_EDRX_CYC_6144,
      LTE_EDRX_CYC_8192,
      LTE_EDRX_CYC_10240,
      LTE_EDRX_CYC_12288,
      LTE_EDRX_CYC_14336,
      LTE_EDRX_CYC_16384,
      LTE_EDRX_CYC_32768,
      LTE_EDRX_CYC_65536,
      LTE_EDRX_CYC_131072,
      LTE_EDRX_CYC_262144,
    };

  uint8_t edrx_ptw_table[] =
    {
      LTE_EDRX_PTW_128,
      LTE_EDRX_PTW_256,
      LTE_EDRX_PTW_384,
      LTE_EDRX_PTW_512,
      LTE_EDRX_PTW_640,
      LTE_EDRX_PTW_768,
      LTE_EDRX_PTW_896,
      LTE_EDRX_PTW_1024,
      LTE_EDRX_PTW_1152,
      LTE_EDRX_PTW_1280,
      LTE_EDRX_PTW_1408,
      LTE_EDRX_PTW_1536,
      LTE_EDRX_PTW_1664,
      LTE_EDRX_PTW_1792,
      LTE_EDRX_PTW_1920,
      LTE_EDRX_PTW_2048,
    };

  if (!cmd_edrx || !lte_edrx)
    {
      return -EINVAL;
    }

  if (APICMD_EDRX_ENABLE == cmd_edrx->enable)
    {
      if ((ALTCOMBS_EDRX_ACTTYPE_MIN > cmd_edrx->acttype ||
          ALTCOMBS_EDRX_ACTTYPE_MAX < cmd_edrx->acttype) ||
          (ALTCOMBS_EDRX_CYCLE_MIN > cmd_edrx->edrx_cycle ||
          ALTCOMBS_EDRX_CYCLE_MAX < cmd_edrx->edrx_cycle) ||
          (ALTCOMBS_EDRX_PTW_MIN > cmd_edrx->ptw_val ||
          ALTCOMBS_EDRX_PTW_MAX < cmd_edrx->ptw_val))
        {
          return -EINVAL;
        }

      lte_edrx->enable = LTE_ENABLE;
      lte_edrx->act_type = edrx_acttype_table[cmd_edrx->acttype];
      lte_edrx->edrx_cycle = edrx_cycle_table[cmd_edrx->edrx_cycle];
      lte_edrx->ptw_val = edrx_ptw_table[cmd_edrx->ptw_val];
    }
  else
    {
      lte_edrx->enable = LTE_DISABLE;
    }

  return 0;
}

/****************************************************************************
 * Name: altcombs_check_psm
 *
 * Description:
 *   Check api comand PSM param.
 *
 * Input Parameters:
 *   set    Pointer of api command PSM struct.
 *
 * Returned Value:
 *   When check success is returned 0.
 *   When check failed return negative value.
 *
 ****************************************************************************/

int32_t altcombs_check_psm(struct apicmd_cmddat_psm_set_s *set)
{
  if (!set)
    {
      DBGIF_LOG_ERROR("null param\n");
      return -EINVAL;
    }

  if (set->enable < APICMD_PSM_DISABLE ||
      set->enable > APICMD_PSM_ENABLE)
    {
      DBGIF_LOG1_ERROR("Invalid enable :%d\n", set->enable);
      return -EINVAL;
    }

  if (APICMD_PSM_ENABLE == set->enable)
    {
      if (set->rat_time.unit < APICMD_PSM_RAT_UNIT_2SEC ||
          set->rat_time.unit > APICMD_PSM_RAT_UNIT_6MIN)
        {
          DBGIF_LOG1_ERROR("Invalid rat_time unit :%d\n", set->rat_time.unit);
          return -EINVAL;
        }

      if (set->rat_time.time_val < APICMD_PSM_TIMER_MIN ||
          set->rat_time.time_val > APICMD_PSM_TIMER_MAX)
        {
          DBGIF_LOG1_ERROR("Invalid rat_time time_val :%d\n", set->rat_time.time_val);
          return -EINVAL;
        }

      if (set->tau_time.unit < APICMD_PSM_TAU_UNIT_2SEC ||
          set->tau_time.unit > APICMD_PSM_TAU_UNIT_320HOUR)
        {
          DBGIF_LOG1_ERROR("Invalid tau_time unit :%d\n", set->tau_time.unit);
          return -EINVAL;
        }

      if (set->tau_time.time_val < APICMD_PSM_TIMER_MIN ||
          set->tau_time.time_val > APICMD_PSM_TIMER_MAX)
        {
          DBGIF_LOG1_ERROR("Invalid tau_time time_val :%d\n", set->tau_time.time_val);
          return -EINVAL;
        }
    }

  return 0;
}

/****************************************************************************
 * Name: altcombs_set_psm
 *
 * Description:
 *   Set lte_psm_setting_t param.
 *
 * Input Parameters:
 *   cmd_psm    Pointer of api command PSM struct.
 *   lte_psm    Pointer of lte_psm_setting_t.
 *
 * Returned Value:
 *   When set success is returned 0.
 *   When set failed return negative value.
 *
 ****************************************************************************/

int32_t altcombs_set_psm(struct apicmd_cmddat_psm_set_s *cmd_psm,
  lte_psm_setting_t *lte_psm)
{
  uint8_t t3324_unit_table[] =
    {
      LTE_PSM_T3324_UNIT_2SEC,
      LTE_PSM_T3324_UNIT_1MIN,
      LTE_PSM_T3324_UNIT_6MIN,
    };
  uint8_t t3412_unit_table[] =
    {
      LTE_PSM_T3412_UNIT_2SEC,
      LTE_PSM_T3412_UNIT_30SEC,
      LTE_PSM_T3412_UNIT_1MIN,
      LTE_PSM_T3412_UNIT_10MIN,
      LTE_PSM_T3412_UNIT_1HOUR,
      LTE_PSM_T3412_UNIT_10HOUR,
      LTE_PSM_T3412_UNIT_320HOUR
    };

  if (!cmd_psm || !lte_psm)
    {
      return -EINVAL;
    }

  if (APICMD_PSM_ENABLE == cmd_psm->enable)
    {
      if ((ALTCOMBS_PSM_UNIT_T3324_MIN > cmd_psm->rat_time.unit ||
          ALTCOMBS_PSM_UNIT_T3324_MAX < cmd_psm->rat_time.unit) ||
          (ALTCOMBS_PSM_UNIT_T3412_MIN > cmd_psm->tau_time.unit ||
          ALTCOMBS_PSM_UNIT_T3412_MAX < cmd_psm->tau_time.unit))
        {
          return -EINVAL;
        }

      lte_psm->enable = LTE_ENABLE;

      lte_psm->req_active_time.unit =
        t3324_unit_table[cmd_psm->rat_time.unit];
      lte_psm->req_active_time.time_val = cmd_psm->rat_time.time_val;

      lte_psm->ext_periodic_tau_time.unit =
        t3412_unit_table[cmd_psm->tau_time.unit];
      lte_psm->ext_periodic_tau_time.time_val = cmd_psm->tau_time.time_val;
    }
  else
    {
      lte_psm->enable = LTE_DISABLE;
    }

  return 0;
}

/****************************************************************************
 * Name: altcombs_set_quality
 *
 * Description:
 *   Set lte_quality_t.
 *
 * Input Parameters:
 *   data           Pointer of lte_quality_t.
 *   cmd_quality    Pointer of api command Quality struct.
 *
 * Returned Value:
 *   When check success is returned 0.
 *   When check failed return negative value.
 *
 ****************************************************************************/

int32_t altcombs_set_quality(FAR lte_quality_t *data,
          FAR struct apicmd_cmddat_quality_s *cmd_quality)
{
  data->valid = APICMD_QUALITY_ENABLE == cmd_quality->enability ?
                         LTE_VALID : LTE_INVALID;
  if(data->valid)
    {
      data->rsrp  = ntohs(cmd_quality->rsrp);
      data->rsrq  = ntohs(cmd_quality->rsrq);
      data->sinr  = ntohs(cmd_quality->sinr);
      data->rssi  = ntohs(cmd_quality->rssi);
      if (data->rsrp < APICMD_QUALITY_RSRP_MIN ||
        APICMD_QUALITY_RSRP_MAX < data->rsrp)
        {
          DBGIF_LOG1_ERROR("data.rsrp error:%d\n", data->rsrp);
          data->valid = LTE_INVALID;
        }
      else if (data->rsrq < APICMD_QUALITY_RSRQ_MIN ||
          APICMD_QUALITY_RSRQ_MAX < data->rsrq)
        {
          DBGIF_LOG1_ERROR("data.rsrq error:%d\n", data->rsrq);
          data->valid = LTE_INVALID;
        }
      else if (data->sinr < APICMD_QUALITY_SINR_MIN ||
          APICMD_QUALITY_SINR_MAX < data->sinr)
        {
          DBGIF_LOG1_ERROR("data->sinr error:%d\n", data->sinr);
          data->valid = LTE_INVALID;
        }
      else
        {
          /* Do nothing. */
        }
    }
  return 0;
}

