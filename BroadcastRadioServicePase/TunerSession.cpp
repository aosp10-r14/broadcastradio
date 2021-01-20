/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "BcRadio.pase.tunersession"
#define LOG_NDEBUG 0

#include <android-base/logging.h>
#include <broadcastradio-utils-2x/Utils.h>
#include <log/log.h>
 
#include "TunerSession.h"
#include "BroadcastRadio.h"
#include "TunerSdkClient.h"
#ifdef CFG_BCRADIO_PASA_ADDITIONS
  #include "PASAVendorKeyValue.h"
#endif
  #include "TunerParameters.h"
  
#define GET_BIT_VALUE(where, bit_number) (((where) & (1 << (bit_number))) >> bit_number)

int  gFlag[13];
namespace android {
namespace hardware {
namespace broadcastradio {
namespace V2_1 {
namespace implementation {

using namespace std::chrono_literals;

using utils::tunesTo;

using std::lock_guard;
using std::move;
using std::mutex;
using std::sort;
using std::vector;
using V2_0::IdentifierType;
using V2_0::ProgramListChunk;

// instantiate
CTUNSDK_BroadcastRadio* pClient =NULL;

//TunerSession* gTunerSession = NULL;






template <typename Type>
static uint32_t hidl_vec_push_back_result(hidl_vec<Type>* vec, const Type& value) {
    const uint32_t index = vec->size();
    vec->resize(index + 1);
    (*vec)[index] = value;
    return index;
}

TunerSession::TunerSession(BroadcastRadio& module, const sp<V2_0::ITunerCallback>& callback)
    : mCallback(callback), mModule(module), mCallback2_1(V2_1::ITunerCallback::castFrom(callback)) {
        
    ALOGV("%s", __func__);
        
#ifdef CFG_BCRADIO_COMBINED_STL_DAB_FM
    mCombinedStlDabFm = true;
#else    
    mCombinedStlDabFm = false;
#endif        
    
    memset(&gFlag[0],0,sizeof(gFlag));    
    mIsDabQualityValid = false;
    mDabQuality = 0;
        
    pClient = getClientInstance();
    
    mRadioState = TUNSDK_STATE_INVALID;
    mMode = TUNSDK_MODE_INVALID;
  
    Te_tunsdk_status status = TUNSDK_STATUS_INVALID;  
    status = pClient->TUNSDK_RegisterForObserver(TUNSDK_OBSERVER_ID_ALL, 1);  

    status = pClient->TUNSDK_GetStationList(0, TUNSDK_MODE_FM);
    status = pClient->TUNSDK_GetStationList(0, TUNSDK_MODE_DAB);

    
}

// makes ProgramInfo that points to no program
static ProgramInfo makeDummyProgramInfo(const ProgramSelector& selector) {
    ProgramInfo info = {};
    info.selector = selector;
    info.logicallyTunedTo = utils::make_identifier(
        IdentifierType::AMFM_FREQUENCY, utils::getId(selector, IdentifierType::AMFM_FREQUENCY));
    info.physicallyTunedTo = info.logicallyTunedTo;
    return info;
}

void TunerSession::tuneInternalLocked(const ProgramSelector& sel) {
    LOG(VERBOSE) << "tune (internal) to " << toString(sel);

   // VirtualProgram virtualProgram;
    //ProgramInfo programInfo;
    //if (virtualRadio().getProgram(sel, virtualProgram)) {
    //    mCurrentProgram = virtualProgram.selector;
    //    programInfo = virtualProgram;
    //} else {
    //    mCurrentProgram = sel;
    //    programInfo = makeDummyProgramInfo(sel);
    //}
    mIsTuneCompleted = true;

    //mCallback->onCurrentProgramInfoChanged(programInfo);
}

const BroadcastRadio& TunerSession::module() const {
    
//    ALOGV("%s", __func__);
    return mModule.get();
}

//const VirtualRadio& TunerSession::virtualRadio() const {
//    return module().mVirtualRadio;
//}

Return<Result> TunerSession::tune(const ProgramSelector& sel) {
    LOG(DEBUG) << "tune() - tune to " << toString(sel);

    Te_tunsdk_status status  = TUNSDK_STATUS_INVALID;
    Te_tunsdk_mode   modeNew = TUNSDK_MODE_INVALID;
    ProgramInfo programInfo;
    
    lock_guard<mutex> lk(mMut);
    
    if (mIsClosed) return Result::INVALID_STATE;

    if (!utils::isSupported(module().mProperties, sel)) {
        LOG(WARNING) << "tune() - selector not supported: " << toString(sel);
        return Result::NOT_SUPPORTED;
    }

    if (!utils::isValid(sel)) {
        LOG(ERROR) << "tune() - selector is not valid: " << toString(sel) << " - Let us try to continue for now";
//        return Result::INVALID_ARGUMENTS;
    }

     mCurrentProgram = sel;
     programInfo = makeDummyProgramInfo(sel);
     mCallback->onCurrentProgramInfoChanged(programInfo);
     
    if (utils::getType(sel.primaryId) == IdentifierType::DAB_FREQUENCY)
    {
        LOG(DEBUG) << "  tune to DAB_FREQUENCY " << toString(sel);
        
        mIsDabQualityValid = false;
        
        uint32_t freqKHz = sel.primaryId.value;        
        
        if( freqKHz < 174928 ) 
        {
            freqKHz = 174928;
            LOG(DEBUG) << "  tune to DAB_FREQUENCY - FIXING TO " << freqKHz;
        }
        
        modeNew = TUNSDK_MODE_DAB;
        if (mMode != modeNew){
            mMode = modeNew;
            status = pClient->TUNSDK_SetModeStationDab(0, 0, 0, 0, 0, freqKHz);
        }
        else{
            status = pClient->TUNSDK_SelectStationDab(0, 0, 0, 0, 0, freqKHz);
        }
    }
    else if (utils::getType(sel.primaryId) == IdentifierType::DAB_SID_EXT)
    {
        LOG(DEBUG) << "  tune to DAB_SID_EXT " << toString(sel);
        
        mIsDabQualityValid = false;
        
        modeNew = TUNSDK_MODE_DAB;
        uint32_t freqKHz = 0;
        uint16_t eid = 0;
        
        if (utils::hasId(sel,IdentifierType::DAB_FREQUENCY))
        {
            freqKHz = utils::getId(sel,IdentifierType::DAB_FREQUENCY, 0);
        }
        if (utils::hasId(sel,IdentifierType::DAB_ENSEMBLE))
        {
            eid = utils::getId(sel,IdentifierType::DAB_ENSEMBLE, 0);
        }
        
        uint32_t sidext = sel.primaryId.value;
      
        uint32_t sid     = (sidext)       & 0xffff;
        uint32_t ECC     = (sidext >> 16) & 0xff;
        uint32_t SCIdI   = (sidext >> 24) & 0xf;
        
        if (mMode != modeNew){
            status = pClient->TUNSDK_SetModeStationDab(0, sid, SCIdI, eid, ECC, freqKHz);
            mMode = modeNew;
        }
        else{
            status = pClient->TUNSDK_SelectStationDab(0, sid, SCIdI, eid, ECC, freqKHz);
        }
    }
    else if (utils::getType(sel.primaryId) == IdentifierType::HD_STATION_ID_EXT)
    {
        uint64_t hdidext = sel.primaryId.value;
        uint32_t stationId;

        stationId = hdidext & 0xffffffff;
        uint8_t mcast   = (hdidext >> HDEXT_SUBCHANNEL_BIT) & 0x0f;
        uint32_t freq    = (hdidext >> HDEXT_FREQ_BIT) & 0x3ffff;
        modeNew = TUNSDK_MODE_FM;
        if (mMode != modeNew)
        {
          if (freq < 1705)
          {
            status = pClient->TUNSDK_SetModeStationAmFm(0, TUNSDK_AMFM_BAND_AM_MW, freq, 0, mcast);
          }
          else
          {
            status = pClient->TUNSDK_SetModeStationAmFm(0, TUNSDK_AMFM_BAND_FM, freq, 0, mcast);
          }
          mMode = modeNew;
        }
        else
        {
          status = pClient->TUNSDK_SelectStationAmFm(0, TUNSDK_AMFM_BAND_FM, freq, 0, mcast);
        }
    }
    else if (utils::getType(sel.primaryId) == IdentifierType::RDS_PI)
    {
        modeNew = TUNSDK_MODE_FM;
        uint32_t pi       = sel.primaryId.value;
        uint32_t freq     = utils::getId(sel,IdentifierType::AMFM_FREQUENCY);
        uint8_t mcast    = 0;                  
        if (mMode != modeNew)
        {
          status = pClient->TUNSDK_SetModeStationAmFm(0, TUNSDK_AMFM_BAND_FM, freq, pi, mcast);
          mMode = modeNew;
        }
        else
        {
          status = pClient->TUNSDK_SelectStationAmFm(0, TUNSDK_AMFM_BAND_FM, freq, pi, mcast);
        }

    }
    else if (utils::getType(sel.primaryId) == IdentifierType::AMFM_FREQUENCY)
    {
      uint32_t pi;
      Te_tunsdk_amfm_band band;
        if (sel.primaryId.value < 1705)
        {
            modeNew = TUNSDK_MODE_AM_MW;
            band = TUNSDK_AMFM_BAND_AM_MW;
            pi    = 0xFFFFFFFF; 
        }
        else
        {
            band = TUNSDK_AMFM_BAND_FM;
            modeNew = TUNSDK_MODE_FM;
            pi    = 0xFFFFFFFF;                  
            modeNew = TUNSDK_MODE_FM;
        }
        uint32_t freq  = sel.primaryId.value;
        uint8_t mcast = 0;                            
        if (mMode != modeNew)
        {
          status = pClient->TUNSDK_SetModeStationAmFm(0, band, freq, pi, mcast);
          mMode = modeNew;
        }
        else
        {
          status = pClient->TUNSDK_SelectStationAmFm(0, band, freq, pi, mcast);
        }
    }
    else
    {
        LOG(WARNING) << "tune  unexpected identifier type " << toString(utils::getType(sel.primaryId));
        return Result::NOT_SUPPORTED;
    }
     
     
    LOG(DEBUG) << "  tune status " << status;
     
    return Result::OK;
}


/* 
  Scan 

  Note: Scan should actually do a seek operation 
*/
Return<Result> TunerSession::scan(bool directionUp, bool skipSubChannel) {
    LOG(DEBUG) << "seek directionUp=" << directionUp << " skipSubChannel=" << skipSubChannel;

    lock_guard<mutex> lk(mMut);
    if (mIsClosed) return Result::INVALID_STATE;

    Te_tunsdk_status status = TUNSDK_STATUS_INVALID;
    Te_tunsdk_direction direction = directionUp ? TUNSDK_DIRECTION_UP : TUNSDK_DIRECTION_DOWN;
    
    if (mMode== TUNSDK_MODE_DAB)
    {   
       Te_tunsdk_dab_seek_type param_seek_type = TUNSDK_DAB_SEEK_SERVICE_ALL;
       
        if (skipSubChannel)
        {
          param_seek_type = TUNSDK_DAB_SEEK_SERVICE_PRIMARY;
        }
        
        status = pClient->TUNSDK_SeekDab(0, direction, param_seek_type);
    }
    else
    {
        status = pClient->TUNSDK_Seek(0, direction);
    }

    return Result::OK;
}

Return<Result> TunerSession::step(bool directionUp) {
    LOG(DEBUG) << "step up=" << directionUp;
    lock_guard<mutex> lk(mMut);
    if (mIsClosed) return Result::INVALID_STATE;

    ALOGV("%s  directionUp %d ", __func__, directionUp);
    Te_tunsdk_status status;
    if(directionUp)
    {
        status = pClient->TUNSDK_TuneUpDown(0, TUNSDK_DIRECTION_UP, 1);
    }
    else
    {
        status = pClient->TUNSDK_TuneUpDown(0, TUNSDK_DIRECTION_DOWN, 1);
    }

    return Result::OK;
}

Return<void> TunerSession::cancel() {
    
    LOG(DEBUG) << "cancel";
    
    Te_tunsdk_status status = TUNSDK_STATUS_INVALID; 
    
    if( (mRadioState == TUNSDK_STATE_SEEK ) ||
        (mRadioState == TUNSDK_STATE_SEEK_NONSTOP ) ||
        (mRadioState == TUNSDK_STATE_ENSEMBLE_SEEK ) ||
        (mRadioState == TUNSDK_STATE_SERVCOMP_SEEK ) )
    {
        status = pClient->TUNSDK_SeekCancel(0);    
    } else {
        
        
    }
    

    return {};
}

Return<void> TunerSession::close2_1() {
    return {};
}
/*
Return<Result> TunerSession::startProgramListUpdates(const ProgramFilter& filter) {
    LOG(DEBUG) << "requested program list updates, filter=" << toString(filter);
    lock_guard<mutex> lk(mMut);
    if (mIsClosed) return Result::INVALID_STATE;

    auto list = virtualRadio().getProgramList();
    vector<VirtualProgram> filteredList;
    auto filterCb = [&filter](const VirtualProgram& program) {
        return utils::satisfies(filter, program.selector);
    };
    std::copy_if(list.begin(), list.end(), std::back_inserter(filteredList), filterCb);

    auto task = [this, list]() {
        lock_guard<mutex> lk(mMut);

        ProgramListChunk chunk = {};
        chunk.purge = true;
        chunk.complete = true;
        chunk.modified = hidl_vec<ProgramInfo>(list.begin(), list.end());

        mCallback->onProgramListUpdated(chunk);
    };
    mThread.schedule(task, delay::list);

    return Result::OK;
}
*/
Return<Result> TunerSession::startProgramListUpdates(const ProgramFilter& filter) {
//    LOG(DEBUG) << "requested program list updates, filter=" << toString(filter);
    ALOGV("%s -  mode %d  filter (%s)", __func__, mMode, toString(filter).c_str());
    Te_tunsdk_status status;
    lock_guard<mutex> lk(mMut);
    if (mIsClosed) return Result::INVALID_STATE;
    
    progListReqInfo.filter = filter;
    progListReqInfo.isProgramListRequested = true;  
    
//No manual update, only disabling/enabling of notifications    status = pClient->TUNSDK_UpdateStationList(0,(Te_tunsdk_mode)mMode);

    std::vector<ProgramInfo> filteredList;
    std::vector<ProgramInfo> list;

    list = mProgramListFM;
    list.insert( list.end(), mProgramListDAB.begin(), mProgramListDAB.end() );
    
    //TODO apply filter
    filteredList = list;
    
    ProgramListChunk chunk = {};
    chunk.purge = true;
    chunk.complete = true;
    chunk.modified = hidl_vec<ProgramInfo>(filteredList.begin(), filteredList.end());
    ALOGV("  Callback onProgramListUpdated() - Complete list, %d entries ", filteredList.size() );
    mCallback->onProgramListUpdated(chunk);
    
    return Result::OK;
}


Return<void> TunerSession::stopProgramListUpdates() {
//    LOG(DEBUG) << "requested program list updates to stop";
    ALOGV("%s", __func__);
    Te_tunsdk_status status;

//No manual update, only disabling/enabling of notifications    status = pClient->TUNSDK_UpdateStationListCancel(0);

    progListReqInfo.filter = {};
    progListReqInfo.isProgramListRequested = false;
    
    return {};
}

Return<void> TunerSession::isConfigFlagSet(ConfigFlag flag, isConfigFlagSet_cb _hidl_cb) {
    LOG(VERBOSE) << __func__ << " " << toString(flag);
     bool value;
    if (flag == ConfigFlag::FORCE_MONO){
         value = gFlag[0];
    }
    else if (flag == ConfigFlag::FORCE_ANALOG){
        value = gFlag[1];
    }
    else if (flag == ConfigFlag::FORCE_DIGITAL){
        value = gFlag[2];
    } 
    else if (flag == ConfigFlag::RDS_AF){
        value = gFlag[3];
    }
    else if (flag == ConfigFlag::RDS_REG){
        value = gFlag[4];
    }
    else if (flag == ConfigFlag::DAB_DAB_LINKING){
        value = gFlag[5];
    }
    else if (flag == ConfigFlag::DAB_FM_LINKING){
        value = gFlag[6];
    }
    else if (flag == ConfigFlag::DAB_FM_SOFT_LINKING){
        value = gFlag[7];
    }
    else if (flag == ConfigFlag::DAB_DAB_SOFT_LINKING){
        value = gFlag[8];
    }
    else{
        value = 0;
    }         
    _hidl_cb(Result::OK, value);
    return {};
}

Return<Result> TunerSession::setConfigFlag(ConfigFlag flag, bool value) {
    LOG(VERBOSE) << __func__ << " " << toString(flag) << " " << value;
     ALOGV("%s(%s, %d)", __func__, toString(flag).c_str(), value);
     Te_tunsdk_status status;
     Te_tunsdk_setting setting;
     uint32_t val;
     lock_guard<mutex> lk(mMut);
    if (mIsClosed) return Result::INVALID_STATE;
     
     val = (uint32_t)value;
     if (flag == ConfigFlag::FORCE_MONO){
        setting = TUNSDK_SETTING_FM_AF;
        gFlag[0]=value;
    }
    else if (flag == ConfigFlag::FORCE_ANALOG){
        setting = TUNSDK_SETTING_FM_HD;
         gFlag[1]=value;
         //val = TUNSDK_SETTING_OFF;
    }
    else if (flag == ConfigFlag::FORCE_DIGITAL){
        setting = TUNSDK_SETTING_FM_HD;
         gFlag[2]=value;
         //val = TUNSDK_SETTING_ON;
    } 
    if (flag == ConfigFlag::RDS_AF){
        setting = TUNSDK_SETTING_FM_AF;
        gFlag[3]=value;
    }
    else if (flag == ConfigFlag::RDS_REG){
        setting = TUNSDK_SETTING_FM_REG;
         gFlag[4]=value;
    }
    else if (flag == ConfigFlag::DAB_DAB_LINKING){
        setting = TUNSDK_SETTING_DAB_AF;
        gFlag[5]=value;
        if (val == 1){
            val = TUNSDK_SETTING_DAB_AF_DAB_ONLY;
        }
        else{
            val = TUNSDK_SETTING_DAB_AF_OFF;
        }
    }
    else if (flag == ConfigFlag::DAB_FM_LINKING){
        setting = TUNSDK_SETTING_DAB_AF;
        gFlag[6]=value;
        if (val == 1){
            val = TUNSDK_SETTING_DAB_AF_FM_ONLY;
        }
        else{
            val = TUNSDK_SETTING_DAB_AF_DAB_ONLY;
        }
    }
    else if (flag == ConfigFlag::DAB_FM_SOFT_LINKING){
        setting = TUNSDK_SETTING_DAB_AF;
         gFlag[7]=value;
        if (val == 1){
            val = TUNSDK_SETTING_DAB_AF_FM_AND_DAB;
        }
        else{
            val = TUNSDK_SETTING_DAB_AF_OFF;
        }
    }
    else if (flag == ConfigFlag::DAB_DAB_SOFT_LINKING){
        setting = TUNSDK_SETTING_DAB_AF_DAB_SOFTLINKS;
         gFlag[8]=value;
    }
    else{
        setting = TUNSDK_SETTING_INVALID;
    }     
     status = pClient->TUNSDK_ConfigureSetting(0, setting, val);
     
    return Result::OK;
}

Return<void> TunerSession::setParameters(const hidl_vec<VendorKeyValue>& parameters ,
                                         setParameters_cb _hidl_cb) {
     Te_tunsdk_status status;
     Te_tunsdk_setting setting;
     uint32_t val;
     hidl_vec<VendorKeyValue> resultInfo = hidl_vec<VendorKeyValue>({}); 
     for (auto& it : parameters) {
        std::string setting_func(it.key);
        std::string setting_value(it.value);
       ALOGV(" key   '%s' ", it.key.c_str());
       ALOGV(" key   '%s' ", it.value.c_str());
       
        /*TP*/
       if(setting_func.compare(SETTING_TP) == 0){
          setting = TUNSDK_SETTING_TP;
          /*get values*/
          if(setting_value.compare(TP_OFF) == 0){
            val = TUNSDK_SETTING_TP_OFF;
          }
          else if(setting_value.compare(TP_DAB_ONLY) == 0){
             val = TUNSDK_SETTING_TP_DAB_ONLY;
          }
          else if(setting_value.compare(TP_FM_ONLY) == 0){
             val = TUNSDK_SETTING_TP_FM_ONLY;
          }
          else if(setting_value.compare(TP_FM_AND_DAB) == 0){
             val = TUNSDK_SETTING_TP_FM_AND_DAB;
          }
          else
          {
             val = TUNSDK_SETTING_TP_INVALID;
          }
          if(val!=TUNSDK_SETTING_TP_INVALID){
            
             gFlag[9] = val;
             status = pClient->TUNSDK_ConfigureSetting(0, setting, val);
              hidl_vec_push_back_result(&resultInfo,{ SETTING_RESULT,std::to_string(status)});
          }
          else
          {
             hidl_vec_push_back_result(&resultInfo,{ SETTING_RESULT,std::to_string(TUNSDK_STATUS_INVALID)});
          }
       }
       /*EPG*/
       else  if(setting_func.compare(SETTING_EPG) == 0){
          /*get values*/
           setting = TUNSDK_SETTING_EPG;
           if(setting_value.compare(DAB_EPG_OFF) == 0){
            val = 0;/*TDB: need to use type define o means off*/
          }
          else if(setting_value.compare(DAB_EPG_ON) == 0){
             val = 1;/*TDB: need to use type define 1 means on*/
          }
          else
          {
             val = -1;
          }
          if(val!=-1){
            
            gFlag[10] = val;
             status = pClient->TUNSDK_ConfigureSetting(0, setting, val);
             hidl_vec_push_back_result(&resultInfo,{ SETTING_RESULT,std::to_string(status)});
          }
          else
          {
            hidl_vec_push_back_result(&resultInfo,{ SETTING_RESULT,std::to_string(TUNSDK_STATUS_INVALID)});
          }
       }
        /*Slideshow*/
       else if(setting_func.compare(SETTING_SLIDE_SHOW) == 0){
          /*get values*/
           setting = TUNSDK_SETTING_SLIDE_SHOW;
             if(setting_value.compare(DAB_SLS_OFF) == 0){
            val = 0;/*TDB: need to use type define o means off*/
          }
          else if(setting_value.compare(DAB_SLS_ON) == 0){
             val = 1;/*TDB: need to use type define 1 means on*/
          }
          else
          {
             val = -1;
          }
          if(val!=-1){
            
             gFlag[11] = val;
             status = pClient->TUNSDK_ConfigureSetting(0, setting, val);
             hidl_vec_push_back_result(&resultInfo,{ SETTING_RESULT,std::to_string(status)});
          }
          else
          {
             hidl_vec_push_back_result(&resultInfo,{ SETTING_RESULT,std::to_string(TUNSDK_STATUS_INVALID)});
          }
       }
       else
       {
          setting = TUNSDK_SETTING_INVALID;
       }

    }
     
    _hidl_cb(resultInfo);
    return {};
}

Return<void> TunerSession::getParameters(const hidl_vec<hidl_string>& parameters,
                                         getParameters_cb _hidl_cb) {
    Te_tunsdk_status status;
    Te_tunsdk_setting setting;
    int32_t val =0;   
    hidl_vec<VendorKeyValue> resultInfo = hidl_vec<VendorKeyValue>({}); 
    
     status = pClient->TUNSDK_GetConfiguredSettings(0);
    /*wait for the respose in the notifcation and then pack the requested values*/
    
    for (auto& it : parameters) {
     
        std::string setting_func(it);
  
      
       
         if(setting_func.compare(SETTING_TP) == 0){
            setting = TUNSDK_SETTING_TP;
            val = gFlag[9];
            hidl_vec_push_back_result(&resultInfo,{ it,std::to_string(val)});
         }
         else  if(setting_func.compare(SETTING_EPG) == 0){
            setting = TUNSDK_SETTING_EPG;
             val = gFlag[10];
            hidl_vec_push_back_result(&resultInfo,{ it,std::to_string(val)});
         }
         else  if(setting_func.compare(SETTING_SLIDE_SHOW) == 0){
            setting = TUNSDK_SETTING_SLIDE_SHOW;
             val = gFlag[11];
            hidl_vec_push_back_result(&resultInfo,{ it,std::to_string(val)});
         }
         else{
         }
         
         
    }
    _hidl_cb(resultInfo);
    return {};
}

Return<void> TunerSession::close() {
    LOG(DEBUG) << "closing session...";
    Te_tunsdk_status status;
    lock_guard<mutex> lk(mMut);
    if (mIsClosed) return {};

    status = pClient->TUNSDK_RegisterForObserver(TUNSDK_OBSERVER_ID_ALL, 0);  
  
    mIsClosed = true;

    status = pClient->TUNSDK_RegisterForObserver(TUNSDK_OBSERVER_ID_ALL, 0);  
    pClient =NULL;
    mThread.cancelAll();
    return {};
}

std::optional<AmFmBandRange> TunerSession::getAmFmRangeLocked() const {
    if (!mIsTuneCompleted) {
        LOG(WARNING) << "tune operation is in process";
        return {};
    }
    if (!utils::hasId(mCurrentProgram, IdentifierType::AMFM_FREQUENCY)) return {};

    auto freq = utils::getId(mCurrentProgram, IdentifierType::AMFM_FREQUENCY);
    for (auto&& range : module().getAmFmConfig().ranges) {
        if (range.lowerBound <= freq && range.upperBound >= freq) return range;
    }

    return {};
}










#ifdef CFG_BCRADIO_PASA_ADDITIONS
//PASA additions

void TunerSession::updateMulticastInfo(Ts_tunsdk_station_Handle* pStation)
{
    Ts_tunsdk_amfmhd_station_Handle amfmStation = pStation->station().amfm();
  
    uint8_t avaiProg = amfmStation.hdData().hdServicesOnFreqBits();
    char avaiProg_c[9] = {0};
    uint8_t ui8DigitalAudio = 0u;

    /* Digital Audio */
    if(amfmStation.hdData().signal_rcptn() == TUNSDK_HD_STATUS_PLAYING_DIGITAL_AUDIO)
    {
      ui8DigitalAudio = 1u;
    }
    
    for(uint8_t bit_count = 0; bit_count < (sizeof(uint8_t)<<3); ++bit_count)
    {
        if((GET_BIT_VALUE(avaiProg, bit_count)) == 0)
        {
            avaiProg_c[bit_count] = '0';
        }
        else
        {
            avaiProg_c[bit_count] = '1';
        }
    }

    avaiProg_c[8] = '\0';
    std::string str((char*)avaiProg_c);
    
    // isHdAvailable = 1 and isDigitalAvailable = 0 Show HD Icon and buffering..
    // isHdAvailable = 1 and isDigitalAvailable = 1 Show HD Icon and Playing
    mCallback-> onParametersUpdated(hidl_vec<VendorKeyValue>({
          {ISHDAVAILABLE,std::to_string(amfmStation.hd())},
          {ISDIGITALAUDIO,std::to_string(ui8DigitalAudio)}, 
          {HDSERVICEAVAILABLE_FREQUENCY,std::to_string(amfmStation.amfmhdTunable().freqKHz())},
          {HDSERVICEFLAGS,str}
      }));
        
}

#if 0
void TunerSession::updateMulticastInfo(Ts_graSrvIfTuner_station *pStation)
{
    uint8_t avaiProg = pStation->uStationData.amfmStationData.hdData.hdServicesOnFreqBits;
    char avaiProg_c[9] = {0};
    uint8_t ui8DigitalAudio = 0u;

    /* Digital Audio */
    if(pStation->uStationData.amfmStationData.hdData.signal_rcptn == GRA_SRV_IF_TUNER_AMFMHD_HD_STATUS_PLAYING_DIGITAL_AUDIO)
    {
      ui8DigitalAudio = 1u;
    }

    for(uint8_t bit_count = 0; bit_count < (sizeof(uint8_t)<<3); ++bit_count)
    {
  if((GET_BIT_VALUE(avaiProg, bit_count)) == 0)
  {
      avaiProg_c[bit_count] = '0';
  }
  else
  {
      avaiProg_c[bit_count] = '1';
  }
    }
    
    avaiProg_c[8] = '\0';
    std::string str((char*)avaiProg_c);
  
    // isHdAvailable = 1 and isDigitalAvailable = 0 Show HD Icon and buffering..
    // isHdAvailable = 1 and isDigitalAvailable = 1 Show HD Icon and Playing
    mCallback-> onParametersUpdated(hidl_vec<VendorKeyValue>({
          {ISHDAVAILABLE,std::to_string(pStation->uStationData.amfmStationData.hd)},
          {ISDIGITALAUDIO,std::to_string(ui8DigitalAudio)}, 
          {HDSERVICEAVAILABLE_FREQUENCY,std::to_string(pStation->uStationData.amfmStationData.amfmhdTunable.freq)},
          {HDSERVICEFLAGS,str}
      }));
}
#endif


#endif

Return<V2_1::Result> TunerSession::cancelAnnouncement() {
   ALOGV("%s", __func__);
   if (mCallback2_1 != nullptr) {
	mCallback2_1->onTrafficAnnouncement(false);
	mCallback2_1->onEmergencyAnnouncement(false);
   }
   return V2_1::Result::OK;
}

Return<V2_1::Result> TunerSession::setConfiguration(const BandConfig& config) {
   ALOGV("%s", __func__);
   if (mCallback2_1 != nullptr) {
	mCallback2_1->onConfigurationChanged(config);
   }
   return V2_1::Result::OK;
}

Return<void> TunerSession::getConfiguration(getConfiguration_cb _hidl_cb) {
    ALOGV("%s", __func__);
    _hidl_cb(V2_1::Result::OK, {});
    return {};
}

Return<V2_1::Result> TunerSession::startBackgroundScan() {
   ALOGV("%s", __func__);
   return V2_1::Result::OK;
}






}  // namespace implementation
}  // namespace V2_0
}  // namespace broadcastradio
}  // namespace hardware
}  // namespace android
