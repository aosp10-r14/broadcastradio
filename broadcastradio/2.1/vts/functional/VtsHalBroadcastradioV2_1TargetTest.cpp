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

#define EGMOCK_VERBOSE 1

#include <VtsHalHidlTargetTestBase.h>
#include <android-base/logging.h>
#include <android-base/strings.h>
#include <android/hardware/broadcastradio/2.1/IBroadcastRadio.h>
#include <android/hardware/broadcastradio/2.1/ITunerSession.h>
#include <android/hardware/broadcastradio/2.1/ITunerCallback.h>
#include <android/hardware/broadcastradio/2.0/types.h>
#include <broadcastradio-utils-2x/Utils.h>
#include <broadcastradio-vts-utils/call-barrier.h>
#include <broadcastradio-vts-utils/environment-utils.h>
#include <broadcastradio-vts-utils/mock-timeout.h>
#include <broadcastradio-vts-utils/pointer-utils.h>
#include <cutils/bitops.h>
#include <gmock/gmock.h>

#include <chrono>
#include <optional>
#include <regex>

namespace android {
namespace hardware {
namespace broadcastradio {
namespace V2_1 {
namespace vts {

using namespace std::chrono_literals;

using std::unordered_set;
using std::vector;
using testing::_;
using testing::AnyNumber;
using testing::ByMove;
using testing::DoAll;
using testing::Invoke;
using testing::Mock;
using testing::SaveArg;

using broadcastradio::vts::BroadcastRadioHidlEnvironment;
using broadcastradio::vts::CallBarrier;
using broadcastradio::vts::clearAndWait;
using utils::make_identifier;
using utils::make_selector_amfm;
using ::android::hardware::broadcastradio::V2_1::Result;
using ::android::hardware::broadcastradio::V2_0::ProgramInfo;
using ::android::hardware::broadcastradio::V2_0::ProgramSelector;
using ::android::hardware::broadcastradio::V2_0::VendorKeyValue;
using ::android::hardware::broadcastradio::V2_0::ProgramListChunk;

static constexpr auto kConfigTimeout = 10s;

class TunerCallbackMock : public ITunerCallback {
   public:
    TunerCallbackMock();

    MOCK_TIMEOUT_METHOD2(onTuneFailed_, Return<void>(V2_0::Result, const ProgramSelector&));
    MOCK_TIMEOUT_METHOD1(onCurrentProgramInfoChanged_, Return<void>(const ProgramInfo&));
    virtual Return<void> onCurrentProgramInfoChanged(const ProgramInfo& info);
    virtual Return<void> onTuneFailed(V2_0::Result, const ProgramSelector& sel);
    virtual Return<void> onParametersUpdated(const hidl_vec<VendorKeyValue>& parameters);

    MOCK_TIMEOUT_METHOD2(configChange, Return<void>(Result, const BandConfig&));
    virtual Return<void> onConfigurationChanged(const BandConfig& config);
    virtual Return<void> onTrafficAnnouncement(const bool active);
    virtual Return<void> onEmergencyAnnouncement(const bool active);
    virtual Return<void> onBackgroundScanAvailabilityChange(const bool isAvailable);
    virtual Return<void> backgroundScanComplete();
    virtual Return<void> onControlChanged(const bool control);
    virtual Return<void> onError(const int32_t status);
    Return<void> onProgramListUpdated(const ProgramListChunk& chunk);
    MOCK_METHOD1(onAntennaStateChange, Return<void>(bool connected));
    MOCK_METHOD1(onTrafficAnnouncement_, Return<void>(bool connected));
    MOCK_TIMEOUT_METHOD1(onParametersUpdated_, Return<void>(const hidl_vec<VendorKeyValue>& parameters));

    MOCK_TIMEOUT_METHOD0(onProgramListReady, void());

    std::mutex mLock;
};

static BroadcastRadioHidlEnvironment<IBroadcastRadio>* gEnv = nullptr;

class BroadcastRadioHalTest : public ::testing::VtsHalHidlTargetTestBase {
   protected:
    virtual void SetUp() override;
    virtual void TearDown() override;
    sp<IBroadcastRadio> mModule;

    bool openSession();
    bool cancelAnnouncement();
    bool setConfiguration();
    bool isAntennaConnected();
    
    sp<ITunerSession> mSession;
    sp<TunerCallbackMock> mCallback = new TunerCallbackMock();
};

MATCHER_P(InfoHasId, id,
          std::string(negation ? "does not contain" : "contains") + " " + toString(id)) {
    auto ids = utils::getAllIds(arg.selector, utils::getType(id));
    return ids.end() != find(ids.begin(), ids.end(), id.value);
}

TunerCallbackMock::TunerCallbackMock() {
}

Return<void> TunerCallbackMock::onTuneFailed(V2_0::Result result, const ProgramSelector& sel) {
    return onTuneFailed_(result, sel);
}

Return<void> TunerCallbackMock::onParametersUpdated(const hidl_vec<VendorKeyValue>& parameters) {
    LOG(DEBUG) << "onParametersUpdated in VTS " << toString(parameters);
    return onParametersUpdated_(parameters);
}

Return<void> TunerCallbackMock::onCurrentProgramInfoChanged(const ProgramInfo& info) {
    return onCurrentProgramInfoChanged_(info);
}

Return<void> TunerCallbackMock::onProgramListUpdated(const ProgramListChunk& /*chunk*/) {
    onProgramListReady();
    return {};
}

Return<void> TunerCallbackMock::onConfigurationChanged(const BandConfig& config) {
    LOG(DEBUG) << "onConfigurationChanged in VTS ";
    configChange(V2_1::Result::OK, config);
    return {};
}

Return<void> TunerCallbackMock::onTrafficAnnouncement(const bool active) {
    LOG(DEBUG) << "onTrafficAnnouncement in VTS ";
    onTrafficAnnouncement_(active);
    return {};
}
Return<void> TunerCallbackMock::onEmergencyAnnouncement(const bool /*active*/) {
    return {};
}
Return<void> TunerCallbackMock::onBackgroundScanAvailabilityChange(const bool /*isAvailable*/) {
    return {};
}
Return<void> TunerCallbackMock::backgroundScanComplete() {
    return {};
}
Return<void> TunerCallbackMock::onControlChanged(const bool /*control*/) {
    return {};
}
Return<void> TunerCallbackMock::onError(const int32_t /*status*/) {
    return {};
}

void BroadcastRadioHalTest::SetUp() {
    EXPECT_EQ(nullptr, mModule.get()) << "Module is already open";

    // lookup HIDL service (radio module)
    mModule = getService<IBroadcastRadio>(gEnv->getServiceName<IBroadcastRadio>());
    ASSERT_NE(nullptr, mModule.get()) << "Couldn't find broadcast radio HAL implementation";

}

void BroadcastRadioHalTest::TearDown() {
    LOG(DEBUG) << "TearDown ==============#############============";
    mSession.clear();
    mModule.clear();
    clearAndWait(mCallback, 1s);
    if (mCallback == NULL) {
        LOG(DEBUG) << "TearDown ==============############### callback Null=====";
    } else {
        LOG(DEBUG) << "TearDown ==============############### callback Not Null";
    }
}

bool BroadcastRadioHalTest::openSession() {
    EXPECT_EQ(nullptr, mSession.get()) << "Session is already open";

    V2_0::Result halResult = V2_0::Result::UNKNOWN_ERROR;
    auto openCb = [&](V2_0::Result result, const sp<V2_0::ITunerSession>& session) {
        halResult = result;
        if (result != V2_0::Result::OK) return;
        mSession = ITunerSession::castFrom(session);
    };
    auto hidlResult = mModule->openSession(mCallback, openCb);

    EXPECT_TRUE(hidlResult.isOk());
    EXPECT_EQ(V2_0::Result::OK, halResult);
    EXPECT_NE(nullptr, mSession.get());

    return nullptr != mSession.get();
}

/**
 * Test session opening.
 *
 * Verifies that:
 *  - the method succeeds on a first and subsequent calls;
 *  - the method succeeds when called for the second time without
 *    closing previous session.
 */
TEST_F(BroadcastRadioHalTest, OpenSession) {
    // simply open session for the first time
    ASSERT_TRUE(openSession());

    // drop (without explicit close) and re-open the session
    mSession.clear();
    ASSERT_TRUE(openSession());

    // open the second session (the first one should be forcibly closed)
    auto secondSession = mSession;
    mSession.clear();
    ASSERT_TRUE(openSession());
    //testing::Mock::AllowLeak(&mCallback);
    //Mock::VerifyAndClearExpectations(&mCallback);
}

TEST_F(BroadcastRadioHalTest, cancelAnnouncement) {
    openSession();

    LOG(DEBUG) << "cancelAnnouncement in VTS ";
    mSession->cancelAnnouncement();    
    EXPECT_CALL(*mCallback, onTrafficAnnouncement_(false)).Times(1);
}


TEST_F(BroadcastRadioHalTest, setConfiguration) {
    openSession();

    LOG(DEBUG) << "setConfiguration in VTS ";
    BandConfig bandCb;
    EXPECT_TIMEOUT_CALL(*mCallback, configChange, Result::OK, _)
        .WillOnce(DoAll(SaveArg<1>(&bandCb), testing::Return(ByMove(Void()))));
    static const BandConfig dummyBandConfig = {};
    auto hidlResult = mSession->setConfiguration(dummyBandConfig);
    EXPECT_EQ(V2_1::Result::OK, hidlResult);
    EXPECT_TIMEOUT_CALL_WAIT(*mCallback, configChange, kConfigTimeout);
}

TEST_F(BroadcastRadioHalTest, getConfiguration) {
    openSession();

    LOG(DEBUG) << "getConfiguration in VTS ";

    BandConfig halConfig;
    //EXPECT_TIMEOUT_CALL(*mCallback, configChange, Result::OK, _)
      //  .WillOnce(DoAll(SaveArg<1>(&halConfig), testing::Return(ByMove(Void()))));
    Result halResult = Result::OK;
    mSession->getConfiguration([&](Result result, const BandConfig& config) {
       halResult = result;
       halConfig = config;
    });
    EXPECT_EQ(Result::OK, halResult);
    //EXPECT_TIMEOUT_CALL_WAIT(*mCallback, configChange, kConfigTimeout);
}

TEST_F(BroadcastRadioHalTest, startBackgroundScan) {
    openSession();

    LOG(DEBUG) << "startBackgroundScan in VTS ";
    auto hidlResult = mSession->startBackgroundScan();    
    EXPECT_EQ(V2_1::Result::OK, hidlResult);
}

TEST_F(BroadcastRadioHalTest, isAntennaConnected) {
    openSession();

//    bool connected = false;
//    Result halResult = Result::OK;
    LOG(DEBUG) << "isAntennaConnected in VTS ";
    auto cb = [&](V2_1::Result /*result*/, bool /*isConnected*/) {
//       halResult = result;
//       connected = isConnected;
    };
    mModule->isAntennaConnected(cb);
}

}  // namespace vts
}  // namespace V2_1
}  // namespace broadcastradio
}  // namespace hardware
}  // namespace android

int main(int argc, char** argv) {
    using android::hardware::broadcastradio::V2_1::vts::gEnv;
    using android::hardware::broadcastradio::V2_1::IBroadcastRadio;
    using android::hardware::broadcastradio::vts::BroadcastRadioHidlEnvironment;
    android::base::SetDefaultTag("BcRadio.vts.2.1");
    android::base::SetMinimumLogSeverity(android::base::VERBOSE);
    gEnv = new BroadcastRadioHidlEnvironment<IBroadcastRadio>;
    ::testing::AddGlobalTestEnvironment(gEnv);
    ::testing::InitGoogleTest(&argc, argv);
    gEnv->init(&argc, argv);
    return RUN_ALL_TESTS();
}
