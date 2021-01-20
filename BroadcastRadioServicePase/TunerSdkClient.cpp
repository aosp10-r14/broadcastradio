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
  Template: example_app.cpp (4.0.5.0)
-----------------------------------------------------------------------------*/

/**
 * @file      $HeadURL: https://svn-gl005-pais.eu.panasonic.com/GL005_PROJ_PAE15/trunk/ext/aosp/vendor/tunsdk/BroadcastRadioServicePase/TunerSdkClient.cpp $
 * @author    $Author: Lars Haack (70G4791) $ 
 * @version   $LastChangedRevision: 180 $
 * @Date      $LastChangedDate: 2020-09-04 14:39:22 +0200 (Fr, 04 Sep 2020) $
 *
 * @brief Brief description of this file
 *
 * detailed description of this file
 *
 */

/*-----------------------------------------------------------------------------
    includes
-----------------------------------------------------------------------------*/
#include "TunerSession.h"
#include "BroadcastRadio.h"
#include "resources.h"
#include <broadcastradio-utils-2x/Utils.h>
#include <android/hardware/broadcastradio/2.0/types.h>
#include "TunerSdkClient.h"
#include "TunerAnnouncements.h"
#include <semaphore.h>
#ifdef CFG_BCRADIO_PASA_ADDITIONS
  #include "PASAVendorKeyValue.h"
#endif
#include <pthread.h>

#define LOG_TAG "BcRadio.pase.tunsdkclient"
#define LOG_NDEBUG 0

#include <log/log.h>
#include <android-base/logging.h>


using namespace android;
using namespace android::hardware::broadcastradio::V2_0;
using namespace android::hardware::broadcastradio::V2_0::implementation;
using namespace android::hardware::broadcastradio;

//using android::hardware::broadcastradio::V2_0::implementation::VirtualProgram;
using android::hardware::broadcastradio::V2_1::implementation::TunerSession;
using android::hardware::hidl_vec;
using android::hardware::broadcastradio::utils::make_metadata;
using std::lock_guard;
using std::mutex;
using std::move;
using std::sort;
using std::vector;
using utils::getType;
using utils::make_metadata;

/*-----------------------------------------------------------------------------
    private defines
-----------------------------------------------------------------------------*/

/*TOD move to class*/
sem_t image_sem;
uint8_t logo_buffer[HD_IMAGE_SIZE_MAX];
uint32_t logo_length;
uint32_t ann_count;

extern int gFlag[13];

wp<TunerSession> g_tunerSession = nullptr;

std::mutex g_session_mutex;
//ProgramInfo info = {};

extern TunerAnnouncements *pgObjAnnouncements; 

/*-----------------------------------------------------------------------------
    private type definitions
-----------------------------------------------------------------------------*/

/******************************************************************************
    main class section
******************************************************************************/

/*-----------------------------------------------------------------------------
    static member initialization
-----------------------------------------------------------------------------*/
CTUNSDK_BroadcastRadio *pClient;


/*-----------------------------------------------------------------------------
    deprecated: global friend function definitions
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
    public methods definitions
-----------------------------------------------------------------------------*/
template <typename Type>
static uint32_t hidl_vec_push_back(hidl_vec<Type>* vec, const Type& value) {
    const uint32_t index = vec->size();
    vec->resize(index + 1);
    (*vec)[index] = value;
    return index;
}


ProgramSelector make_selector_identifierType_dab( uint32_t sid, uint16_t eid, uint8_t ecc, uint8_t scids, uint32_t freqKHz) {
    ProgramSelector sel = {};
    
    uint32_t sidext=((scids & 0x0f) <<24) | ((ecc & 0xff) <<16) | (sid & 0xffff);
    
//    ALOGV("make_selector_identifierType_dab  sid 0x%x   eid 0x%x  ecc 0x%x  scids 0x%x  freqKHz %d ", sid, eid, ecc, scids, freqKHz);
    
    sel.secondaryIds = hidl_vec<ProgramIdentifier>({});

    if( sidext != 0 ) {
        sel.primaryId = utils::make_identifier( IdentifierType::DAB_SID_EXT, sidext );
    } else {
        sel.primaryId = utils::make_identifier( IdentifierType::DAB_FREQUENCY, freqKHz );
    }   
    
    hidl_vec_push_back( &(sel.secondaryIds), utils::make_identifier(IdentifierType::DAB_FREQUENCY, freqKHz) );
    hidl_vec_push_back( &(sel.secondaryIds), utils::make_identifier(IdentifierType::DAB_ENSEMBLE,  eid) );
    
//    ALOGV("make_selector_identifierType_dab  sel %s ", toString(sel).c_str());
    
    return sel;
}


ProgramSelector make_selector_identifierType_hd( uint32_t stationid, uint32_t freq, uint8_t mcast, uint32_t pi) {
    ProgramSelector sel = {};  
    uint64_t hdChannelInfo = 0;
     //TODO station id needs to be used , but currently it is not used. we are using freq
    uint64_t stationId = freq & 0xFFFFFFFF;
    uint64_t subChannel =mcast& 0xF;
    uint64_t frequency = freq & 0x3FFFF;

    hdChannelInfo = (frequency << HDEXT_FREQ_BIT) | (subChannel << HDEXT_SUBCHANNEL_BIT) | stationId;
    sel.primaryId = utils::make_identifier( IdentifierType::HD_STATION_ID_EXT, hdChannelInfo );
    sel.secondaryIds = hidl_vec<ProgramIdentifier>({});
     if (((pi & 0x0000ffffu) != 0x0000) && ((pi & 0x0000ffffu) != 0xffff)){
      hidl_vec_push_back( &(sel.secondaryIds), utils::make_identifier(IdentifierType::RDS_PI, pi) );
      hidl_vec_push_back( &(sel.secondaryIds), utils::make_identifier(IdentifierType::AMFM_FREQUENCY,  freq) );
     }
     else
     {
       hidl_vec_push_back( &(sel.secondaryIds), utils::make_identifier(IdentifierType::AMFM_FREQUENCY,  freq) );
     }
    return sel;
}


ProgramSelector make_selector_identifierType_fm( uint32_t freq, uint32_t pi) {
    ProgramSelector sel = {};
    sel.secondaryIds = hidl_vec<ProgramIdentifier>({});  
    
     if (((pi & 0x0000ffffu) != 0x0000) && ((pi & 0x0000ffffu) != 0xffff)){
      sel.primaryId = utils::make_identifier( IdentifierType::RDS_PI,pi );
      hidl_vec_push_back( &(sel.secondaryIds), utils::make_identifier(IdentifierType::AMFM_FREQUENCY,  freq) );
     }
     else
     {
       sel.primaryId = utils::make_identifier( IdentifierType::AMFM_FREQUENCY, freq );
     }
    return sel;
}


void  getNameFromFrequency(uint32_t freq, uint32_t band,  char *stationName, uint8_t size)
{
  std::string freqString = {};
  if (band == TUNSDK_BAND_FM)
  {    
    uint32_t  integer = freq / 1000;
    uint32_t rest = (freq % 1000) / 100;
    freqString = std::to_string(integer) + '.' + std::to_string(rest) + " FM";
    if (freqString.size() < size)
    {
      freqString.copy(stationName, freqString.size() + 1);
      stationName[freqString.size()] = '\0';
    }
  }
  else
  {   
    freqString = std::to_string(freq) + " AM";
    if (freqString.size() < size)
    {
      freqString.copy(stationName, freqString.size() + 1);
      stationName[freqString.size()] = '\0';
    }
  }
  
}


void getImageFromId(uint32_t id, uint8_t*buffer, uint32_t buffer_size, uint32_t*image_size) {
  Te_tunsdk_status status;
  if (pClient != __nullptr)
  {
    LOG(DEBUG) << "TUNSDK_GetImage " << std::hex << id;
    // memset( &logo_buffer[0],0,sizeof( logo_buffer));
    //logo_length = 0;
    status = pClient->TUNSDK_GetImage(0, id);
    LOG(DEBUG) << "wait for semaphore ";
    /*wait for the response*/
    sem_wait(&image_sem);
    //LOG(DEBUG) << "after semaphore ";
    //LOG(DEBUG) << "image length  " << logo_length;
    if ((logo_length > 0) && (logo_length <= buffer_size))
    {
      memcpy(buffer, &logo_buffer[0], logo_length);
      LOG(DEBUG) << "image length  " << logo_length;
      *image_size = logo_length;
    }
  }
}


CTUNSDK_BroadcastRadio *getClientInstance()
{
  CTUNSDK_BroadcastRadio& client = *pClient;
  return & client;
}


void TUNSDK_TestClient_On_StartCb(void)
{
    ALOGV("TUNSDK_TestClient_On_StartCb \n"); 
}


static void* TUNSDK_Communication(void* arg)
{
  int status;
  CTUNSDK_BroadcastRadio& client = *pClient;
   
  if( arg==NULL)
  {
      ALOGV("TUNSDK_Communication \n");
  }
  /*initialize semaphore*/
  sem_init(&image_sem,0,0);
  (void)pthread_setname_np(pthread_self(), "TUNSDK_Communication");
  status = CTUNSDK::initCommunication("TestClient", &client, TUNSDK_TestClient_On_StartCb);
  
  return  NULL;
}


void TUNSDK_CreateThreadInit(CTUNSDK_BroadcastRadio& client)
{
  pthread_t gui;
  pthread_attr_t attr;
  sp<TunerSession> currentSession=nullptr;
  {
    lock_guard<mutex> lk(g_session_mutex);
    currentSession = g_tunerSession.promote();
  }
  pClient = &client;
  ann_count = 0;

  /*lint -save -e586 Warning: function 'printf' is deprecated. [MISRA 2012 Rule 21.8, required] */
   ALOGV("TUNSDK_Communication: Create \"GUI\" thread\n");
  /*lint -restore */
  (void)pthread_attr_init(&attr);
  (void)pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  (void)pthread_create(&gui, &attr, &TUNSDK_Communication, NULL);
  // err = pthread_create(&tid, NULL, &TUNSDK_HMI_GUI, NULL);
}


inline const char* boolOO(bool in)
{
  const char* myReturn = "OFF";
  if (in) 
  {
    myReturn ="ON"; 
  }
  return myReturn;
}


void CTUNSDK_BroadcastRadio::TUNSDK_On_Connected(bool connected)
{
  if (connected)
  {
    ALOGV("HMI: Connected, calling TUNSDK_Start!\n");
    TUNSDK_Start(0, TUNSDK_START_MODE_NORMAL);
  }
  else
  {
    ALOGV("HMI: Disconnected!\n");
  }
}


void CTUNSDK_BroadcastRadio::TUNSDK_On_Start_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_Start_Response_Handle outData)
{
    (void)transId;
    (void)outData;
    ALOGV("HMI: TUNSDK_On_Start_Response with result code %d\n", result);
   
    Te_tunsdk_status status = TUNSDK_STATUS_INVALID;  
    status = TUNSDK_RegisterForObserver(TUNSDK_OBSERVER_ID_Availability, 1);  
    status = TUNSDK_RegisterForObserver(TUNSDK_OBSERVER_ID_TPStatus, 1);  
    status = TUNSDK_RegisterForObserver(TUNSDK_OBSERVER_ID_Announcement_Background, 1);  
    status = TUNSDK_RegisterForObserver(TUNSDK_OBSERVER_ID_Announcement_Start, 1);  
    status = TUNSDK_RegisterForObserver(TUNSDK_OBSERVER_ID_Announcement_End, 1);  
    //status = TUNSDK_RegisterForObserver(TUNSDK_OBSERVER_ID_Announcement_Info, 1);  
    status = TUNSDK_RegisterForObserver(TUNSDK_OBSERVER_ID_HdEmergencyWarningAlert, 1);  
}


void CTUNSDK_BroadcastRadio::TUNSDK_On_Stop_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_Stop_Response_Handle outData)
{
  (void)transId;
  (void)outData;
   ALOGV("HMI: TUNSDK_On_Stop_Response with result code %d\n", result);
}


void CTUNSDK_BroadcastRadio::TUNSDK_On_SetMode_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_SetMode_Response_Handle outData)
{
  (void)transId;
  (void)outData;
   ALOGV("TUNSDK_On_SetMode_Response: %d\n", result);
}


void CTUNSDK_BroadcastRadio::TUNSDK_On_GetAvailability_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_GetAvailability_Response_Handle outData)
{
  (void)transId;
  (void)outData; 
  (void)result; 
  ALOGV("TUNSDK_On_GetAvailability_Response: %s\n", boolOO(outData.available()));
}


void CTUNSDK_BroadcastRadio::TUNSDK_On_SetModeStationAmFm_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_SetModeStationAmFm_Response_Handle outData)
{
  (void)transId;
  (void)outData;
   ALOGV("HMI: TUNSDK_On_SetModeStationAmFm_Response with result code %d\n", result);
}
void CTUNSDK_BroadcastRadio::TUNSDK_On_SetModeStationDab_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_SetModeStationDab_Response_Handle outData)
{
  (void)transId;
  (void)outData;
   ALOGV("HMI: TUNSDK_On_SetModeStationDab_Response with result code %d\n", result);
}
void CTUNSDK_BroadcastRadio::TUNSDK_On_SelectStationAmFm_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_SelectStationAmFm_Response_Handle outData)
{
  (void)transId;
  (void)outData;
   ALOGV("HMI: TUNSDK_On_SelectStationAmFm_Response with result code %d\n", result);
}
void CTUNSDK_BroadcastRadio::TUNSDK_On_SelectStationDab_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_SelectStationDab_Response_Handle outData)
{
  (void)transId;
  (void)outData; 
  ALOGV("HMI: TUNSDK_On_SelectStationDab_Response with result code %d\n", result);
}
void CTUNSDK_BroadcastRadio::TUNSDK_On_TuneUpDown_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_TuneUpDown_Response_Handle outData)
{
    (void)transId;
  (void)outData;
   ALOGV("TUNSDK_On_TuneUpDown_Response: %d\n", result);
}
void CTUNSDK_BroadcastRadio::TUNSDK_On_Seek_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_Seek_Response_Handle outData)
{
    (void)transId;
  (void)outData;
   ALOGV("TUNSDK_On_Seek_Response: %d\n", result);
}
void CTUNSDK_BroadcastRadio::TUNSDK_On_SeekNonStop_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_SeekNonStop_Response_Handle outData)
{
    (void)transId;
  (void)outData;
   ALOGV("TUNSDK_On_SeekNonStop_Response: %d\n", result);
}
void CTUNSDK_BroadcastRadio::TUNSDK_On_SeekStop_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_SeekStop_Response_Handle outData)
{
    (void)transId;
  (void)outData;
   ALOGV("TUNSDK_On_SeekStop_Response: %d\n", result);
}
void CTUNSDK_BroadcastRadio::TUNSDK_On_SeekCancel_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_SeekCancel_Response_Handle outData)
{
    (void)transId;
  (void)outData;
   ALOGV("TUNSDK_On_SeekCancel_Response: %d\n", result);
}
void CTUNSDK_BroadcastRadio::TUNSDK_On_SeekDab_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_SeekDab_Response_Handle outData)
{
    (void)transId;
  (void)outData;
   ALOGV("HMI: TUNSDK_On_SeekDab_Response with result code %d\n", result);
}
void CTUNSDK_BroadcastRadio::TUNSDK_On_UpdateStationList_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_UpdateStationList_Response_Handle outData)
{
    (void)transId;
  (void)outData;
   ALOGV("TUNSDK_On_UpdateStationList_Response: %d\n", result);
}
void CTUNSDK_BroadcastRadio::TUNSDK_On_UpdateStationListCancel_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_UpdateStationListCancel_Response_Handle outData)
{
    (void)transId;
  (void)outData;
   ALOGV("TUNSDK_On_UpdateStationListCancel_Response: %d\n", result);
}
void CTUNSDK_BroadcastRadio::TUNSDK_On_ConfigureSetting_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_ConfigureSetting_Response_Handle outData)
{
    (void)transId;
  (void)outData;
   ALOGV("TUNSDK_On_ConfigureSetting_Response: %d\n", result);
}
void CTUNSDK_BroadcastRadio::TUNSDK_On_GetConfiguredSettings_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_GetConfiguredSettings_Response_Handle outData)
{
    (void)transId;
    (void)outData;
    ALOGV("TUNSDK_On_GetConfiguredSettings_Response: %d \n", result);
}
void CTUNSDK_BroadcastRadio::TUNSDK_On_ResetFactoryDefault_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_ResetFactoryDefault_Response_Handle outData)
{
    (void)transId;
    (void)outData;
    (void)result;
    ALOGV("TUNSDK_On_ResetFactoryDefault_Response \n");
}

void CTUNSDK_BroadcastRadio::TUNSDK_On_GetStationList_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_GetStationList_Response_Handle outData)
{
    (void)transId;
    (void)outData;
  ALOGV("TUNSDK_On_GetStationList_Response:  result %d \n", result);
}

void CTUNSDK_BroadcastRadio::TUNSDK_On_AnnouncementCancel_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_AnnouncementCancel_Response_Handle outData)
{
    (void)transId;
    (void)outData;
    ALOGV("TUNSDK_On_AnnouncementCancel_Response:  result %d \n", result);
}



/*===========================================================================*/
/*                     Notifications                                         */
/*===========================================================================*/

void CTUNSDK_BroadcastRadio::TUNSDK_On_Availability_Notification(Ts_TUNSDK_On_Availability_Notification_Handle outData)
{
    bool avail = outData.available();
  
    ALOGV("%s %d\n", __func__, avail);

    if (avail)
    {
//        ALOGV("  Available, Set mode -> background scanning \n");
//        TUNSDK_SetMode(1, TUNSDK_MODE_BACKGROUND);
        ALOGV("  Available, Set mode FM!\n");
        TUNSDK_SetMode(1, TUNSDK_MODE_FM);

        TUNSDK_GetConfiguredSettings(0);
    }
}


void CTUNSDK_BroadcastRadio::TUNSDK_On_AmStationList_Notification(Ts_TUNSDK_On_AmStationList_Notification_Handle outData)
{
  static const uint32_t MAXSTATIONLISTSIZE = 50;
  auto list = outData.stationList();
  ALOGV("Station list received for AM stations %d (display limited to %d)\n",  list.nrStations(), MAXSTATIONLISTSIZE);
  uint32_t printStations = list.nrStations() < MAXSTATIONLISTSIZE ? list.nrStations() : MAXSTATIONLISTSIZE;
  Ts_tunsdk_amfmhd_station_Handle station(0);
  bool listUpdateAM = false;
  ProgramInfo programinfo;
  ProgramSelector stationinfo;
  ProgramSelector selector;
 
  sp<TunerSession> currentSession = nullptr;
  {
    lock_guard<mutex> lk(g_session_mutex);
    if (g_tunerSession != nullptr)
    currentSession = g_tunerSession.promote();
  }


//TODO limit display to printStations number, but propagate complete list to HMI
  
  for (uint32_t i = 0; i < printStations; i++)
  {
    if (list.stations_ElementAt(i, station))
    {
     
    
        auto amfmTunable = station.amfmhdTunable();
        auto hdData = station.hdData();

        ALOGV("STL: AM: %d: freqKHz: %d pi: %x ps: %s pty: %d mcast: %d \n", i, amfmTunable.freqKHz(), amfmTunable.pi(), station.ps(), station.pty(), amfmTunable.mcast());
        ProgramInfo programinfo = {};
        if (currentSession != nullptr)
        { 
//TODO fake name necessary for PASA?        
        char freqname[16] ={" "};
        getNameFromFrequency(amfmTunable.freqKHz(),TUNSDK_BAND_AM_MW, &freqname[0], sizeof(freqname));
        programinfo.selector.primaryId = utils::make_identifier( IdentifierType::AMFM_FREQUENCY, amfmTunable.freqKHz());
        if( !(currentSession->mProgramListAM.empty()) && (listUpdateAM == false)  )
          {
              currentSession->mProgramListAM.clear();
              listUpdateAM = true;
          }
          programinfo.metadata = hidl_vec<Metadata>({});
          hidl_vec_push_back(&(programinfo.metadata), make_metadata(MetadataKey::RDS_PS, std::string((char*)&freqname[0])));
         currentSession->mProgramListAM.push_back( programinfo );
      }
    
    }
  }
  

    if( currentSession != nullptr) 
    {
        ProgramListChunk chunk = {};
      
        chunk.purge = true;
        chunk.complete = true;
        std::vector<ProgramInfo> listSend;
        bool listupdate = false;

        if((currentSession->mMode == TUNSDK_MODE_AM_MW))
        {        
            listSend = currentSession->mProgramListAM;
            if(!(listSend.empty()) && (listUpdateAM==true))
            {
              listupdate = true;
              ALOGV("  Callback TUNSDK_On_AMStationList_Notification() ");

              //    std::sort(listSend.begin(), listSend.end());
              chunk.modified = hidl_vec<ProgramInfo>(listSend.begin(), listSend.end());
              (currentSession->mCallback)->onProgramListUpdated(chunk);
            }
        }
    }
}

void CTUNSDK_BroadcastRadio::TUNSDK_On_FmStationList_Notification(Ts_TUNSDK_On_FmStationList_Notification_Handle outData)
{
  static const uint32_t MAXSTATIONLISTSIZE = 50;
  auto list = outData.stationList();
  uint32_t printStations = list.nrStations() < MAXSTATIONLISTSIZE ? list.nrStations() : MAXSTATIONLISTSIZE;
  Ts_tunsdk_amfmhd_station_Handle station(0);
  bool listUpdateFM = false;
  ProgramInfo programinfo;
  ProgramSelector stationinfo;
  ProgramSelector selector;

  sp<TunerSession> currentSession = nullptr;
  {
    lock_guard<mutex> lk(g_session_mutex);
    if (g_tunerSession != nullptr) currentSession = g_tunerSession.promote();
  }

//  ALOGV("Station list received for FM stations %d (display limited to %d) mMode %d \n",  list.nrStations(), MAXSTATIONLISTSIZE, currentSession->mMode);
  ALOGV("Station list received for FM stations %d (display limited to %d) mMode \n",  list.nrStations(), MAXSTATIONLISTSIZE);


  //TODO limit display to printStations number, but propagate complete list to HMI

  for (uint32_t i = 0; i < printStations; i++)
  {
    if (list.stations_ElementAt(i, station))
    {
     
        auto amfmTunable = station.amfmhdTunable();
        auto hdData = station.hdData();
        ALOGV("STL: FM %d: freqKHz: %d pi: %x ps: %s pty: %d mcast: %d \n", i, amfmTunable.freqKHz(), amfmTunable.pi(), station.ps(), station.pty(), amfmTunable.mcast());

        ProgramInfo programinfo = {};

        if (currentSession != nullptr)
        {
          if (!(currentSession->mProgramListFM.empty()) && (listUpdateFM == false))
          {
            currentSession->mProgramListFM.clear();
            listUpdateFM = true;
            //                  ALOGV("  mProgramListFM cleared, listUpdateFM true");
          }

          if (station.hd())
          {
            programinfo.selector = make_selector_identifierType_hd(amfmTunable.freqKHz(), amfmTunable.freqKHz(),amfmTunable.mcast(), amfmTunable.pi());
            
            
            programinfo.metadata = hidl_vec<Metadata>({});
            
            if ((std::string((char*)&hdData.hd_station_name())).length() > 0) {
            hidl_vec_push_back(&(programinfo.metadata), make_metadata(MetadataKey::PROGRAM_NAME, std::string((char*)&hdData.hd_station_name())));
            ALOGV("  PROGRAM_NAME     '%s' ", hdData.hd_station_name());
            }
            if ((std::string((char*)&station.ps())).length() > 0) {
              hidl_vec_push_back(&(programinfo.metadata), make_metadata(MetadataKey::RDS_PS, std::string((char*)&station.ps())));
              //ALOGV("  RDS_PS     '%s' ", station.ps());
            }

           LOG(DEBUG) << "  FM/HD station " << toString(programinfo.selector);

            
          }
          else
          {
            programinfo.selector = make_selector_identifierType_fm(amfmTunable.freqKHz(), amfmTunable.pi());
            programinfo.metadata = hidl_vec<Metadata>({});

             if ((std::string((char*)&station.ps())).length() > 0) {
               hidl_vec_push_back(&(programinfo.metadata), make_metadata(MetadataKey::RDS_PS, std::string((char*)&station.ps())));
            //ALOGV("  RDS_PS     '%s' ", station.ps());
             }
             
           LOG(DEBUG) << "  FM station " << toString(programinfo.selector);
             
          }
          currentSession->mProgramListFM.push_back(programinfo);
        }
    }
  }


  if (currentSession != nullptr && currentSession->progListReqInfo.isProgramListRequested == true)
  {
    ProgramListChunk chunk = {};

    chunk.purge = true;
    chunk.complete = true;
    std::vector<ProgramInfo> list;
    bool listupdate = false;

    if (currentSession->mMode == TUNSDK_MODE_FM)
    {
  
      list = currentSession->mProgramListFM;
      
      if (!(list.empty()) && (listUpdateFM == true))
      {
        listupdate = true;

        if( currentSession->mCombinedStlDabFm==true ) {
            list.insert( list.end(), currentSession->mProgramListDAB.begin(), currentSession->mProgramListDAB.end() );
            ALOGV("  Callback onProgramListUpdated() - Combined DAB/FM station list, %d entries", list.size() );
        } else {
            ALOGV("  Callback onProgramListUpdated() - FM station list, %d entries", list.size() );
        }        
        
        
        //std::sort(listSend.begin(), listSend.end());
        chunk.modified = hidl_vec<ProgramInfo>(list.begin(), list.end());
        (currentSession->mCallback)->onProgramListUpdated(chunk);
      }
    }
  }
}

void CTUNSDK_BroadcastRadio::TUNSDK_On_DabStationList_Notification(Ts_TUNSDK_On_DabStationList_Notification_Handle outData)
{
  static const uint32_t MAXSTATIONLISTSIZE = 50;
  auto list = outData.stationList();
  uint32_t printStations = list.nrStations() < MAXSTATIONLISTSIZE ? list.nrStations() : MAXSTATIONLISTSIZE;
  Ts_tunsdk_dab_station_Handle station(0);
  bool listUpdateDAB = false;

  ProgramInfo programinfo;
  ProgramSelector stationinfo;
  ProgramSelector selector;

  sp<TunerSession> currentSession = nullptr;
  {
    lock_guard<mutex> lk(g_session_mutex);
    if (g_tunerSession != nullptr) currentSession = g_tunerSession.promote();
  }

//  ALOGV("Station list received for DAB stations %d (display limited to %d) mMode %d\n",  list.nrStations(), MAXSTATIONLISTSIZE, currentSession->mMode);
  ALOGV("Station list received for DAB stations %d (display limited to %d) mMode \n",  list.nrStations(), MAXSTATIONLISTSIZE);


  //TODO limit display to printStations number, but propagate complete list to HMI

  for (uint32_t i = 0; i < printStations; i++)
  {
    if (list.stations_ElementAt(i, station))
    {

     
      auto dabTunable = station.dabTunable();
      auto label = station.stationLabel();
      auto ensembleLabel = station.ensembleLabel();
      auto channel = station.channelLabel();

      ProgramInfo programinfo = {};

      ALOGV("STL: DAB %d: channel %s ensemble: '%s' label: '%s' eid: %x sid: %x scids: %x",
        i, channel, ensembleLabel.label(), label.label(), dabTunable.eid(), dabTunable.sid(), dabTunable.scids());

      programinfo.selector = make_selector_identifierType_dab(dabTunable.sid(), dabTunable.eid(), dabTunable.ecc(), dabTunable.scids(), dabTunable.freqKHz());

      LOG(DEBUG) << "  DAB station " << toString(programinfo.selector);
      
      programinfo.metadata = hidl_vec<Metadata>({});
      
      hidl_vec_push_back(&(programinfo.metadata), make_metadata(MetadataKey::DAB_SERVICE_NAME, std::string((char*)&label.label())));

      //        programinfo.metadata = hidl_vec<Metadata>({
      //            make_metadata(MetadataKey::DAB_SERVICE_NAME,        std::string((char*)&label.label()) ),
      //            make_metadata(MetadataKey::DAB_SERVICE_NAME_SHORT,  std::string((char*)&label.shortLabel() ),
      //            make_metadata(MetadataKey::DAB_ENSEMBLE_NAME,       std::string((char*)&ensembleLabel.label() ),
      //            make_metadata(MetadataKey::DAB_ENSEMBLE_NAME_SHORT, std::string((char*)&ensembleLabel.shortLabel() ),
      //            make_metadata(MetadataKey::STATION_ICON, resources::dabLogoPngId),
      //        });

      if (currentSession != NULL)
      {
        if (!(currentSession->mProgramListDAB.empty()) && (listUpdateDAB == false))
        {
          currentSession->mProgramListDAB.clear();
          listUpdateDAB = true;
          ALOGV("  mProgramListDAB cleared, listUpdateDAB true");
        }

        currentSession->mProgramListDAB.push_back(programinfo);
      }
    }
  }

  if (currentSession != nullptr && currentSession->progListReqInfo.isProgramListRequested == true)
  {
    ProgramListChunk chunk = {};

    chunk.purge = true;
    chunk.complete = true;
    std::vector<ProgramInfo> list;
    bool listupdate = false;

    if (currentSession->mMode == TUNSDK_MODE_DAB || currentSession->mCombinedStlDabFm)
    {

      list = currentSession->mProgramListDAB;
      
      if (!(list.empty()) && (listUpdateDAB == true))
      {
        listupdate = true;

        //    std::sort(listSend.begin(), listSend.end());

        if( currentSession->mCombinedStlDabFm==true ) {
            list.insert( list.end(), currentSession->mProgramListFM.begin(), currentSession->mProgramListFM.end() );
            ALOGV("  Callback onProgramListUpdated() - Combined DAB/FM station list, %d entries", list.size() );
        } else {
            ALOGV("  Callback onProgramListUpdated() - DAB station list, %d entries", list.size() );
        }        

        chunk.modified = hidl_vec<ProgramInfo>(list.begin(), list.end());
        (currentSession->mCallback)->onProgramListUpdated(chunk);
      }

    }
  }
}


void CTUNSDK_BroadcastRadio::TUNSDK_On_SelectedStation_Notification(Ts_TUNSDK_On_SelectedStation_Notification_Handle outData)
{
  Ts_tunsdk_station_Handle  station    = outData.station();
  Ts_tunsdk_metaData_Handle meta       = outData.stationMetaData();
  Te_tunsdk_radioState      radioState = outData.radioState();
  ProgramInfo selectedProgramInfo = {};
  
 
  sp<TunerSession> currentSession=nullptr;
  {
    lock_guard<mutex> lk(g_session_mutex);
    currentSession = g_tunerSession.promote();
  }

  if( currentSession != nullptr ) {
      currentSession->mRadioState = radioState;
  }

  ALOGV("TUNSDK_On_SelectedStation_Notification  mMode %d \n", currentSession->mMode);
  
  switch (station.band())
  {

  case TUNSDK_BAND_AM_MW:
  {
    Ts_tunsdk_amfmhd_station_Handle amfmStation = station.station().amfm();
    auto amfmTunable = amfmStation.amfmhdTunable();
    Ts_tunsdk_amfmhd_metaData_Handle amfmMeta = meta.metaData().amfm();
    if (amfmStation.hd())
    {
      auto hdData = amfmStation.hdData();
  
      selectedProgramInfo.selector = make_selector_identifierType_hd(amfmTunable.freqKHz(),amfmTunable.freqKHz(),amfmTunable.mcast(),amfmTunable.pi());
      selectedProgramInfo.metadata = hidl_vec<Metadata>({});
      if ((std::string((char*)&amfmMeta.songTitle())).length() > 0){
      hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::SONG_TITLE, std::string((char*)&amfmMeta.songTitle())));
      ALOGV(" HD SONG_TITLE     '%s' ", amfmMeta.songTitle());
      }
      if ((std::string((char*)&amfmMeta.songArtist())).length() > 0){
      hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::SONG_ARTIST, std::string((char*)&amfmMeta.songArtist())));
      ALOGV(" HD SONG_ARTIST     '%s' ", amfmMeta.songArtist());
      } 
      if ((std::string((char*)&amfmMeta.radioText())).length() > 0){
      hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::RDS_RT, std::string((char*)&amfmMeta.radioText())));
      ALOGV(" HD RDS_RT     '%s' ", amfmMeta.radioText());
      }
      if ((std::string((char*)&amfmStation.ps())).length() > 0){
      hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::RDS_PS, std::string((char*)&amfmStation.ps())));
      ALOGV(" HD RDS_PS     '%s' ", amfmStation.ps());
      }
      if ((std::string((char*)&amfmMeta.hdLongStationName())).length() > 0){
      hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::PROGRAM_NAME, std::string((char*)&amfmMeta.hdLongStationName())));
      ALOGV(" HD PROGRAM_NAME     '%s' ",amfmMeta.hdLongStationName());
      }
      if (hdData.hdPty() != 0){
      hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::RBDS_PTY,   hdData.hdPty()));
      ALOGV(" HD RDS_PTY     '%d' ", amfmStation.pty());
      }          
 #ifdef CFG_BCRADIO_PASA_ADDITIONS
      /*send station logo and album art as album art*/
      if( amfmMeta.albumArtId() != 0){
      hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::ALBUM_ART,   amfmMeta.albumArtId()));
      ALOGV(" HD ALBUM_ART     '0x%x' ", amfmMeta.albumArtId());
      } 
      else  if (hdData.logoId() != 0){
      hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::ALBUM_ART,  hdData.logoId()));
      ALOGV(" HD STATION_ICON     '0x%x' ", hdData.logoId());
      } 
 #else     
       if (hdData.logoId() != 0){
      hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::STATION_ICON,  hdData.logoId()));
      ALOGV(" HD STATION_ICON     '0x%x' ", hdData.logoId());
      } 
      if( amfmMeta.albumArtId() != 0){
      hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::ALBUM_ART,   amfmMeta.albumArtId()));
      ALOGV(" HD ALBUM_ART     '0x%x' ", amfmMeta.albumArtId());
      }
#endif     
      selectedProgramInfo.logicallyTunedTo = utils::make_identifier(IdentifierType::HD_STATION_ID_EXT, utils::getId(selectedProgramInfo.selector, IdentifierType::HD_STATION_ID_EXT));   
      selectedProgramInfo.physicallyTunedTo = utils::make_identifier(IdentifierType::AMFM_FREQUENCY, amfmTunable.freqKHz());
    }
    else
    {
      selectedProgramInfo.selector = make_selector_identifierType_fm(amfmTunable.freqKHz(),amfmTunable.pi());
      selectedProgramInfo.logicallyTunedTo = utils::make_identifier(IdentifierType::AMFM_FREQUENCY, utils::getId(selectedProgramInfo.selector, IdentifierType::AMFM_FREQUENCY));  
      selectedProgramInfo.physicallyTunedTo = utils::make_identifier(IdentifierType::AMFM_FREQUENCY, amfmTunable.freqKHz());
    }
    selectedProgramInfo.infoFlags |= ProgramInfoFlags::TUNED;
    selectedProgramInfo.infoFlags |= ProgramInfoFlags::LIVE;
    
    if( currentSession != nullptr ) {
        (currentSession->mCallback)-> onCurrentProgramInfoChanged(selectedProgramInfo);
        currentSession->mSelectedProgramInfo = selectedProgramInfo;
    }
  }
  break;
  case TUNSDK_BAND_FM:
  {
    Ts_tunsdk_amfmhd_station_Handle amfmStation = station.station().amfm();
    auto amfmTunable = amfmStation.amfmhdTunable();
    Ts_tunsdk_amfmhd_metaData_Handle amfmMeta = meta.metaData().amfm();
    
    uint32_t signalQuality = amfmStation.quality();   /* Signal quality in range 0 to 100 */
    if( signalQuality > 100) { signalQuality=100; }
    
    if (amfmStation.hd())
    {
      ALOGV(" HD  freqKHz %d  pi %x  radioState %d  signalQuality %d ", amfmTunable.freqKHz(), amfmTunable.pi(), radioState, signalQuality);

      auto hdData = amfmStation.hdData();
      selectedProgramInfo.selector = make_selector_identifierType_hd(amfmTunable.freqKHz(),amfmTunable.freqKHz(),amfmTunable.mcast(),amfmTunable.pi());
      selectedProgramInfo.signalQuality = signalQuality;
      selectedProgramInfo.metadata = hidl_vec<Metadata>({});
      
      if ((std::string((char*)&amfmMeta.songTitle())).length() > 0){
      hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::SONG_TITLE, std::string((char*)&amfmMeta.songTitle())));
      ALOGV(" HD SONG_TITLE     '%s' ", amfmMeta.songTitle());
      }
      if ((std::string((char*)&amfmMeta.songArtist())).length() > 0){
      hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::SONG_ARTIST, std::string((char*)&amfmMeta.songArtist())));
      ALOGV(" HD SONG_ARTIST     '%s' ", amfmMeta.songArtist());
      } 
      if ((std::string((char*)&amfmMeta.radioText())).length() > 0){
      hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::RDS_RT, std::string((char*)&amfmMeta.radioText())));
      ALOGV(" HD RDS_RT     '%s' ", amfmMeta.radioText());
      }
      if ((std::string((char*)&amfmStation.ps())).length() > 0){
      hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::RDS_PS, std::string((char*)&amfmStation.ps())));
      ALOGV(" HD RDS_PS     '%s' ", amfmStation.ps());
      }
       if ((std::string((char*)&amfmMeta.hdLongStationName())).length() > 0){
      hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::PROGRAM_NAME, std::string((char*)&amfmMeta.hdLongStationName())));
      ALOGV(" HD PROGRAM_NAME     '%s' ",amfmMeta.hdLongStationName());
      }
      if (hdData.hdPty() != 0){
      hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::RBDS_PTY,   hdData.hdPty()));
      ALOGV(" HD RDS_PTY     '%d' ", amfmStation.pty());
      }          
#ifdef CFG_BCRADIO_PASA_ADDITIONS
      /*send station logo and album art as album art*/
      if( amfmMeta.albumArtId() != 0){
      hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::ALBUM_ART,   amfmMeta.albumArtId()));
      ALOGV(" HD ALBUM_ART     '0x%x' ", amfmMeta.albumArtId());
      } 
      else  if (hdData.logoId() != 0){
      hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::ALBUM_ART,  hdData.logoId()));
      ALOGV(" HD STATION_ICON     '0x%x' ", hdData.logoId());
      } 
 #else     
       if (hdData.logoId() != 0){
      hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::STATION_ICON,  hdData.logoId()));
      ALOGV(" HD STATION_ICON     '0x%x' ", hdData.logoId());
      } 
      if( amfmMeta.albumArtId() != 0){
      hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::ALBUM_ART,   amfmMeta.albumArtId()));
      ALOGV(" HD ALBUM_ART     '0x%x' ", amfmMeta.albumArtId());
      }
#endif        
    }
    else
    {
      ALOGV(" FM  freqKHz %d  pi %x  radioState %d  signalQuality %d ", amfmTunable.freqKHz(), amfmTunable.pi(), radioState, signalQuality);

      selectedProgramInfo.selector = make_selector_identifierType_fm(amfmTunable.freqKHz(), amfmTunable.pi());
      selectedProgramInfo.signalQuality = signalQuality;
      selectedProgramInfo.metadata = hidl_vec<Metadata>({});
      
      if ((std::string((char*)&amfmMeta.songTitle())).length() > 0){
      hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::SONG_TITLE, std::string((char*)&amfmMeta.songTitle())));
      ALOGV(" FM SONG_TITLE     '%s' ", amfmMeta.songTitle());
      }
      if ((std::string((char*)&amfmMeta.songArtist())).length() > 0){
      hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::SONG_ARTIST, std::string((char*)&amfmMeta.songArtist())));
      ALOGV(" FM SONG_ARTIST     '%s' ", amfmMeta.songArtist());
      } 
      if ((std::string((char*)&amfmMeta.radioText())).length() > 0){
      hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::RDS_RT, std::string((char*)&amfmMeta.radioText())));
      ALOGV(" FM RDS_RT     '%s' ", amfmMeta.radioText());
      }
      if ((std::string((char*)&amfmStation.ps())).length() > 0){
      hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::RDS_PS, std::string((char*)&amfmStation.ps())));
      ALOGV(" FM RDS_PS     '%s' ", amfmStation.ps());
      }
      if ( amfmStation.pty() != 0){
      hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::RDS_PTY,  amfmStation.pty()));
      ALOGV(" FM RDS_PTY     '%d' ", amfmStation.pty());
      }                                                                
    }
    selectedProgramInfo.infoFlags |= ProgramInfoFlags::TUNED;
    selectedProgramInfo.infoFlags |= ProgramInfoFlags::LIVE;

    if( currentSession != nullptr ) {
        (currentSession->mCallback)->onCurrentProgramInfoChanged(selectedProgramInfo);
        currentSession->mSelectedProgramInfo = selectedProgramInfo;
    
#ifdef CFG_BCRADIO_PASA_ADDITIONS
        if(amfmStation.hd() == true)
        {
            /* update hd multicast information */
            currentSession->updateMulticastInfo(&station);
        }
        else
        {
            // isHdAvailable = 1 and isDigitalAvailable = 0 Show HD Icon and buffering..
            // isHdAvailable = 1 and isDigitalAvailable = 1 Show HD Icon and Playing

            (currentSession->mCallback)->onParametersUpdated(hidl_vec<VendorKeyValue>({
//                      {ISHDAVAILABLE,std::to_string(pStation->uStationData.amfmStationData.hd)},
//                      {ISDIGITALAUDIO,std::to_string(ui8DigitalAudio)}
                      {ISHDAVAILABLE, "false"},
                      {ISDIGITALAUDIO, "0"}
            }));
        }
      
        
#endif    
    }


    
  }
  break;
  case TUNSDK_BAND_DAB:
  {
    auto dabStation = station.station().dab();
    auto dabTunable = dabStation.dabTunable();
    auto label = dabStation.stationLabel();
    //auto channel = dabStation.channelLabel();
    auto ensembleLabel = dabStation.ensembleLabel();
    auto dabMeta = meta.metaData().dab();
    auto dlPlusData = dabMeta.dlPlusData();
    
    selectedProgramInfo.selector = make_selector_identifierType_dab(dabTunable.sid(),dabTunable.eid(),dabTunable.ecc(),dabTunable.scids(),dabTunable.freqKHz());
    selectedProgramInfo.metadata = hidl_vec<Metadata>({});
    
    if ((std::string((char*)&label.label())).length() > 0){
    hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::DAB_SERVICE_NAME, std::string((char*)&label.label())));
    ALOGV(" DAB DAB_SERVICE_NAME     '%s' ", label.label());
    }                                                                 
    if ((std::string((char*)&label.shortLabel())).length() > 0){
    hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::DAB_SERVICE_NAME_SHORT, std::string((char*)&label.shortLabel())));
    ALOGV(" DAB DAB_SERVICE_NAME_SHORT     '%s' ", label.shortLabel());
    }
    if ((std::string((char*)&ensembleLabel.label())).length() > 0){
    hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::DAB_ENSEMBLE_NAME, std::string((char*)&ensembleLabel.label())));
    ALOGV(" DAB DAB_ENSEMBLE_NAME     '%s' ", ensembleLabel.label());
    } 
    if ((std::string((char*)&ensembleLabel.shortLabel())).length() > 0){
    hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::DAB_ENSEMBLE_NAME_SHORT, std::string((char*)&ensembleLabel.shortLabel())));
    ALOGV(" DAB DAB_ENSEMBLE_NAME_SHORT     '%s' ", ensembleLabel.shortLabel());
    } 
    if ((std::string((char*)&dlPlusData.title())).length() > 0){
    hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::SONG_TITLE, std::string((char*)&dlPlusData.title())));
    ALOGV(" DAB SONG_TITLE     '%s' ", dlPlusData.title());
    }
    if ((std::string((char*)&dlPlusData.artist())).length() > 0){
    hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::SONG_ARTIST, std::string((char*)&dlPlusData.artist())));
    ALOGV(" DAB SONG_ARTIST     '%s' ", dlPlusData.artist());
    } 
    if ((std::string((char*)&dabMeta.dls())).length() > 0){
    hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::RDS_RT, std::string((char*)&dabMeta.dls())));
    ALOGV(" DAB RDS_RT     '%s' ", dabMeta.dls());
    }
//    if ( dabMeta.albumArtId() != 0){
//    hidl_vec_push_back(&(selectedProgramInfo.metadata), make_metadata(MetadataKey::ALBUM_ART, dabMeta.albumArtId()));
//    ALOGV(" DAB ALBUM_ART     '%d' IGNORED", dabMeta.albumArtId());
//    } 


    if( currentSession->mDabAudioStatus == TUNSDK_DAB_AUDIO_DABFM_LINK  && currentSession->mIsFmQualityValid == true)
    {
        selectedProgramInfo.signalQuality = currentSession->mFmQuality;
    }
    else  if( currentSession->mIsDabQualityValid == true )
    {
        selectedProgramInfo.signalQuality = currentSession->mDabQuality;
    }
    else
    {
        selectedProgramInfo.signalQuality = 99;    // TODO check how to get initial value from tunsdk
    }
    
    selectedProgramInfo.infoFlags |= ProgramInfoFlags::TUNED;
    selectedProgramInfo.infoFlags |= ProgramInfoFlags::LIVE;

    if( currentSession != nullptr ) {
        (currentSession->mCallback)->onCurrentProgramInfoChanged(selectedProgramInfo);
        currentSession->mSelectedProgramInfo = selectedProgramInfo;
        LOG(DEBUG) << "  DAB station " << toString(currentSession->mSelectedProgramInfo);
        
    }
  }
  break;
  default:
     ALOGV("On_SelectedStation: band %d not supported in HMI\n", station.band());
    break;
  }
  
}


void CTUNSDK_BroadcastRadio::TUNSDK_On_ConfiguredSettings_Notification(Ts_TUNSDK_On_ConfiguredSettings_Notification_Handle outData)
{
   auto s = outData.settings();
   
   ALOGV("TUNSDK_On_ConfiguredSettings_Notification\n");
   ALOGV("  fm rds: %s af: %s reg: %s\n", boolOO(s.fmRdsSwitch()), boolOO(s.fmAfSwitch()), Te_tunsdk_setting_fm_reg_toString(s.fmRegional()));
   ALOGV("  dab af: %s dabOther: %s dabSoft: %s\n", Te_tunsdk_setting_dab_af_toString(s.dabAfSwitch()), boolOO(s.dabOtherAnnouncements()), boolOO(s.dabSoftlinks()));
   ALOGV("  tp: %s\n", Te_tunsdk_setting_tp_toString(s.mstrTp()));
   ALOGV("  hd am: %s fm : %s\n", boolOO(s.amHd()), boolOO(s.fmHd()));
  
 
  if(s.fmHd() )
  {
    // ConfigFlag::FORCE_DIGITAL
    gFlag[2] =1;
    gFlag[1] = 0;
  }
  else
  {
    // ConfigFlag::FORCE_ANALOG
    gFlag[1] = 1;
    gFlag[2] = 0;
  }
  //ConfigFlag::RDS_AF
  gFlag[3] = s.fmRdsSwitch();
  
   // ConfigFlag::RDS_REG
   if(TUNSDK_SETTING_FM_REG_OFF== s.fmRegional())
   {
    gFlag[4] =0; 
   }
   else if((TUNSDK_SETTING_FM_REG_ON == s.fmRegional())||( TUNSDK_SETTING_FM_REG_ON == s.fmRegional()))
   {
    gFlag[4] =1;
   }
 
   // ConfigFlag::DAB_DAB_LINKING
    if( s.dabAfSwitch() == TUNSDK_SETTING_DAB_AF_OFF)
    {
      gFlag[5] =0;
    }
    else if( s.dabAfSwitch() == TUNSDK_SETTING_DAB_AF_DAB_ONLY)
    {
       gFlag[5] =1;
    }
    else
    {
      
    }
   
    //ConfigFlag::DAB_FM_LINKING
   if( s.dabAfSwitch() == TUNSDK_SETTING_DAB_AF_FM_ONLY)
    {
      gFlag[6] =1;
    }
    else 
    {
       gFlag[6] =0;
    }
  
    //ConfigFlag::DAB_FM_SOFT_LINKING
   if( s.dabAfSwitch() == TUNSDK_SETTING_DAB_AF_FM_AND_DAB)
    {
      gFlag[7] =1;
    }
    else 
    {
       gFlag[7] =0;
    } 
   
    //ConfigFlag::DAB_DAB_SOFT_LINKING
    gFlag[8] =s.dabSoftlinks();

    gFlag[9] =s.mstrTp();
    
    gFlag[10] =s.epg();
    gFlag[11] =s.slideshow();
}


void CTUNSDK_BroadcastRadio::TUNSDK_On_TPStatus_Notification(Ts_TUNSDK_On_TPStatus_Notification_Handle outData)
{
   ALOGV("TUNSDK_On_TPStatus_Notification status: %s\n", Te_tunsdk_tpStatus_toString(outData.tpStatusData()));
}


void CTUNSDK_BroadcastRadio::TUNSDK_On_Announcement_Start_Notification(Ts_TUNSDK_On_Announcement_Start_Notification_Handle outData)
{
  Announcement announcement = {};
  ProgramSelector selector = {};
  hidl_vec<VendorKeyValue> vendorInfo = hidl_vec<VendorKeyValue>({
                      {"com.announcement.start", "true"},
                      {"com.announcement.stop", "false"}
            });

  ALOGV("TUNSDK_On_Announcement_Start type: %s\n", Te_tunsdk_announcement_type_toString(outData.annoType()));
   ALOGV("==========================================================================\n");
  Ts_tunsdk_station_Handle station = outData.station();
  switch (station.band())
  {
  case TUNSDK_BAND_FM:
  {
    Ts_tunsdk_amfmhd_station_Handle amfmStation = station.station().amfm();
    auto amfmTunable = amfmStation.amfmhdTunable();
     if (ann_count % 2 == 0) {
	announcement.vendorInfo = hidl_vec<VendorKeyValue>({
                      {"com.announcement.start", "true"},
                      {"com.announcement.stop", "false"},
                      {"com.announcement.station", "Radio Mirchi"}
            });
     } else {
	announcement.vendorInfo = hidl_vec<VendorKeyValue>({
                      {"com.announcement.start", "false"},
                      {"com.announcement.stop", "true"},
                      {"com.announcement.station", "Radio Mirchi"}
            });
     }
     ann_count = ann_count + 1;
     ALOGV("Anno from FM freqKHz: %d pi: %x ps: %s pty: %d\n", amfmTunable.freqKHz() + (100*ann_count), amfmTunable.pi(), amfmStation.ps(), amfmStation.pty());
     selector = make_selector_identifierType_fm(amfmTunable.freqKHz() + (100*ann_count), amfmTunable.pi());
     
	if(TUNSDK_ANNO_TYPE_FM_TA ==outData.annoType())
	{
		announcement.type = AnnouncementType::TRAFFIC;
	} 
	else if(TUNSDK_ANNO_TYPE_DAB_NEWS ==outData.annoType())
	{
		announcement.type = AnnouncementType::NEWS;
	}
	else
	{
		announcement.type = AnnouncementType::MISC;
	}
	announcement.selector = selector;
  }
  break;
  case TUNSDK_BAND_DAB:
  { /*this is currently not yet sent from hmi_if and I cannot verify it on my target, for some reason no DAB*/

    auto dabStation = station.station().dab();
    auto dabTunable = dabStation.dabTunable();
    auto label = dabStation.stationLabel();
    auto channel = dabStation.channelLabel();
    auto ensembleLabel = dabStation.ensembleLabel();
     ALOGV("Anno from DAB: channel %s ensemble: %s label: %s", channel, ensembleLabel.label(), label.label());
     ALOGV(" eid: %x sid: %x scids: %x\n", dabTunable.eid(), dabTunable.sid(), dabTunable.scids());

    selector = make_selector_identifierType_dab(dabTunable.sid(),dabTunable.eid(),dabTunable.ecc(),dabTunable.scids(),dabTunable.freqKHz());

	if(TUNSDK_ANNO_TYPE_DAB_TA ==outData.annoType())
	{
		announcement.type = AnnouncementType::TRAFFIC;
	} 
	else if(TUNSDK_ANNO_TYPE_DAB_WARNING ==outData.annoType())
	{
		announcement.type = AnnouncementType::WARNING;
	}
	else if(TUNSDK_ANNO_TYPE_DAB_NEWS ==outData.annoType())
	{
		announcement.type = AnnouncementType::NEWS;
	}
	else if(TUNSDK_ANNO_TYPE_DAB_EVENT ==outData.annoType())
	{
		announcement.type = AnnouncementType::EVENT;
	}
	else if(TUNSDK_ANNO_TYPE_DAB_SPECIAL_EVENT ==outData.annoType())
	{
		announcement.type = AnnouncementType::EVENT;
	}
	else if(TUNSDK_ANNO_TYPE_DAB_SPORT==outData.annoType())
	{
		announcement.type = AnnouncementType::SPORT;
	}
	else if(TUNSDK_ANNO_TYPE_DAB_WEATHER==outData.annoType())
	{
		announcement.type = AnnouncementType::WEATHER;
	}
	else if(TUNSDK_ANNO_TYPE_DAB_ALARM==outData.annoType())
	{
		announcement.type = AnnouncementType::EMERGENCY ;
	}
	else
	{
		announcement.type = AnnouncementType::MISC;
	}
    announcement.selector = selector;


  }
  break;
  default:
     ALOGV("TUNSDK_On_Announcement_Start: band %d not supported yet in HMI\n", station.band());
    break;
  }
   ALOGV("==========================================================================\n");

   pgObjAnnouncements->notifyAnnouncement(announcement);

}


void CTUNSDK_BroadcastRadio::TUNSDK_On_Announcement_End_Notification(Ts_TUNSDK_On_Announcement_End_Notification_Handle outData)
{
   ALOGV("==========================================================================\n");
   ALOGV("TUNSDK_On_Announcement_End %s\n", Te_tunsdk_announcement_type_toString(outData.annoType()));
   ALOGV("==========================================================================\n");
   
   /*todo, need to check how to remove the annoucements*/
}

/*
void CTUNSDK_BroadcastRadio::TUNSDK_On_Announcement_Info_Notification(Ts_TUNSDK_On_Announcement_Info_Notification_Handle outData)
{
   ALOGV("==========================================================================\n");
   ALOGV("TUNSDK_On_Announcement_Info %s\n", Te_tunsdk_band_toString(outData.station().band()));
   ALOGV("==========================================================================\n");
}
*/

void CTUNSDK_BroadcastRadio::TUNSDK_On_Announcement_Background_Notification(Ts_TUNSDK_On_Announcement_Background_Notification_Handle outData)
{
  Announcement announcement = {};
  ProgramSelector selector = {};
  auto station = outData.station();
   ALOGV("TUNSDK_On_Announcement_Background_Notification: type %s band %s\n", Te_tunsdk_announcement_type_toString(outData.annoType()), Te_tunsdk_band_toString(station.band()));
  switch (station.band())
  {
  case TUNSDK_BAND_FM:
  {
    Ts_tunsdk_amfmhd_station_Handle amfmStation = station.station().amfm();
    auto amfmTunable = amfmStation.amfmhdTunable();
     ALOGV("BGR anno from FM freqKHz: %d pi: %x ps: %s pty: %d\n", amfmTunable.freqKHz(), amfmTunable.pi(), amfmStation.ps(), amfmStation.pty());
	 selector = make_selector_identifierType_fm(amfmTunable.freqKHz(), amfmTunable.pi());
     if(TUNSDK_ANNO_TYPE_FM_TA ==outData.annoType())
	{
		announcement.type = AnnouncementType::TRAFFIC;
	} 
	else
	{
		announcement.type = AnnouncementType::MISC;
	}
	announcement.selector = selector;
  }
  break;
  case TUNSDK_BAND_DAB:
  {
    auto dabStation = station.station().dab();
    auto dabTunable = dabStation.dabTunable();
    auto label = dabStation.stationLabel();
    auto channel = dabStation.channelLabel();
    auto ensembleLabel = dabStation.ensembleLabel();
     ALOGV("BGR anno from DAB: channel %s ensemble: %s label: %s", channel, ensembleLabel.label(), label.label());
     ALOGV(" eid: %x sid: %x scids: %x\n", dabTunable.eid(), dabTunable.sid(), dabTunable.scids());
	  selector = make_selector_identifierType_dab(dabTunable.sid(),dabTunable.eid(),dabTunable.ecc(),dabTunable.scids(),dabTunable.freqKHz());

	if(TUNSDK_ANNO_TYPE_DAB_TA ==outData.annoType())
	{
		announcement.type = AnnouncementType::TRAFFIC;
	} 
	else if(TUNSDK_ANNO_TYPE_DAB_WARNING ==outData.annoType())
	{
		announcement.type = AnnouncementType::WARNING;
	}
	else if(TUNSDK_ANNO_TYPE_DAB_NEWS ==outData.annoType())
	{
		announcement.type = AnnouncementType::NEWS;
	}
	else if(TUNSDK_ANNO_TYPE_DAB_EVENT ==outData.annoType())
	{
		announcement.type = AnnouncementType::EVENT;
	}
	else if(TUNSDK_ANNO_TYPE_DAB_SPECIAL_EVENT ==outData.annoType())
	{
		announcement.type = AnnouncementType::EVENT;
	}
	else if(TUNSDK_ANNO_TYPE_DAB_SPORT==outData.annoType())
	{
		announcement.type = AnnouncementType::SPORT;
	}
	else if(TUNSDK_ANNO_TYPE_DAB_WEATHER==outData.annoType())
	{
		announcement.type = AnnouncementType::WEATHER;
	}
	else if(TUNSDK_ANNO_TYPE_DAB_ALARM==outData.annoType())
	{
		announcement.type = AnnouncementType::EMERGENCY ;
	}
	else
	{
		announcement.type = AnnouncementType::MISC;
	}
    announcement.selector = selector;
  }
  break;
  default:
     ALOGV("On_Announcement_Background: band %d not supported in HMI\n", station.band());
    break;
  }
   pgObjAnnouncements->notifyAnnouncement(announcement);
}


void CTUNSDK_BroadcastRadio::TUNSDK_On_HdAudioStatus_Notification(Ts_TUNSDK_On_HdAudioStatus_Notification_Handle outData)
{
    ALOGV("HMI: HD Audio status: %s\n", Te_tunsdk_hd_audio_status_toString(outData.hdAudioStatus()));
}


void CTUNSDK_BroadcastRadio::TUNSDK_On_DabAudioStatus_Notification(Ts_TUNSDK_On_DabAudioStatus_Notification_Handle outData)
{
   ALOGV("HMI:TUNSDK_On_DabAudioStatus_Notification: %s\n", Te_tunsdk_dab_audio_status_toString(outData.dabAudioStatus()));
   

#ifdef CFG_BCRADIO_PASE_VENDOR_KEYS

    sp<TunerSession> currentSession=nullptr;
    {
      lock_guard<mutex> lk(g_session_mutex);
      currentSession = g_tunerSession.promote();
    }

    if( currentSession != nullptr ) {

       Te_tunsdk_dab_audio_status dabAudioStatus = outData.dabAudioStatus();
       
       currentSession->mDabAudioStatus = dabAudioStatus;
    
       auto vendorKeys = hidl_vec<VendorKeyValue>( { {PASE_KEY_DAB_AUDIO_STATUS, std::to_string(dabAudioStatus)} } );

       LOG(DEBUG) << "  onParametersUpdated: " << toString(vendorKeys);

       (currentSession->mCallback)->onParametersUpdated(vendorKeys);
    }
#endif       
  
   
}

void CTUNSDK_BroadcastRadio::TUNSDK_On_AmFmSetTmcService_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_AmFmSetTmcService_Response_Handle outData)
{
  (void)transId;
  (void)outData;
   ALOGV("HMI: TUNSDK_On_AmFmSetTmcService_Response %d\n",  result);
}

void CTUNSDK_BroadcastRadio::TUNSDK_On_AmFmTmcChannel_Notification(Ts_TUNSDK_On_AmFmTmcChannel_Notification_Handle outData)
{
    Ts_tunsdk_amfmTmcChannel_Handle  channel = outData.channel();
    ALOGV("HMI:TUNSDK_On_AmFmTmcChannel_Notification for pi 0x%x freqKHz %d\n", outData.pi(), outData.freqKHz());
    ALOGV("HMI:TUNSDK_On_AmFmTmcChannel_Notification for cc 0x%x ltn %d sid %d ecc 0x%x ps %s spn %s\n", channel.cc(), channel.ltn(), channel.sid(), channel.ecc(), channel.ps(), channel.spn());
}

void CTUNSDK_BroadcastRadio::TUNSDK_On_AmFmTmcMessage_Notification(Ts_TUNSDK_On_AmFmTmcMessage_Notification_Handle outData)
{
    ALOGV("HMI:TUNSDK_On_AmFmTmcMessage_Notification freqKHz %d ltn %d sid %d Data[a,b,c,d] : " "0x%x,0x%x,0x%x,0x%x" "\n", outData.freqKHz(), outData.ltn(), outData.sid(),
            outData.block_a(), outData.block_b(), outData.block_c(), outData.block_d());
}

void CTUNSDK_BroadcastRadio::TUNSDK_On_AmFmSetTmcScan_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_AmFmSetTmcScan_Response_Handle outData)
{
   (void)transId;
   (void)outData;
    ALOGV("HMI:TUNSDK_On_AmFmSetTmcScan_Response %d\n",  result);
}



void CTUNSDK_BroadcastRadio::TUNSDK_On_PresetStore_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_PresetStore_Response_Handle outData)
{
    ALOGV("HMI:TUNSDK_On_PresetStore_Response %d\n", result);
}

void CTUNSDK_BroadcastRadio::TUNSDK_On_PresetList_Notification(Ts_TUNSDK_On_PresetList_Notification_Handle outData)
{
    auto presetList = outData.presetList();

   ALOGV("HMI:Preset list presetListLength %d\n", presetList.presets_Length());

    Ts_tunsdk_preset_Handle preset(0);
    for (uint32_t i= 0U; i<presetList.presets_Length(); i++)
    {
        bool presetValid = presetList.presets_ElementAt(i, preset);
        if (presetValid)
        {
            if (preset.band() != TUNSDK_BAND_INVALID)
            {
                printf("%d: band %s ",i,Te_tunsdk_band_toString(preset.band())); /*no newline*/
                auto band = preset.band();
                switch (band)
                {
                    case TUNSDK_BAND_DAB:
                    {
                        auto dabPreset = preset.presetData().dab();
                        auto label  = dabPreset.serviceLabel();
                        auto ensembleLabel = dabPreset.ensembleLabel();
                        auto dabTunable = dabPreset.dabTunable();
                        auto channel = dabPreset.channelLabel();
                       // printf("channel: " COLORMAGENTA "%s " COLORNORMAL "ensemble: " COLORYELLOW "%s" COLORNORMAL " label: " COLORYELLOW  "%s" COLORNORMAL, channel, ensembleLabel.label(), label.label());
                        //printf(" eid:" COLORCYAN " 0x%x " COLORNORMAL "sid: " COLORCYAN "0x%x " COLORNORMAL "scids: 0x%x\n", dabTunable.eid(), dabTunable.sid(), dabTunable.scids());
                    }
                    break;

                    case TUNSDK_BAND_FM:
                    case TUNSDK_BAND_AM_MW:
                    {
                        auto fmPreset = preset.presetData().amfm();
                        auto amfmTunable = fmPreset.amfmhdTunable();
                      //  printf("freq: " COLORMAGENTA "%d" COLORNORMAL " pi: " COLORCYAN "%x" COLORNORMAL " ps: " COLORYELLOW  "%s" COLORNORMAL " pty: %d\n", amfmTunable.freq(), amfmTunable.pi(), fmPreset.ps(), fmPreset.pty());
                    }
                    break;

                    default:
                        ALOGV("HMI:No good preset data %d\n", band);
                        break;
                }
            }
        }
    }
    ALOGV("HMI:Preset list end\n");

}

void CTUNSDK_BroadcastRadio::TUNSDK_On_PresetGetList_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_PresetGetList_Response_Handle outData)
{
   (void)transId;
   (void)outData;
    ALOGV("HMI:TUNSDK_On_PresetGetList_Response %d\n", result);
}

void CTUNSDK_BroadcastRadio::TUNSDK_On_PresetRecall_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_PresetRecall_Response_Handle outData)
{
   (void)transId;
   (void)outData;
    ALOGV("HMI:TUNSDK_On_PresetRecall_Response %d\n", result);
}

void CTUNSDK_BroadcastRadio::TUNSDK_On_RequireModeChange_Notification(Ts_TUNSDK_On_RequireModeChange_Notification_Handle outData)
{
   auto mode = outData.requiredMode();
     ALOGV("HMI:TUNSDK_On_RequireModeChange_Notification %d\n", mode);
}

void CTUNSDK_BroadcastRadio::TUNSDK_On_PresetClear_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_PresetClear_Response_Handle outData)
{
    printf("HMI: TUNSDK_On_PresetClear_Response  %d\n", result);
}


void CTUNSDK_BroadcastRadio::TUNSDK_On_Slideshow_Notification(Ts_TUNSDK_On_Slideshow_Notification_Handle outData)
{
    Ts_tunsdk_slide_Handle slide = outData.slide();
    ALOGV("HMI: TUNSDK_On_Slideshow_Notification name %s\n",slide.contentName());
}

void CTUNSDK_BroadcastRadio::TUNSDK_On_PresetMove_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_PresetMove_Response_Handle outData)
{
    ALOGV("HMI: TUNSDK_On_PresetMove_Response %d\n", result);
}

void CTUNSDK_BroadcastRadio::TUNSDK_On_UpdateFirmware_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_UpdateFirmware_Response_Handle outData)
{
    ALOGV("HMI: TUNSDK_On_UpdateFirmware_Response %d\n", result);
}

void CTUNSDK_BroadcastRadio::TUNSDK_On_SetHdTpeg_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_SetHdTpeg_Response_Handle outData)
{
    ALOGV("HMI: TUNSDK_On_SetHdTpeg_Response %d\n", result);
}

void CTUNSDK_BroadcastRadio::TUNSDK_On_HdTpegChannel_Notification(Ts_TUNSDK_On_HdTpegChannel_Notification_Handle outData)
{
    ALOGV("HMI: TUNSDK_On_HdTpegChannel_Notification\n");
}

void CTUNSDK_BroadcastRadio::TUNSDK_On_HdTpegData_Notification(Ts_TUNSDK_On_HdTpegData_Notification_Handle outData)
{
    ALOGV("HMI: TUNSDK_On_HdTpegData_Notification\n");
}

#if 0
void CTUNSDK_BroadcastRadio::TUNSDK_On_ETMSetTestmode_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_ETMSetTestmode_Response_Handle outData)
{
    ALOGV("HMI: TUNSDK_On_ETMSetTestmode_Response %d\n", result);
}

void CTUNSDK_BroadcastRadio::TUNSDK_On_ETMAMFMLiveData_Notification(Ts_TUNSDK_On_ETMAMFMLiveData_Notification_Handle outData)
{
    ALOGV("TUNSDK_On_ETMAMFMLiveData_Notification\n");
}

void CTUNSDK_BroadcastRadio::TUNSDK_On_ETMAMFMSetSetting_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_ETMAMFMSetSetting_Response_Handle outData)
{
    ALOGV("HMI: TUNSDK_On_ETMAMFMSetSetting_Response %s %d\n", result);
}

void CTUNSDK_BroadcastRadio::TUNSDK_On_ETMAMFMSetting_Notification(Ts_TUNSDK_On_ETMAMFMSetting_Notification_Handle outData)
{
    ALOGV("TUNSDK_On_ETMAMFMSetting_Notification\n");
    Ts_tunsdk_etm_amfm_analog_settings_Handle analog = outData.settings().analogSetting();
    Ts_tunsdk_etm_amfm_iboc_settings_Handle iboc = outData.settings().ibocSetting();

    (void)iboc;
    ALOGV("pd: %d, bs: %d\n", analog.pdSetting(), analog.bgrBandScanSetting());
}

void CTUNSDK_BroadcastRadio::TUNSDK_On_ETMDABLiveData_Notification(Ts_TUNSDK_On_ETMDABLiveData_Notification_Handle outData)
{
    ALOGV("TUNSDK_On_ETMDABLiveData_Notification %s\n", outData.dabLiveData().listeningData().serviceLabel());
    Ts_tunsdk_etm_dab_tunerData_Handle handle(0);
    for (int32_t i = 0; i< outData.dabLiveData().tunerData_Length(); i++)
    {
        if (outData.dabLiveData().tunerData_ElementAt(i, handle))
        {
            //handele the data
        }
    }
}
#endif


void CTUNSDK_BroadcastRadio::TUNSDK_On_AmFmReceptionQuality_Notification(Ts_TUNSDK_On_AmFmReceptionQuality_Notification_Handle outData)
{
    static const uint8_t maxQualities = 10U;
    static uint8_t qualities [maxQualities] = {0U};
    static uint8_t nrQualities = 0U;

    uint8_t quality = outData.quality();    
    
    
    
    sp<TunerSession> currentSession = nullptr;
    {
        lock_guard<mutex> lk(g_session_mutex);
        currentSession = g_tunerSession.promote();
    }

    if (currentSession != nullptr)
    {
        currentSession->mIsFmQualityValid = true;
        currentSession->mFmQuality = quality;

        if (currentSession->mMode == TUNSDK_MODE_FM)
        {      
            currentSession->mIsDabQualityValid = false;
            
            qualities [nrQualities] = quality;
            nrQualities++;
            nrQualities = nrQualities % maxQualities;
            
            if (nrQualities == 0) {
                ALOGV("TUNSDK_On_AmFmReceptionQuality_Notification * 10 %d %d %d %d %d %d %d %d %d %d\n",
                    qualities[0], qualities[1], qualities[2], qualities[3], qualities[4], qualities[5], qualities[6], qualities[7], qualities[8], qualities[9]);
    
                if( currentSession->mSelectedProgramInfo.signalQuality != qualities[0] )
                {
                    currentSession->mSelectedProgramInfo.signalQuality = qualities[0]; //TODO average?
                    ALOGV("  Callback onCurrentProgramInfoChanged() - FM Updating signal quality %d", currentSession->mSelectedProgramInfo.signalQuality );
    //                    LOG(DEBUG) << "  FM station " << toString(currentSession->mSelectedProgramInfo);
                    (currentSession->mCallback)->onCurrentProgramInfoChanged(currentSession->mSelectedProgramInfo);
                }
            }
        } 
        else if (currentSession->mMode == TUNSDK_MODE_DAB)
        {
           
            //ALOGV("TUNSDK_On_AmFmReceptionQuality_Notification in DAB mode  %d\n", quality );
        
            if( currentSession->mDabAudioStatus == TUNSDK_DAB_AUDIO_DABFM_LINK)
            {
                currentSession->mSelectedProgramInfo.signalQuality = quality;
                ALOGV("  Callback onCurrentProgramInfoChanged() - DAB/FM link - FM Updating signal quality %d", currentSession->mSelectedProgramInfo.signalQuality );
//                    LOG(DEBUG) << "  FM station " << toString(currentSession->mSelectedProgramInfo);
                (currentSession->mCallback)->onCurrentProgramInfoChanged(currentSession->mSelectedProgramInfo);
            }            
        }  
        else
        {      
            ALOGE("  TUNSDK_On_AmFmReceptionQuality_Notification unexpected in mode %d ", currentSession->mMode);
        }
           
    }
    
}

void CTUNSDK_BroadcastRadio::TUNSDK_On_DabAudioQuality_Notification(Ts_TUNSDK_On_DabAudioQuality_Notification_Handle outData)
{
    uint8_t quality = outData.quality();
    //ALOGV("TUNSDK_On_DabAudioQuality_Notification %d", quality);

    sp<TunerSession> currentSession = nullptr;
    {
        lock_guard<mutex> lk(g_session_mutex);
        currentSession = g_tunerSession.promote();
    }

    if (currentSession != nullptr)
    {
        if (currentSession->mMode == TUNSDK_MODE_DAB)
        {      
            currentSession->mIsDabQualityValid = true;
            currentSession->mDabQuality = quality;
    
            if( currentSession->mDabAudioStatus != TUNSDK_DAB_AUDIO_DABFM_LINK && currentSession->mSelectedProgramInfo.signalQuality != quality )
            {
                currentSession->mSelectedProgramInfo.signalQuality = quality;
                
                ALOGV("  Callback onCurrentProgramInfoChanged() - DAB Updating signal quality %d", currentSession->mSelectedProgramInfo.signalQuality );
//                LOG(DEBUG) << "  DAB station " << toString(currentSession->mSelectedProgramInfo);
                (currentSession->mCallback)->onCurrentProgramInfoChanged(currentSession->mSelectedProgramInfo);
            }
        }
        else
        {      
            ALOGE("  TUNSDK_On_DabAudioQuality_Notification unexpected in mode %d ", currentSession->mMode);
        }
           
    }
    
}


void CTUNSDK_BroadcastRadio::TUNSDK_On_GetImage_Response(tunsdk_transid_t transId, Te_tunsdk_result result, Ts_TUNSDK_On_GetImage_Response_Handle outData)
{
  ALOGV("HMI: TUNSDK_On_GetImage_Response for image 0x%x %d length %d\n", outData.imageIdOut(), result, outData.image().size());
  logo_length = 0;
  memset(&logo_buffer[0], 0, sizeof(logo_buffer));
  if (result == TUNSDK_RESULT_OK)
  {
    if (outData.image().size() > 0U) /*note this is guaranteed in case of TUNSDK_RESULT_OK, but i's a good habit, so let's check it anyway*/
    {
      memcpy(&logo_buffer[0], outData.image().buffer(), outData.image().size());
      //ALOGV("HMI: copy done   %d\n", outData.image().size());
      logo_length = outData.image().size();
    }
  }

  /*post semaphore*/
  sem_post(&image_sem);
  ALOGV("HMI: Semaphore post");

}

/*-----------------------------------------------------------------------------
    protected methods definitions
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
    private methods definitions
-----------------------------------------------------------------------------*/

/*===========================================================================*/
/*  CClass::CClass()                                                        */
/*===========================================================================*/

/******************************************************************************
    inner class section
******************************************************************************/

/*=============================================================================
    end of file
=============================================================================*/
