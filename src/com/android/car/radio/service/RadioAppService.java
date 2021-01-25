/**
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.car.radio.service;

import static com.android.car.radio.util.Remote.tryExec;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.hardware.radio.ProgramList;
import android.hardware.radio.ProgramSelector;
import android.hardware.radio.RadioManager.ProgramInfo;
import android.hardware.radio.RadioManager;
import android.hardware.radio.RadioTuner;
import android.hardware.radio.Announcement;
import android.media.browse.MediaBrowser.MediaItem;
import android.media.session.PlaybackState;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.RemoteException;
import android.os.SystemClock;
import android.service.media.MediaBrowserService;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.lifecycle.Lifecycle;
import androidx.lifecycle.LifecycleOwner;
import androidx.lifecycle.LifecycleRegistry;

import com.android.car.broadcastradio.support.media.BrowseTree;
import com.android.car.radio.audio.AudioStreamController;
import com.android.car.radio.bands.ProgramType;
import com.android.car.radio.bands.RegionConfig;
import com.android.car.radio.media.TunerSession;
import com.android.car.radio.platform.ImageMemoryCache;
import com.android.car.radio.platform.RadioManagerExt;
import com.android.car.radio.platform.RadioTunerExt;
import com.android.car.radio.platform.RadioTunerExt.TuneCallback;
import com.android.car.radio.storage.RadioStorage;
import com.android.car.radio.util.Log;

import java.lang.Runnable;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.Iterator;
import java.util.HashSet;
import java.util.Map;
import java.util.HashMap;
import java.util.List;
import java.util.Objects;
import java.util.Set;
import java.util.Arrays;

/**
 * A service handling hardware tuner session and audio streaming.
 */
public class RadioAppService extends MediaBrowserService implements LifecycleOwner {
    private static final String TAG = "BcRadioApp.service";

    public static String ACTION_APP_SERVICE = "com.android.car.radio.ACTION_APP_SERVICE";
    private static final long PROGRAM_LIST_RATE_LIMITING = 1000;

    private final Object mLock = new Object();
    private final LifecycleRegistry mLifecycleRegistry = new LifecycleRegistry(this);
    private final List<IRadioAppCallback> mRadioAppCallbacks = new ArrayList<>();
    private RadioAppServiceWrapper mWrapper;

    private RadioManagerExt mRadioManager;
    @Nullable private RadioTunerExt mRadioTuner;
    @Nullable private ProgramList mProgramList;

    private Announcement announcement;
    private Announcement.OnListUpdatedListener mAnnouncementListener;

    private RadioStorage mRadioStorage;
    private ImageMemoryCache mImageCache;
    @Nullable private AudioStreamController mAudioStreamController;

    private BrowseTree mBrowseTree;
    private TunerSession mMediaSession;

    // current observables state for newly bound IRadioAppCallbacks
    private ProgramInfo mCurrentProgram = null;
    private List<Announcement> mAnnouncementList = null;
    private int mCurrentPlaybackState = PlaybackState.STATE_NONE;
    private long mLastProgramListPush;

    private RegionConfig mRegionConfigCache;

    @Override
    public void onCreate() {
        super.onCreate();

        Log.i(TAG, "Starting RadioAppService...");

        mWrapper = new RadioAppServiceWrapper(mBinder);
        mRadioManager = new RadioManagerExt(this);
        mRadioStorage = RadioStorage.getInstance(this);
        mImageCache = new ImageMemoryCache(mRadioManager, 1000);
        mRadioTuner = mRadioManager.openSession(mHardwareCallback, null);
	ArrayList<String> list = new ArrayList<>();
	mAnnouncementListener = new Announcement.OnListUpdatedListener() {
	    @Override
	    public void onListUpdated(Collection<Announcement> activeAnnouncements) {
		List<Announcement> lists = new ArrayList<>(activeAnnouncements);
		onAnnouncementListUpdated(lists);
	    }
	};
        if (mRadioTuner == null) {
            Log.e(TAG, "Couldn't open tuner session");
            return;
        }

        mAudioStreamController = new AudioStreamController(this, mRadioTuner,
                this::onPlaybackStateChanged);
        mBrowseTree = new BrowseTree(this, mImageCache);
        mMediaSession = new TunerSession(this, mBrowseTree, mWrapper, mImageCache);
        setSessionToken(mMediaSession.getSessionToken());
        mBrowseTree.setAmFmRegionConfig(mRadioManager.getAmFmRegionConfig());
        mRadioStorage.getFavorites().observe(this,
                favs -> mBrowseTree.setFavorites(new HashSet<>(favs)));

	Set<Integer> identifierTypes = new HashSet<Integer>();
	//identifierTypes.addAll(Arrays.asList(new Integer[] {ProgramSelector.IDENTIFIER_TYPE_AMFM_FREQUENCY, ProgramSelector.IDENTIFIER_TYPE_DAB_SID_EXT}));
	identifierTypes.addAll(Arrays.asList(new Integer[] {ProgramSelector.IDENTIFIER_TYPE_AMFM_FREQUENCY}));

	Set<ProgramSelector.Identifier> identifiers = new HashSet<ProgramSelector.Identifier>();
	ProgramSelector.Identifier identifier = new ProgramSelector.Identifier(ProgramSelector.IDENTIFIER_TYPE_RDS_PI, 1234);
	identifiers.add(identifier);

	ProgramList.Filter filter = new ProgramList.Filter(identifierTypes, identifiers, true, false);

        mProgramList = mRadioTuner.getDynamicProgramList(filter);
	Map<String, String> parameters = new HashMap<String, String>();
	parameters.put("com.announcement.start", "true");
	parameters.put("com.announcement.stop", "false");
	Map<String, String> returnParameters = mRadioTuner.setParameters(parameters);
	Log.e(TAG, "return SetParameters1 = "+returnParameters.get("com.announcement.start"));
	Log.e(TAG, "return SetParameters2 = "+returnParameters.get("com.announcement.stop"));

	List<String> keys = new ArrayList<String> ();
	keys.add("com.announcement.start");
	keys.add("com.announcement.stop");
	Map<String, String> returnGetParameters = mRadioTuner.getParameters(keys);
	Log.e(TAG, "return GetParameters1 = "+returnGetParameters.get("com.announcement.start"));
	Log.e(TAG, "return GetParameters2 = "+returnGetParameters.get("com.announcement.stop"));
        if (mProgramList != null) {
            mBrowseTree.setProgramList(mProgramList);
            mProgramList.registerListCallback(new ProgramList.ListCallback() {
                @Override
                public void onItemChanged(@NonNull ProgramSelector.Identifier id) {
                    onProgramListChanged();
                }
            });
            mProgramList.addOnCompleteListener(this::pushProgramListUpdate);
        }

        tuneToDefault(null);
        mAudioStreamController.requestMuted(false);

        mLifecycleRegistry.markState(Lifecycle.State.CREATED);
	addAnnouncement();
	setConfiguration(buildConfiguration());
        getConfiguration();
        startBackgroundScan();
        isAntennaConnected();
    }

    private RadioManager.BandConfig buildConfiguration() {
        RadioManager.FmBandDescriptor desc = new RadioManager.FmBandDescriptor(RadioManager.REGION_JAPAN, 0, 0, 0, 0, false, false, false, false, false);
        RadioManager.BandConfig bandConfig = new RadioManager.FmBandConfig.Builder(desc).build();
        return bandConfig;
    }

    public void setConfiguration(RadioManager.BandConfig config) {
        Log.e(TAG, "setConfiguration call in RadioAppService");
	mRadioTuner.setConfiguration(config);
    }

    public void getConfiguration() {
        Log.e(TAG, "getConfiguration call in RadioAppService");
	RadioManager.BandConfig[] config = new RadioManager.BandConfig[1];
	mRadioTuner.getConfiguration(config);
    }

    public void startBackgroundScan() {
        Log.e(TAG, "startBackgroundScan call in RadioAppService");
	mRadioTuner.startBackgroundScan();
    }

    public void isAntennaConnected() {
        Log.e(TAG, "isAntennaConnected call in RadioAppService");
	mRadioTuner.isAntennaConnected();
    }

    public void addAnnouncement() {
	Set<Integer> announcementTypes = new HashSet<Integer>();
	announcementTypes.add(Announcement.TYPE_TRAFFIC);
	announcementTypes.add(Announcement.TYPE_WEATHER);
	announcementTypes.add(Announcement.TYPE_EMERGENCY);
	announcementTypes.add(Announcement.TYPE_NEWS);
	announcementTypes.add(Announcement.TYPE_WARNING);
	announcementTypes.add(Announcement.TYPE_SPORT);
	announcementTypes.add(Announcement.TYPE_MISC);
	mRadioManager.addAnnouncementListener(announcementTypes, mAnnouncementListener);
    }


    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        mLifecycleRegistry.markState(Lifecycle.State.STARTED);
        if (BrowseTree.ACTION_PLAY_BROADCASTRADIO.equals(intent.getAction())) {
            Log.i(TAG, "Executing general play radio intent");
            mMediaSession.getController().getTransportControls().playFromMediaId(
                    mBrowseTree.getRoot().getRootId(), null);
            return START_NOT_STICKY;
        }

        return super.onStartCommand(intent, flags, startId);
    }

    @Override
    public IBinder onBind(Intent intent) {
        mLifecycleRegistry.markState(Lifecycle.State.STARTED);
        if (mRadioTuner == null) return null;
        if (ACTION_APP_SERVICE.equals(intent.getAction())) {
            return mBinder;
        }
        return super.onBind(intent);
    }

    @Override
    public boolean onUnbind(Intent intent) {
        mLifecycleRegistry.markState(Lifecycle.State.CREATED);
        return false;
    }

    @Override
    public void onDestroy() {
        Log.i(TAG, "Shutting down RadioAppService...");

        mLifecycleRegistry.markState(Lifecycle.State.DESTROYED);

        if (mMediaSession != null) mMediaSession.release();
        close();

        super.onDestroy();
    }

    @NonNull
    @Override
    public Lifecycle getLifecycle() {
        return mLifecycleRegistry;
    }

    private void onPlaybackStateChanged(int newState) {
        synchronized (mLock) {
            mCurrentPlaybackState = newState;
            for (IRadioAppCallback callback : mRadioAppCallbacks) {
                tryExec(() -> callback.onPlaybackStateChanged(newState));
            }
        }
    }

    private void onAnnouncementListUpdated(List<Announcement> list) {
        Log.e(TAG, "Received Announcement callback in application");
	mAnnouncementList = list;
        synchronized (mLock) {
            for (IRadioAppCallback callback : mRadioAppCallbacks) {
                Log.e(TAG, "sending callback from Service to Wrapper");
                tryExec(() -> callback.onAnnouncementListUpdated(list));
            }
        }
    }

    private void onProgramListChanged() {
        synchronized (mLock) {
            if (SystemClock.elapsedRealtime() - mLastProgramListPush > PROGRAM_LIST_RATE_LIMITING) {
                pushProgramListUpdate();
            }
        }
    }

    private void pushProgramListUpdate() {
        List<ProgramInfo> plist = mProgramList.toList();

        synchronized (mLock) {
            mLastProgramListPush = SystemClock.elapsedRealtime();
            for (IRadioAppCallback callback : mRadioAppCallbacks) {
                tryExec(() -> callback.onProgramListChanged(plist));
            }
        }
    }

    private void tuneToDefault(@Nullable ProgramType pt) {
        synchronized (mLock) {
            if (mRadioTuner == null) throw new IllegalStateException("Tuner session is closed");
            TuneCallback tuneCb = mAudioStreamController.preparePlayback(
                    AudioStreamController.OPERATION_TUNE);
            if (tuneCb == null) return;

            ProgramSelector sel = mRadioStorage.getRecentlySelected(pt);
            if (sel != null) {
                Log.i(TAG, "Restoring recently selected program: " + sel);
                mRadioTuner.tune(sel, tuneCb);
                return;
            }

            if (pt == null) pt = ProgramType.FM;
            Log.i(TAG, "No recently selected program set, selecting default channel for " + pt);
            pt.tuneToDefault(mRadioTuner, mWrapper.getRegionConfig(), tuneCb);
        }
    }

    private void close() {
        synchronized (mLock) {
            if (mAudioStreamController != null) {
                mAudioStreamController.requestMuted(true);
                mAudioStreamController = null;
            }
            if (mProgramList != null) {
                mProgramList.close();
                mProgramList = null;
            }
            if (mRadioTuner != null) {
                mRadioTuner.close();
                mRadioTuner = null;
            }
        }
    }

    @Override
    public BrowserRoot onGetRoot(String clientPackageName, int clientUid, Bundle rootHints) {
        /* Radio application may restrict who can read its MediaBrowser tree.
         * Our implementation doesn't.
         */
        return mBrowseTree.getRoot();
    }

    @Override
    public void onLoadChildren(final String parentMediaId, final Result<List<MediaItem>> result) {
        mBrowseTree.loadChildren(parentMediaId, result);
    }

    private void onHardwareError() {
        close();
        stopSelf();
        synchronized (mLock) {
            for (IRadioAppCallback callback : mRadioAppCallbacks) {
                tryExec(() -> callback.onHardwareError());
            }
        }
    }

    private IRadioAppService.Stub mBinder = new IRadioAppService.Stub() {
        @Override
        public void addCallback(IRadioAppCallback callback) throws RemoteException {
            synchronized (mLock) {
                Log.e(TAG, "addCallback ========");
                if (mCurrentProgram != null) {
		     callback.onCurrentProgramChanged(mCurrentProgram);
                     //Log.e(TAG, "addCallback cancelAnnouncement ========");
		     //cancelAnnouncement();
		}
                callback.onPlaybackStateChanged(mCurrentPlaybackState);
                if (mProgramList != null) {
		     callback.onProgramListChanged(mProgramList.toList());
		}
		if (mAnnouncementList != null) {
		     callback.onAnnouncementListUpdated(mAnnouncementList);
		}
                mRadioAppCallbacks.add(callback);
            }
        }

        @Override
        public void removeCallback(IRadioAppCallback callback) {
            synchronized (mLock) {
                mRadioAppCallbacks.remove(callback);
            }
        }

        @Override
        public void tune(ProgramSelector sel, ITuneCallback callback) {
            Objects.requireNonNull(callback);
            synchronized (mLock) {
                if (mRadioTuner == null) throw new IllegalStateException("Tuner session is closed");
                TuneCallback tuneCb = mAudioStreamController.preparePlayback(
                        AudioStreamController.OPERATION_TUNE);
                if (tuneCb == null) return;
                mRadioTuner.tune(sel, tuneCb.alsoCall(
                        succ -> tryExec(() -> callback.onFinished(succ))));
            }
        }

        @Override
        public void seek(boolean forward, ITuneCallback callback) {
            Objects.requireNonNull(callback);
            synchronized (mLock) {
                if (mRadioTuner == null) throw new IllegalStateException("Tuner session is closed");
                TuneCallback tuneCb = mAudioStreamController.preparePlayback(forward
                        ? AudioStreamController.OPERATION_SEEK_FWD
                        : AudioStreamController.OPERATION_SEEK_BKW);
                if (tuneCb == null) return;
                mRadioTuner.seek(forward, tuneCb.alsoCall(
                        succ -> tryExec(() -> callback.onFinished(succ))));
            }
        }

        @Override
        public void step(boolean forward, ITuneCallback callback) {
            Objects.requireNonNull(callback);
            synchronized (mLock) {
                if (mRadioTuner == null) throw new IllegalStateException("Tuner session is closed");
                TuneCallback tuneCb = mAudioStreamController.preparePlayback(forward
                        ? AudioStreamController.OPERATION_STEP_FWD
                        : AudioStreamController.OPERATION_STEP_BKW);
                if (tuneCb == null) return;
                mRadioTuner.step(forward, tuneCb.alsoCall(
                        succ -> tryExec(() -> callback.onFinished(succ))));
            }
        }

        @Override
        public void setMuted(boolean muted) {
            if (mAudioStreamController == null) return;
            if (muted) mRadioTuner.cancel();
            mAudioStreamController.requestMuted(muted);
        }

        @Override
        public void switchBand(ProgramType band) {
            tuneToDefault(band);
        }

        @Override
        public boolean isProgramListSupported() {
            return mProgramList != null;
        }

        @Override
        public RegionConfig getRegionConfig() {
            synchronized (mLock) {
                if (mRegionConfigCache == null) {
                    mRegionConfigCache = new RegionConfig(mRadioManager.getAmFmRegionConfig());
                }
                return mRegionConfigCache;
            }
        }
    
	@Override
	public void cancelAnnouncement() {
	    Log.e(TAG, "cancel Announcement in RadioAppService ");
	    mRadioTuner.cancelAnnouncement();
    	}
    };

    private RadioTuner.Callback mHardwareCallback = new RadioTuner.Callback() {
        @Override
        public void onProgramInfoChanged(ProgramInfo info) {
            Objects.requireNonNull(info);

            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "Program info changed: " + info);
            }

            synchronized (mLock) {
                mCurrentProgram = info;

                /* Storing recently selected program might be limited to explicit tune calls only
                 * (including next/prev seek), but the implementation would be nontrivial with the
                 * current API. For now, let's make it simple and make it react to all program
                 * selector changes. */
                mRadioStorage.setRecentlySelected(info.getSelector());
                for (IRadioAppCallback callback : mRadioAppCallbacks) {
                    tryExec(() -> callback.onCurrentProgramChanged(info));
                }
            }
        }

        @Override
        public void onError(int status) {
            switch (status) {
                case RadioTuner.ERROR_HARDWARE_FAILURE:
                case RadioTuner.ERROR_SERVER_DIED:
                    Log.e(TAG, "Fatal hardware error: " + status);
                    onHardwareError();
                    break;
                default:
                    Log.w(TAG, "Hardware error: " + status);
            }
        }

        @Override
        public void onControlChanged(boolean control) {
            if (!control) onHardwareError();
        }
    };
}

