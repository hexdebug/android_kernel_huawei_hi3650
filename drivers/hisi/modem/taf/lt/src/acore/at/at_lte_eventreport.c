
/*lint -save -e537 -e718 -e746 -e734 -e958 -e713 -e740*/
#include "osm.h"
#include "gen_msg.h"
#include "msp_at.h"
#include "at_lte_common.h"
#include "ATCmdProc.h"
//#include "AtNdisInterface.h"
//#include "AtDataProc.h"
//#include "AtDataProc.h"


#include "AtParse.h"/*为包含MnClient.h
 */
#include "gen_common.h"
#include "at_common.h"
#include "AtCtx.h"
#include "OmApi.h"
#include "AtCmdMiscProc.h"


/*lint -e767 原因:Log打印*/
#define    THIS_FILE_ID        MSP_FILE_ID_AT_LTE_EVENTREPORT_C
/*lint +e767 */



/******************************************************************************
 */
/* 函数名称: atCsqInfoIndProc
 */
/* 功能描述: 处理CSQ主动上报信息
 */
/*
 */
/* 参数说明: pMsgBlock，MSP MSG消息结构
 */
/*
 */
/* 返 回 值:
 */
/*
 */
/* 调用要求: TODO: ...
 */
/* 调用举例: TODO: ...
 */
/* 作    者: 陈晓军00184452 [2010-10-10]
 */
/******************************************************************************
 */
/*按照V1中的#define MSP_HAVE_AT_RSSI_REPORT修改的
 */
VOS_UINT32 atCsqInfoIndProc(VOS_VOID *pMsgBlock)
{
    L4A_CSQ_INFO_IND_STRU* pCsqInfo = NULL;
    VOS_UINT16 usLength = 0;
    VOS_INT16 ulRssi = 0;

    pCsqInfo = (L4A_CSQ_INFO_IND_STRU*)pMsgBlock;

    if(ERR_MSP_SUCCESS == pCsqInfo->ulErrorCode)
    {
        if(AT_RSSI_UNKNOWN == pCsqInfo->sRssi)
        {
            ulRssi = AT_RSSI_UNKNOWN;
        }
        else if(pCsqInfo->sRssi >= AT_RSSI_HIGH)
        {
            ulRssi = AT_CSQ_RSSI_HIGH;
        }
        else if(pCsqInfo->sRssi <= AT_RSSI_LOW)
        {
            ulRssi = AT_CSQ_RSSI_LOW;
        }
        else
        {
            ulRssi = (pCsqInfo->sRssi - AT_RSSI_LOW) / 2;
        }

        usLength += (VOS_UINT16)At_sprintf(AT_CMD_MAX_LEN,(VOS_CHAR *)pgucLAtSndCodeAddr,
                            (VOS_CHAR *)pgucLAtSndCodeAddr + usLength,"%s%s%d%s",
                            gaucAtCrLf, "^RSSI:", ulRssi, gaucAtCrLf);
        At_SendResultData(AT_BROADCAST_CLIENT_INDEX_MODEM_0,pgucLAtSndCodeAddr,usLength);

    }


    return AT_FW_CLIENT_STATUS_READY;
}

#define AT_ANTENNA_LEVEL_NUM        3
VOS_UINT8         g_ATAntennaLevel[AT_ANTENNA_LEVEL_NUM];
VOS_UINT8         g_ulAntennaLevel = 0;

VOS_UINT32       g_ATE5Order = 0;

VOS_UINT8 AT_CalculateLTEAntennaLevel(
    VOS_INT16                           usRsrp
)
{
	VOS_INT16 usLevel = 0;
       g_ATE5Order =4;
	/*天线格式显示规则
 */
	if ( usRsrp <= g_stRsrpCfg.ssValue[3])
	{
		usLevel = AT_CMD_ANTENNA_LEVEL_4;
	}
	else if (( g_stRsrpCfg.ssValue[3] < usRsrp) && (usRsrp <= g_stRsrpCfg.ssValue[2]))
	{
		usLevel = AT_CMD_ANTENNA_LEVEL_3;
	}
	else if (( g_stRsrpCfg.ssValue[2] < usRsrp) && (usRsrp <= g_stRsrpCfg.ssValue[1]))
	{
		usLevel = AT_CMD_ANTENNA_LEVEL_2;
	}
	else
	{
		usLevel = AT_CMD_ANTENNA_LEVEL_1;
	}
	return (VOS_UINT8)usLevel;

}

VOS_UINT8 AT_GetSmoothLTEAntennaLevel(
    AT_CMD_ANTENNA_LEVEL_ENUM_UINT8     enLevel
)
{
    VOS_UINT8                               i;
    g_ATE5Order =5;
    /* 丢网时 立即更新  */
    if ( AT_CMD_ANTENNA_LEVEL_0 == enLevel )
    {
        g_ulAntennaLevel = enLevel;

        MSP_MEMSET( g_ATAntennaLevel, 0, sizeof(g_ATAntennaLevel) );
        return g_ulAntennaLevel;
    }

    /* 丢网到有服务状态  立即更新  */
    if ( AT_CMD_ANTENNA_LEVEL_0 == g_ulAntennaLevel )
    {
        g_ulAntennaLevel = enLevel;

        MSP_MEMSET( g_ATAntennaLevel, 0, sizeof(g_ATAntennaLevel) );
        g_ATAntennaLevel[AT_ANTENNA_LEVEL_NUM-1] = enLevel;
        return g_ulAntennaLevel;
    }

    /* 与上次的信号格数比较, 变化比较大(超过1格)就立即更新 */
    if ( enLevel > (g_ulAntennaLevel + 1) )
    {
        g_ulAntennaLevel = enLevel;

        MSP_MEMSET( g_ATAntennaLevel, 0, sizeof(g_ATAntennaLevel) );
        g_ATAntennaLevel[AT_ANTENNA_LEVEL_NUM-1] = enLevel;
        return g_ulAntennaLevel;
    }
    else if (( enLevel + 1) < g_ulAntennaLevel )
    {
        g_ulAntennaLevel = enLevel;

        MSP_MEMSET( g_ATAntennaLevel, 0, sizeof(g_ATAntennaLevel) );
        g_ATAntennaLevel[AT_ANTENNA_LEVEL_NUM-1] = enLevel;
        return g_ulAntennaLevel;
    }
    else
    {
      /* Do nothing... */
    }

    /* 先进先出存最近3次的查询结果 */
    for ( i=0; i<AT_ANTENNA_LEVEL_NUM-1; i++ )
    {
         g_ATAntennaLevel[i] = g_ATAntennaLevel[i+1];
    }
    g_ATAntennaLevel[i] = enLevel;


    /* 格数波动则不更新，以达到平滑的效果 */
    for ( i=0; i<AT_ANTENNA_LEVEL_NUM-1; i++ )
    {
        if (g_ATAntennaLevel[i]  != g_ATAntennaLevel[i+1] )
        {
            return g_ulAntennaLevel;
        }
    }

    /* 信号格数稳定了 AT_ANTENNA_LEVEL_MAX_NUM 次时才做更新 */
    g_ulAntennaLevel = enLevel;
	return g_ulAntennaLevel;
}

VOS_UINT32 at_CsqInfoProc(VOS_VOID *pMsgBlock,AT_ANLEVEL_INFO_CNF_STRU* pAnlevelAnqueryInfo)
{
    VOS_UINT32 ulResult = ERR_MSP_SUCCESS;
    L4A_CSQ_INFO_IND_STRU* pCsqInfo = NULL;
    VOS_UINT16                          usRsrp;
    VOS_UINT8 							enCurAntennaLevel;
    g_ATE5Order =2;

    if((NULL == pMsgBlock)||(NULL == pAnlevelAnqueryInfo))
    {
        HAL_SDMLOG("pMsgBlock is NULL!\n");
        return AT_ERROR;
    }
    pCsqInfo = (L4A_CSQ_INFO_IND_STRU*)pMsgBlock;
    pAnlevelAnqueryInfo->usClientId= pCsqInfo->usClientId;
    pAnlevelAnqueryInfo->ulErrorCode = pCsqInfo->ulErrorCode;

    /*RSSI 格式显示规则
 */
    if(AT_RSSI_UNKNOWN == pCsqInfo->sRssi)
    {
        pAnlevelAnqueryInfo->sRssi = AT_RSSI_UNKNOWN;
    }
    else if(pCsqInfo->sRssi >= AT_RSSI_HIGH)
    {
        pAnlevelAnqueryInfo->sRssi = AT_CSQ_RSSI_HIGH;
    }
    else if(pCsqInfo->sRssi <= AT_RSSI_LOW)
    {
        pAnlevelAnqueryInfo->sRssi = AT_CSQ_RSSI_LOW;
    }
    else
    {
        pAnlevelAnqueryInfo->sRssi = ( VOS_UINT16)((pCsqInfo->sRssi - AT_RSSI_LOW) / 2);
    }

	/* 上报数据转换:将 Rscp、Ecio显示为非负值，若Rscp、Ecio为-145，-32，或者rssi为99，
       则转换为0 */
    if ( (AT_RSSI_UNKNOWN  == pCsqInfo->sRsrp)|| (AT_RSSI_UNKNOWN == pCsqInfo->sRssi) )
    {
        /* 丢网返回0, 对应应用的圈外 */
        enCurAntennaLevel       = AT_CMD_ANTENNA_LEVEL_0;
    }
    else
    {
    	 /* 取绝对值 */
    	usRsrp                    = ( VOS_UINT16)(-(pCsqInfo->sRsrp));
	    /* 调用函数AT_CalculateAntennaLevel来根据D25算法计算出信号格数 */
        enCurAntennaLevel = AT_CalculateLTEAntennaLevel((VOS_INT16)usRsrp);
    }

    /* 信号磁滞处理 */
    pAnlevelAnqueryInfo->sLevel = AT_GetSmoothLTEAntennaLevel( enCurAntennaLevel );

	pAnlevelAnqueryInfo->sRsrp = pCsqInfo->sRsrp;
	pAnlevelAnqueryInfo->sRsrq = pCsqInfo->sRsrq;

	return ulResult;
}

VOS_UINT32 atAnlevelInfoIndProc(VOS_VOID *pMsgBlock)
{
  VOS_UINT16 usLength = 0;
  VOS_UINT32 ulResult =0;
  VOS_UINT16 usRsrp=0;
  VOS_UINT16 usRsrq=0;
  AT_ANLEVEL_INFO_CNF_STRU stAnlevelAnqueryInfo = {0};
  g_ATE5Order =3;

  ulResult = at_CsqInfoProc(pMsgBlock,&stAnlevelAnqueryInfo);

  if (ulResult == ERR_MSP_SUCCESS)
  {
    usRsrp = (VOS_UINT16)((stAnlevelAnqueryInfo.sRsrp == 99)?99:(-(stAnlevelAnqueryInfo.sRsrp)));
    usRsrq = (VOS_UINT16)((stAnlevelAnqueryInfo.sRsrq == 99)?99:(-(stAnlevelAnqueryInfo.sRsrq)));

  	usLength += (VOS_UINT16)At_sprintf(AT_CMD_MAX_LEN,(VOS_CHAR *)pgucLAtSndCodeAddr,
						  (VOS_CHAR *)pgucLAtSndCodeAddr + usLength,"%s%s0,99,%d,%d,%d,%d%s",
						  gaucAtCrLf, "^ANLEVEL:",
						  stAnlevelAnqueryInfo.sRssi,
						  stAnlevelAnqueryInfo.sLevel,
						  usRsrp,
						  usRsrq,gaucAtCrLf);
    At_SendResultData(AT_CLIENT_TAB_APP_INDEX,pgucLAtSndCodeAddr,usLength);
  }
  else
  {
    HAL_SDMLOG("ulResult = %d,\n",(VOS_INT)ulResult);
  }

  return AT_FW_CLIENT_STATUS_READY;
}

VOS_UINT32 atSetAnlevelCnfSameProc(VOS_VOID *pMsgBlock)
{
	VOS_UINT16 usLength = 0;
	VOS_UINT32 ulResult =0;
    VOS_UINT16 usRsrp=0;
    VOS_UINT16 usRsrq=0;

	AT_ANLEVEL_INFO_CNF_STRU stAnlevelAnqueryInfo = {0};
     g_ATE5Order =1;

    ulResult = at_CsqInfoProc(pMsgBlock,&stAnlevelAnqueryInfo);

	if(ERR_MSP_SUCCESS == ulResult)
  	{
        usRsrp = (VOS_UINT16)((stAnlevelAnqueryInfo.sRsrp == 99)?99:(-(stAnlevelAnqueryInfo.sRsrp)));
        usRsrq = (VOS_UINT16)((stAnlevelAnqueryInfo.sRsrq == 99)?99:(-(stAnlevelAnqueryInfo.sRsrq)));

  		usLength += (VOS_UINT16)At_sprintf(AT_CMD_MAX_LEN,(VOS_CHAR *)pgucLAtSndCodeAddr,
						  (VOS_CHAR *)pgucLAtSndCodeAddr + usLength,"%s%s0,99,%d,%d,%d,%d%s",
						  gaucAtCrLf, "^ANQUERY:",
						  stAnlevelAnqueryInfo.sRssi,
						  stAnlevelAnqueryInfo.sLevel,
						  usRsrp,
						  usRsrq,gaucAtCrLf);
  	}
  	else
  	{
  	      HAL_SDMLOG(" atSetAnlevelCnfSameProc ulResult = %d,\n",(VOS_INT)ulResult);
  	}

	CmdErrProc((VOS_UINT8)(stAnlevelAnqueryInfo.usClientId), stAnlevelAnqueryInfo.ulErrorCode,
						usLength, pgucLAtSndCodeAddr);

	return AT_FW_CLIENT_STATUS_READY;
}


/******************************************************************************
 */
/* 函数名称: atSetCsqCnfProc
 */
/* 功能描述: 处理SET CSQ命令回复信息
 */
/*
 */
/* 参数说明: pMsgBlock，MSP MSG消息结构
 */
/*
 */
/* 返 回 值:
 */
/*
 */
/* 调用要求: TODO: ...
 */
/* 调用举例: TODO: ...
 */
/* 作    者: 陈晓军00184452 [2010-10-10]
 */
/******************************************************************************
 */

/*这个函数跟AT命令表中的函数名字相同，是不是要重新改名字
 */
VOS_UINT32 atSetCsqCnfSameProc(VOS_VOID *pMsgBlock)
{
    L4A_CSQ_INFO_CNF_STRU * pstSetCsqCnf = NULL;
    VOS_UINT8 ucRssiValue = 0;
    VOS_UINT8 ucChannalQual = 0;

    VOS_UINT16 usLength = 0;

    pstSetCsqCnf = (L4A_CSQ_INFO_CNF_STRU*)pMsgBlock;

    if(ERR_MSP_SUCCESS == pstSetCsqCnf->ulErrorCode)
    {
        if(AT_RSSI_UNKNOWN == pstSetCsqCnf->sRssi)
        {
            ucRssiValue = AT_RSSI_UNKNOWN;
        }
        else if(pstSetCsqCnf->sRssi >= AT_RSSI_HIGH)
        {
            ucRssiValue = AT_CSQ_RSSI_HIGH;
        }
        else if(pstSetCsqCnf->sRssi <= AT_RSSI_LOW)
        {
            ucRssiValue = AT_CSQ_RSSI_LOW;
        }
        else
        {
            ucRssiValue = (VOS_UINT8)((pstSetCsqCnf->sRssi - AT_RSSI_LOW) / 2);
        }

        ucChannalQual = AT_BER_UNKNOWN;
    }
    else
    {
        ucRssiValue   = AT_RSSI_UNKNOWN;
        ucChannalQual = AT_BER_UNKNOWN;
    }

    usLength += (VOS_UINT16)At_sprintf( AT_CMD_MAX_LEN,
                                                    (VOS_CHAR *)pgucLAtSndCodeAddr,
                                                    (VOS_CHAR *)pgucLAtSndCodeAddr + usLength,
                                                    "%s: %d,%d%s",
                                                    "+CSQ",
                                                    ucRssiValue,
                                                    ucChannalQual,
                                                    gaucAtCrLf);

    CmdErrProc((VOS_UINT8)(pstSetCsqCnf->usClientId), ERR_MSP_SUCCESS, usLength, pgucLAtSndCodeAddr);

    return AT_FW_CLIENT_STATUS_READY;
}

VOS_UINT32 atCerssiInfoIndProc(VOS_VOID *pMsgBlock)
{
    L4A_CSQ_INFO_IND_STRU * pCerssi = NULL;
    VOS_UINT16 usLength = 0;

    pCerssi = (L4A_CSQ_INFO_IND_STRU*)pMsgBlock;

    usLength += (VOS_UINT16)At_sprintf(AT_CMD_MAX_LEN,(VOS_CHAR *)pgucLAtSndCodeAddr,
                            (VOS_CHAR *)pgucLAtSndCodeAddr + usLength,"%s%s0,0,255,%d,%d,%d,%d,%d,%d%s",
                            gaucAtCrLf, "^CERSSI:",pCerssi->sRsrp,pCerssi->sRsrq,pCerssi->lSINR,
                            pCerssi->stCQI.usRI,pCerssi->stCQI.ausCQI[0],pCerssi->stCQI.ausCQI[1],gaucAtCrLf);

    At_SendResultData(AT_BROADCAST_CLIENT_INDEX_MODEM_0,pgucLAtSndCodeAddr,usLength);

    return AT_FW_CLIENT_STATUS_READY;
}

VOS_UINT32 atCerssiInfoCnfProc(VOS_VOID *pMsgBlock)
{
    L4A_CSQ_INFO_CNF_STRU * pCerssi = NULL;
    VOS_UINT16 usLength = 0;
    AT_MODEM_NET_CTX_STRU              *pstNetCtx = VOS_NULL_PTR;
    VOS_UINT8                           ucIndex;

    pCerssi = (L4A_CSQ_INFO_CNF_STRU*)pMsgBlock;

    /* 通过clientid获取index */
    /* coverity[example_checked] */
    if (AT_FAILURE == At_ClientIdToUserId(pCerssi->usClientId,&ucIndex))
    {
       printk("atCerssiInfoCnfProc:WARNING:AT INDEX NOT FOUND!");

       return VOS_ERR;
    }

    pstNetCtx = AT_GetModemNetCtxAddrFromClientId(ucIndex);

    if(0 == pCerssi->ulErrorCode)
    {
        usLength = (VOS_UINT32)At_sprintf(AT_CMD_MAX_LEN,(VOS_CHAR *)pgucLAtSndCodeAddr,
            (VOS_CHAR *)pgucLAtSndCodeAddr,
            "%s%s%d,%d,0,0,255,%d,%d,%d,%d,%d,%d%s",
            gaucAtCrLf, "^CERSSI:",  pstNetCtx->ucCerssiReportType,
            pstNetCtx->ucCerssiMinTimerInterval,  pCerssi->sRsrp, pCerssi->sRsrq, pCerssi->lSINR,
            pCerssi->stCQI.usRI, pCerssi->stCQI.ausCQI[0], pCerssi->stCQI.ausCQI[1], gaucAtCrLf);
     }

    CmdErrProc((VOS_UINT8)(pCerssi->usClientId), pCerssi->ulErrorCode, usLength, pgucLAtSndCodeAddr);

    return AT_FW_CLIENT_STATUS_READY;
}

/******************************************************************************
 */
/* 函数名称: atSysModeIndProc
 */
/* 功能描述: 处理SYSMODE主动上报信息
 */
/*
 */
/* 参数说明: pMsgBlock，MSP MSG消息结构
 */
/*
 */
/* 返 回 值:
 */
/*
 */
/* 调用要求: TODO: ...
 */
/* 调用举例: TODO: ...
 */
/* 作    者: 陈晓军00184452 [2010-10-10]
 */
/******************************************************************************
 */

VOS_UINT32 atSysModeIndProc(VOS_VOID *pMsgBlock)
{
    return AT_FW_CLIENT_STATUS_READY;
}


/*****************************************************************************
 函 数 名  : atLwclashCnfProc
 功能描述  : ^LWCLASH命令应答处理
 输入参数  : VOS_VOID *pMsgBlock
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :

 修改历史      :
  1.日    期   : 2012年8月25日
    作    者   : c64416
    修改内容   : 新生成函数

*****************************************************************************/
VOS_UINT32 atLwclashCnfProc(VOS_VOID *pMsgBlock)
{
    L4A_READ_LWCLASH_CNF_STRU *pstLwclash = NULL;
    VOS_UINT16 usLength = 0;

    pstLwclash = (L4A_READ_LWCLASH_CNF_STRU *)pMsgBlock;

    usLength = (VOS_UINT16)At_sprintf( AT_CMD_MAX_LEN,
                (VOS_CHAR *)pgucLAtSndCodeAddr,
                (VOS_CHAR *)pgucLAtSndCodeAddr,
                "^LWCLASH: %d,%d,%d,%d,%d,%d",
                pstLwclash->stLwclashInfo.enState,
                pstLwclash->stLwclashInfo.usUlFreq, pstLwclash->stLwclashInfo.usUlBandwidth,
                pstLwclash->stLwclashInfo.usDlFreq, pstLwclash->stLwclashInfo.usDlBandwidth,
                pstLwclash->stLwclashInfo.usBand);

    CmdErrProc((VOS_UINT8)(pstLwclash->usClientId), pstLwclash->ulErrorCode, usLength, pgucLAtSndCodeAddr);

    return AT_FW_CLIENT_STATUS_READY;
}


/*****************************************************************************
 函 数 名  : atLacellCnfProc
 功能描述  : ^LCACELL命令应答处理
 输入参数  : VOS_VOID *pMsgBlock
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :

 修改历史  :
*****************************************************************************/
VOS_UINT32 atLcacellCnfProc(VOS_VOID *pMsgBlock)
{
    L4A_READ_LCACELL_CNF_STRU *pstLcacell = NULL;
    VOS_UINT16 usLength = 0;
    VOS_UINT32 ulStatCnt = 0;
    
    pstLcacell = (L4A_READ_LCACELL_CNF_STRU *)pMsgBlock;
    
    usLength += (VOS_UINT16)At_sprintf( AT_CMD_MAX_LEN,
                (VOS_CHAR *)pgucLAtSndCodeAddr,
                (VOS_CHAR *)pgucLAtSndCodeAddr+usLength,
                "^LCACELL: ");

    for(ulStatCnt=0;ulStatCnt<CA_MAX_CELL_NUM;ulStatCnt++)
    {
        usLength += (VOS_UINT16)At_sprintf( AT_CMD_MAX_LEN,
                (VOS_CHAR *)pgucLAtSndCodeAddr,
                (VOS_CHAR *)pgucLAtSndCodeAddr+usLength,
                "\"%d %d %d %d\",",ulStatCnt, pstLcacell->stLcacellInfo[ulStatCnt].ucUlConfigured,
                pstLcacell->stLcacellInfo[ulStatCnt].ucDlConfigured,
                pstLcacell->stLcacellInfo[ulStatCnt].ucActived);
    }

    usLength--;
    
    CmdErrProc((VOS_UINT8)(pstLcacell->usClientId), pstLcacell->ulErrorCode, usLength, pgucLAtSndCodeAddr);

    return AT_FW_CLIENT_STATUS_READY;
}


VOS_UINT32 atQryCnmrCnfProc(VOS_VOID *pMsgBlock)
{
    L4A_READ_CNMR_CNF_STRU *pstCnf = NULL;
    VOS_UINT16 usLength = 0;
    VOS_UINT32 ulIndexTotal = 0;
    VOS_UINT32 ulIndexN = 0;
    VOS_UINT32 i=0;
    VOS_UCHAR * pChar = NULL;

    pstCnf = (L4A_READ_CNMR_CNF_STRU *)pMsgBlock;

    if(ERR_MSP_SUCCESS != pstCnf->ulErrorCode)
    {
        usLength += (VOS_UINT16)At_sprintf( AT_CMD_MAX_LEN,
                    (VOS_CHAR *)pgucLAtSndCodeAddr,
                    (VOS_CHAR *)pgucLAtSndCodeAddr+ usLength,"%s^CNMR:%d,%d",
                    gaucAtCrLf,ulIndexTotal,ulIndexN);
    }
    else
    {
        usLength += (VOS_UINT16)At_sprintf( AT_CMD_MAX_LEN,
                    (VOS_CHAR *)pgucLAtSndCodeAddr,
                    (VOS_CHAR *)pgucLAtSndCodeAddr+ usLength,"%s^CNMR:%d,%d",
                    gaucAtCrLf,ulIndexTotal+1,ulIndexN+1);
    }

    if(ERR_MSP_SUCCESS == pstCnf->ulErrorCode)
    {
        usLength += (VOS_UINT16)At_sprintf( AT_CMD_MAX_LEN,
                        (VOS_CHAR *)pgucLAtSndCodeAddr,
                        (VOS_CHAR *)pgucLAtSndCodeAddr+ usLength,",");

        /* coverity[ptr_assign] */
        pChar = ((VOS_UCHAR*)&(pstCnf->stLteCelInfo));

        for(i =0;i<sizeof(L4A_READ_CNMR_CNF_STRU)-12-4;i++)
        {
            /*lint -e661*/
            /*lint -e662*/
            /* coverity[overrun-local] */
			usLength += (VOS_UINT16)At_sprintf( AT_CMD_MAX_LEN,
                        (VOS_CHAR *)pgucLAtSndCodeAddr,
                        (VOS_CHAR *)pgucLAtSndCodeAddr+ usLength,"%02x",
                        *(pChar+i));/* [false alarm]:fortify */
			/*lint -e661*/
			/*lint -e662*/
        }
    }

    usLength += (VOS_UINT16)At_sprintf( AT_CMD_MAX_LEN,
                (VOS_CHAR *)pgucLAtSndCodeAddr,
                (VOS_CHAR *)pgucLAtSndCodeAddr+ usLength,"%s",
                 gaucAtCrLf);

    CmdErrProc((VOS_UINT8)(pstCnf->usClientId), ERR_MSP_SUCCESS, usLength, pgucLAtSndCodeAddr);

    return AT_FW_CLIENT_STATUS_READY;
}



VOS_UINT32 atQryCellIdCnfProc(VOS_VOID *pMsgBlock)
{
    L4A_READ_CELL_ID_CNF_STRU *pstCnf = NULL;
    VOS_UINT16 usLength = 0;

    pstCnf = (L4A_READ_CELL_ID_CNF_STRU *)pMsgBlock;

    usLength = (VOS_UINT16)At_sprintf( AT_CMD_MAX_LEN,
                (VOS_CHAR *)pgucLAtSndCodeAddr,
                (VOS_CHAR *)pgucLAtSndCodeAddr,
                ((2 == pstCnf->usMncNum)?"%s^CECELLID: %03x%02x,%d,%d,%d%s":"%s^CECELLID: %03x%03x,%d,%d,%d%s"),
                gaucAtCrLf,
                pstCnf->usMCC,
                (2 == pstCnf->usMncNum)?(pstCnf->usMNC&0xff):pstCnf->usMNC,
                pstCnf->ulCi,
                pstCnf->usPci,
                pstCnf->usTAC,
                gaucAtCrLf);

    CmdErrProc((VOS_UINT8)(pstCnf->usClientId), ERR_MSP_SUCCESS, usLength, pgucLAtSndCodeAddr);

    return AT_FW_CLIENT_STATUS_READY;
}

VOS_UINT32 g_ulCellNum =0;
VOS_UINT32 atQryCellInfoCnfProc(VOS_VOID *pMsgBlock)
{
    L4A_READ_CELL_INFO_CNF_STRU *pstcnf = NULL;
    VOS_UINT16 usLength = 0;
    VOS_UINT32 i =0;

    pstcnf = (L4A_READ_CELL_INFO_CNF_STRU *)pMsgBlock;

    if(0 != pstcnf->ulErrorCode)
    {
        CmdErrProc((VOS_UINT8)(pstcnf->usClientId), pstcnf->ulErrorCode, 0, NULL);

        return AT_FW_CLIENT_STATUS_READY;
    }

    if(0 == pstcnf->stNcellListInfo.enCellFlag)
    {
        usLength += (VOS_UINT16)At_sprintf( AT_CMD_MAX_LEN,
                (VOS_CHAR *)pgucLAtSndCodeAddr,
                (VOS_CHAR *)pgucLAtSndCodeAddr+usLength,
                "%d:%d,%d,%d%s",
                1,
                EN_SERVICE_CELL_ID,
                pstcnf->stNcellListInfo.stSevCellInfo.stCellMeasInfo[0].stMeasRslt.usPhyCellId,
                pstcnf->stNcellListInfo.stSevCellInfo.stCellMeasInfo[0].stMeasRslt.sRssi,
                gaucAtCrLf);
    }
    else
    {
        /*同频*/
        for( i =0; i< pstcnf->stNcellListInfo.stIntraFreqNcellList.ulNCellNumber ;i++)
        {
            g_ulCellNum ++;
            usLength += (VOS_UINT16)At_sprintf( AT_CMD_MAX_LEN,
                (VOS_CHAR *)pgucLAtSndCodeAddr,
                (VOS_CHAR *)pgucLAtSndCodeAddr+usLength,
                "%d:%d,%d,%d%s",
                g_ulCellNum,
                EN_SYN_FREQ_CELL_ID,
                pstcnf->stNcellListInfo.stIntraFreqNcellList.stCellMeasInfo[i].stMeasRslt.usPhyCellId,
                pstcnf->stNcellListInfo.stIntraFreqNcellList.stCellMeasInfo[i].stMeasRslt.sRssi,
                gaucAtCrLf);
        }
         /*异频*/
        for( i =0; i< pstcnf->stNcellListInfo.stInterFreqNcellList.ulNCellNumber ;i++)
        {
            g_ulCellNum ++;
            usLength += (VOS_UINT16)At_sprintf( AT_CMD_MAX_LEN,
                (VOS_CHAR *)pgucLAtSndCodeAddr,
                (VOS_CHAR *)pgucLAtSndCodeAddr+usLength,
                "%d:%d,%d,%d%s",
                g_ulCellNum,
                EN_ASYN_FREQ_CELL_ID,
                pstcnf->stNcellListInfo.stInterFreqNcellList.stCellMeasInfo[i].stMeasRslt.usPhyCellId,
                pstcnf->stNcellListInfo.stInterFreqNcellList.stCellMeasInfo[i].stMeasRslt.sRssi,
                gaucAtCrLf);
        }
        /*W异频*/
        for( i =0; i< pstcnf->stNcellListInfo.stInterRATUMTSNcellList.ulNCellNumber ;i++)
        {
            g_ulCellNum ++;
            usLength += (VOS_UINT16)At_sprintf( AT_CMD_MAX_LEN,
                (VOS_CHAR *)pgucLAtSndCodeAddr,
                (VOS_CHAR *)pgucLAtSndCodeAddr+usLength,
                "%d:%d,%d,%d%s",
                g_ulCellNum,
                EN_ASYN_UMTS_CELL_ID,
                pstcnf->stNcellListInfo.stInterRATUMTSNcellList.stUMTSNcellList[i].usPrimaryScramCode,
                pstcnf->stNcellListInfo.stInterRATUMTSNcellList.stUMTSNcellList[i].sCpichRscp,
                gaucAtCrLf);
        }
        /*G异频*/
        for( i =0; i< pstcnf->stNcellListInfo.stInterRATGeranNcellList.ulNCellNumber ;i++)
        {
            g_ulCellNum ++;
            usLength += (VOS_UINT16)At_sprintf( AT_CMD_MAX_LEN,
                (VOS_CHAR *)pgucLAtSndCodeAddr,
                (VOS_CHAR *)pgucLAtSndCodeAddr+usLength,
                "%d:%d,%d,%d%s",
                g_ulCellNum,
                EN_ASYN_GSM_CELL_ID,
                (pstcnf->stNcellListInfo.stInterRATGeranNcellList.stGeranNcellList[i].stBSIC.usNcc) * 16 *16 +
                pstcnf->stNcellListInfo.stInterRATGeranNcellList.stGeranNcellList[i].stBSIC.usBcc,
                pstcnf->stNcellListInfo.stInterRATGeranNcellList.stGeranNcellList[i].sRSSI,
                gaucAtCrLf);
        }

        if(g_ulCellNum == 0)
        {
            usLength += (VOS_UINT16)At_sprintf( AT_CMD_MAX_LEN,
                (VOS_CHAR *)pgucLAtSndCodeAddr,
                (VOS_CHAR *)pgucLAtSndCodeAddr+usLength,
                "%d%s",
                g_ulCellNum,
                gaucAtCrLf);
        }
    }

    g_ulCellNum = 0;
    CmdErrProc((VOS_UINT8)(pstcnf->usClientId), pstcnf->ulErrorCode, usLength, pgucLAtSndCodeAddr);

    return AT_FW_CLIENT_STATUS_READY;
}

/*****************************************************************************
 函 数 名  : atLwclashInd
 功能描述  : ^LWURC命令主动上报处理
 输入参数  : VOS_VOID *pMsgBlock
 输出参数  : 无
 返 回 值  : VOS_VOID
 调用函数  :
 被调函数  :

 修改历史      :
  1.日    期   : 2012年8月25日
    作    者   : c64416
    修改内容   : 新生成函数

*****************************************************************************/
VOS_VOID atLwclashInd(VOS_VOID *pMsgBlock)
{
    L4A_READ_LWCLASH_IND_STRU *pstLwclash = NULL;

    VOS_UINT16 usLength = 0;

    pstLwclash = (L4A_READ_LWCLASH_IND_STRU *)pMsgBlock;

        usLength = (VOS_UINT16)At_sprintf(AT_CMD_MAX_LEN,
                    (VOS_CHAR *)pgucLAtSndCodeAddr,
                    (VOS_CHAR *)pgucLAtSndCodeAddr,
                    "%s^LWURC: %d,%d,%d,%d,%d,%d%s",
                    gaucAtCrLf, pstLwclash->stLwclashInfo.enState,
                    pstLwclash->stLwclashInfo.usUlFreq, pstLwclash->stLwclashInfo.usUlBandwidth,
                    pstLwclash->stLwclashInfo.usDlFreq, pstLwclash->stLwclashInfo.usDlBandwidth,
                    pstLwclash->stLwclashInfo.usBand, gaucAtCrLf);

    At_SendResultData(AT_BROADCAST_CLIENT_INDEX_MODEM_0, pgucLAtSndCodeAddr, usLength);
}

/*****************************************************************************
 函 数 名  : atLcacellInd
 功能描述  : ^LCACELLURC命令主动上报处理
 输入参数  : VOS_VOID *pMsgBlock
 输出参数  : 无
 返 回 值  : VOS_VOID
 调用函数  :
 被调函数  :

 修改历史  :

*****************************************************************************/
VOS_VOID atLcacellInd(VOS_VOID *pMsgBlock)
{
    L4A_READ_LCACELL_IND_STRU *pstLcacell = NULL;
    VOS_UINT16 usLength = 0;
    VOS_UINT32 ulStatCnt = 0;

    pstLcacell = (L4A_READ_LCACELL_IND_STRU *)pMsgBlock;

    /* 此命令头尾添加\r\n */
    usLength += (VOS_UINT16)At_sprintf( AT_CMD_MAX_LEN,
                (VOS_CHAR *)pgucLAtSndCodeAddr,
                (VOS_CHAR *)pgucLAtSndCodeAddr+usLength,
                "%s^LCACELLURC: ",
                gaucAtCrLf);


    for(ulStatCnt=0;ulStatCnt<CA_MAX_CELL_NUM;ulStatCnt++)
    {
        if (0 != ulStatCnt)
        {
            usLength += (VOS_UINT16)At_sprintf( AT_CMD_MAX_LEN,
                    (VOS_CHAR *)pgucLAtSndCodeAddr,
                    (VOS_CHAR *)pgucLAtSndCodeAddr+usLength,
                    ",");
        }

        usLength += (VOS_UINT16)At_sprintf( AT_CMD_MAX_LEN,
                (VOS_CHAR *)pgucLAtSndCodeAddr,
                (VOS_CHAR *)pgucLAtSndCodeAddr+usLength,
                "\"%d %d %d %d\"",ulStatCnt, pstLcacell->stLcacellInfo[ulStatCnt].ucUlConfigured,
                pstLcacell->stLcacellInfo[ulStatCnt].ucDlConfigured,
                pstLcacell->stLcacellInfo[ulStatCnt].ucActived);
    }

    usLength += (VOS_UINT16)At_sprintf( AT_CMD_MAX_LEN,
                (VOS_CHAR *)pgucLAtSndCodeAddr,
                (VOS_CHAR *)pgucLAtSndCodeAddr+usLength,
                "%s",
                gaucAtCrLf);

    At_SendResultData(AT_BROADCAST_CLIENT_INDEX_MODEM_0, pgucLAtSndCodeAddr, usLength);
}


typedef VOS_UINT32 (*AT_L4A_MSG_FUN)(VOS_VOID* pMsgBlock);

typedef struct
{
    VOS_UINT32 ulMsgId;
    AT_L4A_MSG_FUN atL4aMsgProc;
}AT_L4A_MSG_FUN_TABLE_STRU;

static const AT_L4A_MSG_FUN_TABLE_STRU g_astAtL4aCnfMsgFunTable[] = {

    {ID_MSG_L4A_CSQ_INFO_CNF        ,   atSetCsqCnfSameProc     },
	{ID_MSG_L4A_ANQUERY_INFO_CNF    , 	atSetAnlevelCnfSameProc },
    {ID_MSG_L4A_CNMR_CNF            , 	atQryCnmrCnfProc     	},
    {ID_MSG_L4A_CELL_ID_CNF         ,   atQryCellIdCnfProc      },
    {ID_MSG_L4A_LWCLASHQRY_CNF      ,   atLwclashCnfProc        },

    {ID_MSG_L4A_RADVER_SET_CNF      ,   atSetRadverCnfProc      },
    {ID_MSG_L4A_CELL_INFO_CNF       ,   atQryCellInfoCnfProc    },
    {ID_MSG_L4A_CERSSI_INQ_CNF      ,   atCerssiInfoCnfProc     },
    {ID_MSG_L4A_LCACELLQRY_CNF      ,   atLcacellCnfProc        },
};

static const AT_L4A_MSG_FUN_TABLE_STRU g_astAtL4aIndMsgFunTable[] = {
    {ID_MSG_L4A_RSSI_IND        	,   atCsqInfoIndProc        },
	{ID_MSG_L4A_ANLEVEL_IND			,	atAnlevelInfoIndProc	},
    {ID_MSG_L4A_CERSSI_IND	        ,	atCerssiInfoIndProc		},
    {ID_MSG_L4A_MODE_IND            ,   atSysModeIndProc        },
    {ID_MSG_L4A_LWCLASH_IND         ,   (AT_L4A_MSG_FUN)atLwclashInd},
    {ID_MSG_L4A_LCACELL_IND         ,   (AT_L4A_MSG_FUN)atLcacellInd},
};

/******************************************************************************
 */
/* 函数名称: atL4aGetMsgFun
 */
/* 功能描述: 获取处理L4A回复消息的函数
 */
/*
 */
/* 参数说明: ulMsgId，消息ID
 */
/*
 */
/* 返 回 值:
 */
/*
 */
/* 调用要求: TODO: ...
 */
/* 调用举例: TODO: ...
 */
/* 作    者: 陈晓军00184452 [2010-10-10]
 */
/******************************************************************************
 */
AT_L4A_MSG_FUN_TABLE_STRU* atL4aGetCnfMsgFun(VOS_UINT32 ulMsgId)
{
    VOS_UINT32 i = 0;
    VOS_UINT32 ulTableLen = 0;
    const AT_L4A_MSG_FUN_TABLE_STRU* pTable;

    pTable = g_astAtL4aCnfMsgFunTable;
    ulTableLen = sizeof(g_astAtL4aCnfMsgFunTable);

    for(i = 0; i < (ulTableLen/sizeof(AT_L4A_MSG_FUN_TABLE_STRU)); i++)
    {
        if(ulMsgId == (pTable + i)->ulMsgId)
        {
            return (AT_L4A_MSG_FUN_TABLE_STRU*)(pTable + i);
        }
    }

    /*如果找不到
 */
    return NULL;
}

/******************************************************************************
 */
/* 函数名称: atL4aGetIndMsgFun
 */
/* 功能描述: 获取处理L4A回复消息的函数
 */
/*
 */
/* 参数说明: ulMsgId，消息ID
 */
/*
 */
/* 返 回 值:
 */
/*
 */
/* 调用要求: TODO: ...
 */
/* 调用举例: TODO: ...
 */
/* 作    者: 陈晓军00184452 [2010-10-10]
 */
/******************************************************************************
 */
AT_L4A_MSG_FUN_TABLE_STRU* atL4aGetIndMsgFun(VOS_UINT32 ulMsgId)
{
    VOS_UINT32 i = 0;
    VOS_UINT32 ulTableLen = 0;
    const AT_L4A_MSG_FUN_TABLE_STRU* pTable;

    pTable = g_astAtL4aIndMsgFunTable;
    ulTableLen = sizeof(g_astAtL4aIndMsgFunTable);

    for(i = 0; i < (ulTableLen/sizeof(AT_L4A_MSG_FUN_TABLE_STRU)); i++)
    {
        if(ulMsgId == (pTable + i)->ulMsgId)
        {
            return (AT_L4A_MSG_FUN_TABLE_STRU*)(pTable + i);
        }
    }

    /*如果找不到
 */
    return NULL;
}




/******************************************************************************
 */
/* 函数名称: at_L4aCnfProc
 */
/* 功能描述: 接收L4A的CNF，IND消息，并查找对应函数进行处理
 */
/*
 */
/* 参数说明: pMsgBlockTmp，VOS消息
 */
/*
 */
/* 返 回 值:
 */
/*
 */
/* 调用要求: TODO: ...
 */
/* 调用举例: TODO: ...
 */
/* 作    者: 陈晓军00184452 [2010-10-10]
 */
/******************************************************************************
 */
VOS_UINT32 at_L4aCnfProc(MsgBlock* pMsgBlockTmp)
{
    VOS_UINT32 ulRet = ERR_MSP_SUCCESS;
    VOS_UINT32 ulMsgId = 0;
    VOS_UINT16 usIndex = 0;
    AT_L4A_MSG_FUN_TABLE_STRU* pTable = NULL;
    AT_L4A_MSG_FUN pMsgProc = NULL;
    L4A_AT_CNF_HEADER_STRU    *pstMsgHeader;

    pstMsgHeader    = (L4A_AT_CNF_HEADER_STRU *)pMsgBlockTmp;
	ulMsgId = pstMsgHeader->enMsgId;

    /* coverity[check_return] */
    At_ClientIdToUserId((VOS_UINT16)pstMsgHeader->usClientId, (TAF_UINT8 *)&usIndex);

    pstMsgHeader->usClientId = usIndex;
    if( (pTable = atL4aGetCnfMsgFun(ulMsgId)) && (pTable->atL4aMsgProc))
    {
        AT_STOP_TIMER_CMD_READY(usIndex);
        pMsgProc = pTable->atL4aMsgProc;
    }
    else if( (pTable = atL4aGetIndMsgFun(ulMsgId)) && (pTable->atL4aMsgProc))
    {
        pMsgProc = pTable->atL4aMsgProc;
    }
    else
    {
        ulRet = ERR_MSP_FAILURE;
    }

	if(pMsgProc)
    {
        ulRet = pMsgProc(pMsgBlockTmp);
    }

    return ulRet;
}


AT_FTM_CNF_MSG_PROC_STRU g_astLteAtFtmCnfMsgTbl[] =
{
    { ID_MSG_SYM_SET_TMODE_CNF,         atSetTmodeParaCnfProc },
    { ID_MSG_FTM_SET_FCHAN_CNF,         atSetFCHANParaCnfProc },
    { ID_MSG_FTM_RD_FCHAN_CNF,          atQryFCHANParaCnfProc },
    { ID_MSG_FTM_SET_TXON_CNF,          atSetFTXONParaCnfProc },
    { ID_MSG_FTM_RD_TXON_CNF,           atQryFTXONParaCnfProc },
    { ID_MSG_FTM_SET_RXON_CNF,          atSetFRXONParaCnfProc },
    { ID_MSG_FTM_RD_RXON_CNF,           atQryFRXONParaCnfProc },
    { ID_MSG_FTM_SET_FWAVE_CNF,         atSetFWAVEParaCnfProc },
    { ID_MSG_FTM_SET_TSELRF_CNF,        atSetTselrfParaCnfProc},/*TSELRF 已实现未联调 */
    { ID_MSG_FTM_SET_AAGC_CNF,          atSetFLNAParaCnfProc },
    { ID_MSG_FTM_RD_AAGC_CNF,           atQryFLNAParaCnfProc },
    { ID_MSG_FTM_FRSSI_CNF,             atQryFRSSIParaCnfProc },
    /* */
    { ID_MSG_FTM_RD_DLOADVER_CNF,       atQryDLoadVerCnf },
    { ID_MSG_FTM_RD_DLOADINFO_CNF,      atQryDLoadInfoCnf },
    { ID_MSG_FTM_RD_AUTHORITYVER_CNF,   atQryAuthorityVerCnf },
    { ID_MSG_FTM_RD_AUTHORITYID_CNF,    atQryAuthorityIDCnf },
    { ID_MSG_FTM_SET_NVBACKUP_CNF,      atSetNVBackupCnf },
    { ID_MSG_FTM_SET_NVRESTORE_CNF,     atSetNVRestoreCnf },
    { ID_MSG_FTM_SET_NVRSTSTTS_CNF,     atSetNVRstSTTSCnf },
    {ID_MSG_FTM_SET_TBAT_CNF,           atSetTbatCnf},
    {ID_MSG_FTM_RD_TBAT_CNF,            atRdTbatCnf},
    {ID_MSG_FTM_SET_SDLOAD_CNF,     atSetSdloadCnf },
    { ID_MSG_FTM_SET_LTCOMMCMD_CNF,     atSetLTCommCmdParaCnfProc},
    { ID_MSG_FTM_RD_LTCOMMCMD_CNF,      atQryLTCommCmdParaCnfProc},
    { ID_MSG_FTM_RD_FPLLSTATUS_CNF,     atQryFPllStatusParaCnfProc },

};


/******************************************************************************
 */
/* 函数名称: atGetFtmCnfMsgProc
 */
/* 功能描述: 根据回复的消息找到匹配的处理函数
 */
/*
 */
/* 参数说明:
 */
/*   MsgId  [in] 消息ID
 */
/* 返 回 值:
 */
/*    非NULL g_astLteAtFtmCnfMsgTbl 元素地址，包含了处理函数
 */
/*    NULL 匹配失败
 */

/******************************************************************************
 */
AT_FTM_CNF_MSG_PROC_STRU* atGetFtmCnfMsgProc(VOS_UINT32 ulMsgId)
{
    VOS_UINT32 i = 0;
    VOS_UINT32 ulTableLen = sizeof(g_astLteAtFtmCnfMsgTbl)/sizeof(g_astLteAtFtmCnfMsgTbl[0]);

    for (i=0; i<ulTableLen; i++)
    {
        if (g_astLteAtFtmCnfMsgTbl[i].ulMsgId == ulMsgId)
        {
            return &(g_astLteAtFtmCnfMsgTbl[i]);
        }
    }

    return NULL;
}

/******************************************************************************
 */
/* 函数名称: atGetFtmCnfMsgProc
 */
/* 功能描述: AT处理LTE装备FTM回复消息入口
 */
/*
 */
/* 参数说明:
 */
/*   pMsg  [in] 核间消息结构
 */
/* 返 回 值:
 */
/*    ERR_MSP_SUCCESS 成功
 */
/*    ERR_MSP_FAILURE 失败
 */

/******************************************************************************
 */
VOS_UINT32 At_FtmEventMsgProc(VOS_VOID* pMsg)
{
    VOS_UINT32 ulRet = ERR_MSP_SUCCESS;
    OS_MSG_STRU *pOsMsg = NULL;
    MsgBlock *pstMsgBlock = NULL;
    AT_FTM_CNF_MSG_PROC_STRU* pMsgProcItem = NULL;
    AT_FW_DATA_MSG_STRU *pDataMsg = (AT_FW_DATA_MSG_STRU*)pMsg;
    VOS_VOID * pTmp;


    pstMsgBlock = VOS_MemAlloc(WUEPS_PID_AT, (DYNAMIC_MEM_PT), (sizeof(MsgBlock)+sizeof(OS_MSG_STRU)-2));
    if (NULL == pstMsgBlock)
    {
    	return ERR_MSP_FAILURE;
    }

    /*消息结构转换
 */
    pOsMsg = (OS_MSG_STRU *)(pstMsgBlock->aucValue);
    pOsMsg->ulMsgId = pDataMsg->ulMsgId;
    pOsMsg->ulParam2 = pDataMsg->ulLen;

    pTmp = VOS_MemAlloc(WUEPS_PID_AT, (DYNAMIC_MEM_PT), pDataMsg->ulLen);
    if (NULL == pTmp)
    {
        VOS_MemFree(WUEPS_PID_AT, pstMsgBlock);
    	return ERR_MSP_FAILURE;
    }

    pOsMsg->ulParam1 = pTmp;


    MSP_MEMCPY((VOS_UINT8*)(pOsMsg->ulParam1), pDataMsg->pContext, pDataMsg->ulLen);

    /*消息处理
 */
    pMsgProcItem = atGetFtmCnfMsgProc(pDataMsg->ulMsgId);

    if(NULL == pMsgProcItem)
    {
        ulRet = ERR_MSP_FAILURE;
    }
    else
    {
        AT_STOP_TIMER_CMD_READY(pDataMsg->ulClientId);
        ulRet = pMsgProcItem->pfnCnfMsgProc((VOS_UINT8)(pDataMsg->ulClientId), (VOS_VOID *)pstMsgBlock);
    }

    VOS_MemFree(WUEPS_PID_AT, pTmp);
    VOS_MemFree(WUEPS_PID_AT, pstMsgBlock);
    return ulRet;
}


/*****************************************************************************
 函 数 名  : AT_ProcTempprtEventInd
 功能描述  : 处理消息ID_TEMPPRT_AT_EVENT_IND
 输入参数  : pstMsg - 来自SPY的消息
 输出参数  : 无
 返 回 值  : 无
 调用函数  :
 被调函数  :

 修改历史      :
 1.日    期   :
   作    者   :
   修改内容   :

*****************************************************************************/
VOS_UINT32    AT_ProcTempprtEventInd(
    TEMP_PROTECT_EVENT_AT_IND_STRU     *pstMsg
)
{
    VOS_UINT16                          usLength = 0;
    TEMP_PROTECT_EVENT_AT_IND_STRU     *pTempPrt = VOS_NULL;

    pTempPrt = (TEMP_PROTECT_EVENT_AT_IND_STRU*)pstMsg;

    HAL_SDMLOG("AT_ProcTempprtEventInd: Event %d Param %d\n", (VOS_INT)pTempPrt->ulTempProtectEvent, (VOS_INT)pTempPrt->ulTempProtectParam);

    usLength = (VOS_UINT16)At_sprintf(AT_CMD_MAX_LEN,(VOS_CHAR *)pgucLAtSndCodeAddr,
                            (VOS_CHAR *)pgucLAtSndCodeAddr,"%s%s%d,%d%s",
                            gaucAtCrLf, "^TEMPPRT:", pTempPrt->ulTempProtectEvent,
                            pTempPrt->ulTempProtectParam, gaucAtCrLf);

    At_SendResultData(AT_CLIENT_TAB_APP_INDEX,pgucLAtSndCodeAddr,usLength);

    return VOS_OK;
}
/*lint -restore*/




