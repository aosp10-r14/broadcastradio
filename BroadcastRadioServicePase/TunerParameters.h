/*=============================================================================
    start of file
=============================================================================*/

/*-----------------------------------------------------------------------------
    Copyright (c) 2014-2019 Panasonic Automotive Systems Europe GmbH.
    All Rights Reserved.
    The reproduction, transmission or use of this document or its contents is
    not permitted without express written authority.
-----------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------------
  Template: service_example.h (1.0.3.0)
-----------------------------------------------------------------------------*/

/**
 * @file      $HeadURL: https://svn-gl005-pais.eu.panasonic.com/GL005_PROJ_COR06/trunk/ext/aosp/vendor/panasonic/BroadcastRadioServicePase/TunerParameters.h $
 * @author    $Author: Zilan Shaik (7876015) $ 
 * @version   $LastChangedRevision: 126 $
 * @Date      $LastChangedDate: 2020-12-07 16:03:20 +0100 (Mo, 07 Dez 2020) $
 *
 * @brief Service interface definitions
 *
 */
#ifndef _TUNER_SDK_PARAMETERS_H_
#define _TUNER_SDK_PARAMETERS_H_

/*-----------------------------------------------------------------------------
    includes
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
    public defines
-----------------------------------------------------------------------------*/

/*TP */
#define SETTING_RESULT                "com.panasonic.tunsdk.setting.result"
#define RC_OK                                "TUNSDK_STATUS_OK"
#define RC_FAIL                              "TUNSDK_STATUS_FAIL"
#define RC_INVALID                           "TUNSDK_STATUS_INVALID"

/*TP */
#define SETTING_TP                    "com.panasonic.tunsdk.setting.tp"
#define TP_OFF                                "TUNSDK_SETTING_TP_OFF"
#define TP_DAB_ONLY                           "TUNSDK_SETTING_TP_DAB_ONLY"
#define TP_FM_ONLY                            "TUNSDK_SETTING_TP_FM_ONLY"
#define TP_FM_AND_DAB                         "TUNSDK_SETTING_TP_FM_AND_DAB"

/*EPG */
#define SETTING_EPG                    "com.panasonic.tunsdk.setting.epg"
#define DAB_EPG_OFF                           "TUNSDK_SETTING_EPG_DAB_OFF"
#define DAB_EPG_ON                            "TUNSDK_SETTING_EPG_DAB_ON"

/*Slide show */
#define SETTING_SLIDE_SHOW              "com.panasonic.tunsdk.setting.slideshow"
#define DAB_SLS_OFF                           "TUNSDK_SETTING_DAB_SLS_OFF"
#define DAB_SLS_ON                            "TUNSDK_SETTING_DAB_SLS_ON"
/*annoucements */
#define ANNOUNCEMENT_STATUS             "com.panasonic.tunsdk.announcement.status"
#define ANNO_FG_START                           "TUNSDK_ANNOUNCEMENT_STATUS_FG_START"
#define ANNO_FG_STOP                            "TUNSDK_ANNOUNCEMENT_STATUS_FG_STOP"
#define ANNO_BG_START                           "TUNSDK_ANNOUNCEMENT_STATUS_BG_START"
#define ANNO_BG_STOP                            "TUNSDK_ANNOUNCEMENT_STATUS_BG_STOP"

#define ANNOUNCEMENT_SOURCE             "com.panasonic.tunsdk.announcement.source"
#define ANNO_SRC_FM                           "TUNSDK_ANNOUNCEMENT_SRC_FM"
#define ANNO_SRC_DAB                          "TUNSDK_ANNOUNCEMENT_SRC_DAB"
#define TUNSDK_NOTIFY_ANNOUNCEMENT_NAME      "com.panasonic.tunsdk.announcement.name"


#endif
