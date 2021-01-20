/*=============================================================================
    start of file
=============================================================================*/

/*-----------------------------------------------------------------------------
    Copyright (c) 2020 Panasonic Automotive Systems Europe GmbH.
    All Rights Reserved.
    The reproduction, transmission or use of this document or its contents is
    not permitted without express written authority.
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
  Template: example_app.h (4.0.5.0)
-----------------------------------------------------------------------------*/

/**
 * @file      $HeadURL: https://svn-gl005-pais.eu.panasonic.com/GL005_PROJ_PAE15/trunk/ext/aosp/vendor/tunsdk/BroadcastRadioServicePase/TunerSdkClient.h $
 * @author    $Author: Lars Haack (70G4791) $ 
 * @version   $LastChangedRevision: 180 $
 * @Date      $LastChangedDate: 2020-09-04 14:39:22 +0200 (Fr, 04 Sep 2020) $
 *
 * @brief Brief description of this app
 *
 * detailed description of this app
 *
 */

#ifndef _TUNER_SDK_H_
#define _TUNER_SDK_H_

/*-----------------------------------------------------------------------------
    includes
-----------------------------------------------------------------------------*/
#include "tunsdk.h"
#include "tunsdk_clientbase.h"
#include "TunerSession.h"

using namespace android;

extern wp<android::hardware::broadcastradio::V2_1::implementation::TunerSession> g_tunerSession;

extern std::mutex g_session_mutex;

/*-----------------------------------------------------------------------------
    public defines
-----------------------------------------------------------------------------*/
#define HDEXT_SUBCHANNEL_BIT 32
#define HDEXT_FREQ_BIT 36
#define HD_IMAGE_SIZE_MAX (24*1024)

/*-----------------------------------------------------------------------------
    public type definitions
-----------------------------------------------------------------------------*/



/*-----------------------------------------------------------------------------
    main class section
-----------------------------------------------------------------------------*/

/**
 * @brief Brief description of the class
 *
 * detailed textual description
 */
class CTUNSDK_BroadcastRadio : public CTUNSDK_ClientBase
{
    /*override all pure virtual functions from the interface*/
    void TUNSDK_On_Connected(bool connected);
    void TUNSDK_On_Start_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_Start_Response_Handle outData);
    void TUNSDK_On_Stop_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_Stop_Response_Handle outData);
    void TUNSDK_On_Availability_Notification(Ts_TUNSDK_On_Availability_Notification_Handle outData);
    void TUNSDK_On_GetAvailability_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_GetAvailability_Response_Handle outData);
    void TUNSDK_On_SetMode_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_SetMode_Response_Handle outData);
    void TUNSDK_On_SetModeStationAmFm_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_SetModeStationAmFm_Response_Handle outData);
    void TUNSDK_On_SetModeStationDab_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_SetModeStationDab_Response_Handle outData);
    void TUNSDK_On_SelectedStation_Notification(Ts_TUNSDK_On_SelectedStation_Notification_Handle outData);
    void TUNSDK_On_SelectStationAmFm_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_SelectStationAmFm_Response_Handle outData);
    void TUNSDK_On_SelectStationDab_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_SelectStationDab_Response_Handle outData);
    void TUNSDK_On_TuneUpDown_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_TuneUpDown_Response_Handle outData);
    void TUNSDK_On_Seek_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_Seek_Response_Handle outData);
    void TUNSDK_On_SeekNonStop_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_SeekNonStop_Response_Handle outData);
    void TUNSDK_On_SeekDab_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_SeekDab_Response_Handle outData);
    void TUNSDK_On_SeekStop_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_SeekStop_Response_Handle outData);
    void TUNSDK_On_SeekCancel_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_SeekCancel_Response_Handle outData);
    void TUNSDK_On_AmStationList_Notification(Ts_TUNSDK_On_AmStationList_Notification_Handle outData);
    void TUNSDK_On_FmStationList_Notification(Ts_TUNSDK_On_FmStationList_Notification_Handle outData);
    void TUNSDK_On_DabStationList_Notification(Ts_TUNSDK_On_DabStationList_Notification_Handle outData);
    void TUNSDK_On_UpdateStationList_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_UpdateStationList_Response_Handle outData);
    void TUNSDK_On_UpdateStationListCancel_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_UpdateStationListCancel_Response_Handle outData);
    void TUNSDK_On_ConfigureSetting_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_ConfigureSetting_Response_Handle outData);
    void TUNSDK_On_ConfiguredSettings_Notification(Ts_TUNSDK_On_ConfiguredSettings_Notification_Handle outData);
    void TUNSDK_On_GetConfiguredSettings_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_GetConfiguredSettings_Response_Handle outData);
    void TUNSDK_On_GetStationList_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_GetStationList_Response_Handle outData);
    void TUNSDK_On_ResetFactoryDefault_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_ResetFactoryDefault_Response_Handle outData);
    void TUNSDK_On_TPStatus_Notification(Ts_TUNSDK_On_TPStatus_Notification_Handle outData);
    void TUNSDK_On_Announcement_Background_Notification(Ts_TUNSDK_On_Announcement_Background_Notification_Handle outData);
    void TUNSDK_On_Announcement_Start_Notification(Ts_TUNSDK_On_Announcement_Start_Notification_Handle outData);
    void TUNSDK_On_Announcement_End_Notification(Ts_TUNSDK_On_Announcement_End_Notification_Handle outData);
    //void TUNSDK_On_Announcement_Info_Notification(Ts_TUNSDK_On_Announcement_Info_Notification_Handle outData);
    void TUNSDK_On_AnnouncementCancel_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_AnnouncementCancel_Response_Handle outData);
    void TUNSDK_On_DabAudioStatus_Notification(Ts_TUNSDK_On_DabAudioStatus_Notification_Handle outData);
    void TUNSDK_On_HdAudioStatus_Notification(Ts_TUNSDK_On_HdAudioStatus_Notification_Handle outData);
    void TUNSDK_On_AmFmSetTmcService_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_AmFmSetTmcService_Response_Handle outData);
    void TUNSDK_On_AmFmSetTmcScan_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_AmFmSetTmcScan_Response_Handle outData);
    void TUNSDK_On_AmFmTmcChannel_Notification(Ts_TUNSDK_On_AmFmTmcChannel_Notification_Handle outData);
    void TUNSDK_On_AmFmTmcMessage_Notification(Ts_TUNSDK_On_AmFmTmcMessage_Notification_Handle outData);
    void TUNSDK_On_Slideshow_Notification(Ts_TUNSDK_On_Slideshow_Notification_Handle outData);
    void TUNSDK_On_PresetStore_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_PresetStore_Response_Handle outData);
    void TUNSDK_On_PresetList_Notification(Ts_TUNSDK_On_PresetList_Notification_Handle outData);
    void TUNSDK_On_PresetGetList_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_PresetGetList_Response_Handle outData);
    void TUNSDK_On_PresetRecall_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_PresetRecall_Response_Handle outData);
    void TUNSDK_On_RequireModeChange_Notification(Ts_TUNSDK_On_RequireModeChange_Notification_Handle outData);
    void TUNSDK_On_PresetClear_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_PresetClear_Response_Handle outData);
    void TUNSDK_On_PresetMove_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_PresetMove_Response_Handle outData);
    void TUNSDK_On_UpdateFirmware_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_UpdateFirmware_Response_Handle outData);
    void TUNSDK_On_SetHdTpeg_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_SetHdTpeg_Response_Handle outData);
    void TUNSDK_On_HdTpegChannel_Notification(Ts_TUNSDK_On_HdTpegChannel_Notification_Handle outData);
    void TUNSDK_On_HdTpegData_Notification(Ts_TUNSDK_On_HdTpegData_Notification_Handle outData);
    void TUNSDK_On_AmFmReceptionQuality_Notification(Ts_TUNSDK_On_AmFmReceptionQuality_Notification_Handle outData);
    void TUNSDK_On_DabAudioQuality_Notification(Ts_TUNSDK_On_DabAudioQuality_Notification_Handle outData);

   void TUNSDK_On_GetImage_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_GetImage_Response_Handle outData);
    
};

void TUNSDK_CreateThreadInit(CTUNSDK_BroadcastRadio& client);

CTUNSDK_BroadcastRadio *getClientInstance();

void getImageFromId(uint32_t id, uint8_t*buffer, uint32_t buffer_size, uint32_t*image_size);


#endif /* _TUNER_SDK_H_ */

/*=============================================================================
    end of file
=============================================================================*/
