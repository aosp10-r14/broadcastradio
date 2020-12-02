/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.car.radio;

import android.app.AlertDialog;
import android.content.DialogInterface;

import android.hardware.radio.Announcement;
import android.hardware.radio.ProgramSelector;
import android.hardware.radio.RadioManager.ProgramInfo;
import android.hardware.radio.RadioMetadata;
import android.media.session.PlaybackState;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemProperties;
import android.view.View;
import android.widget.ListView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.lifecycle.LiveData;

import com.android.car.broadcastradio.support.Program;
import com.android.car.broadcastradio.support.platform.ProgramInfoExt;
import com.android.car.radio.bands.ProgramType;
import com.android.car.radio.bands.RegionConfig;
import com.android.car.radio.service.RadioAppService;
import com.android.car.radio.service.RadioAppServiceWrapper;
import com.android.car.radio.service.RadioAppServiceWrapper.ConnectionState;
import com.android.car.radio.storage.RadioStorage;
import com.android.car.radio.util.Log;

import java.lang.Runnable;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;

/**
 * The main controller of the radio app.
 */
public class RadioController {
    private static final String TAG = "BcRadioApp.controller";

    private final Object mLock = new Object();
    private final RadioActivity mActivity;

    private final RadioAppServiceWrapper mAppService = new RadioAppServiceWrapper();
    private final DisplayController mDisplayController;
    private final RadioStorage mRadioStorage;

    private List<Announcement> mAnnouncementList;
    private ProgramSelector mCurrentSelector;
    private ProgramSelector mPreviousSelector;
    private boolean mIsAnnouncementActive = false;
    private AlertDialog dialog;

    @Nullable private ProgramInfo mCurrentProgram;

    public RadioController(@NonNull RadioActivity activity) {
        mActivity = Objects.requireNonNull(activity);

        mDisplayController = new DisplayController(activity, this);

        mRadioStorage = RadioStorage.getInstance(activity);
        mRadioStorage.getFavorites().observe(activity, this::onFavoritesChanged);

        mAppService.getCurrentProgram().observe(activity, this::onCurrentProgramChanged);
	mAppService.getAnnouncementList().observe(activity, this::onAnnouncementListUpdated);
        Log.e(TAG, "Registering observer for Announcement");
        mAppService.getConnectionState().observe(activity, this::onConnectionStateChanged);

        mDisplayController.setBackwardSeekButtonListener(this::onBackwardSeekClick);
        mDisplayController.setForwardSeekButtonListener(this::onForwardSeekClick);
        mDisplayController.setPlayButtonCallback(this::onSwitchToPlayState);
        mDisplayController.setFavoriteToggleListener(this::onFavoriteToggled);
    }

    private void onConnectionStateChanged(@ConnectionState int state) {
        mDisplayController.setState(state);
        if (state == RadioAppServiceWrapper.STATE_CONNECTED) {
            mActivity.setProgramListSupported(mAppService.isProgramListSupported());
            mActivity.setSupportedProgramTypes(getRegionConfig().getSupportedProgramTypes());
        }
    }

    /**
     * Starts the controller and establishes connection with {@link RadioAppService}.
     */
    public void start() {
        mAppService.bind(mActivity);
    }

    /**
     * Closes {@link RadioAppService} connection and cleans up the resources.
     */
    public void shutdown() {
        mAppService.unbind();
    }

    /**
     * See {@link RadioAppServiceWrapper#getPlaybackState}.
     */
    @NonNull
    public LiveData<Integer> getPlaybackState() {
        return mAppService.getPlaybackState();
    }

    /**
     * See {@link RadioAppServiceWrapper#getCurrentProgram}.
     */
    @NonNull
    public LiveData<ProgramInfo> getCurrentProgram() {
        return mAppService.getCurrentProgram();
    }

    /**
     * See {@link RadioAppServiceWrapper#getProgramList}.
     */
    @NonNull
    public LiveData<List<ProgramInfo>> getProgramList() {
        return mAppService.getProgramList();
    }

    @NonNull
    public LiveData<List<Announcement>> getAnnouncementList() {
        return mAppService.getAnnouncementList();
    }
    /**
     * Tunes the radio to the given channel.
     */
    public void tune(ProgramSelector sel) {
        mAppService.tune(sel);
    }

    /**
     * Steps the radio tuner in the given direction, see {@link RadioAppServiceWrapper#step}.
     */
    public void step(boolean forward) {
        mAppService.step(forward);
    }

    /**
     * Switch radio band. Currently, this only supports FM and AM bands.
     *
     * @param pt {@link ProgramType} to switch to.
     */
    public void switchBand(@NonNull ProgramType pt) {
        mAppService.switchBand(pt);
    }

    @NonNull
    public RegionConfig getRegionConfig() {
        return mAppService.getRegionConfig();
    }

    private void onFavoritesChanged(List<Program> favorites) {
        synchronized (mLock) {
            if (mCurrentProgram == null) return;
            boolean isFav = RadioStorage.isFavorite(favorites, mCurrentProgram.getSelector());
            mDisplayController.setCurrentIsFavorite(isFav);
        }
    }

    private void onCurrentProgramChanged(@NonNull ProgramInfo info) {
        Log.e(TAG, "onCurrentProgramChanged in Controller");
        synchronized (mLock) {
            mCurrentProgram = Objects.requireNonNull(info);
            ProgramSelector sel = info.getSelector();
	    if (!mIsAnnouncementActive) {
		mPreviousSelector = mCurrentSelector;
		mCurrentSelector = sel;
	    }
            Log.e(TAG, "onCurrentProgramChanged in Controller - Selector stored");
            RadioMetadata meta = ProgramInfoExt.getMetadata(info);

            mDisplayController.setChannel(sel);

            mDisplayController.setStationName(
                    ProgramInfoExt.getProgramName(info, ProgramInfoExt.NAME_NO_CHANNEL_FALLBACK));

            if (meta.containsKey(RadioMetadata.METADATA_KEY_TITLE)
                    || meta.containsKey(RadioMetadata.METADATA_KEY_ARTIST)) {
                mDisplayController.setDetails(
                        meta.getString(RadioMetadata.METADATA_KEY_TITLE),
                        meta.getString(RadioMetadata.METADATA_KEY_ARTIST));
            } else {
                mDisplayController.setDetails(meta.getString(RadioMetadata.METADATA_KEY_RDS_RT));
            }

            mDisplayController.setCurrentIsFavorite(mRadioStorage.isFavorite(sel));
        }
    }

    private void onBackwardSeekClick(View v) {
        mDisplayController.startSeekAnimation(false);
        mAppService.seek(false);
    }

    private void onForwardSeekClick(View v) {
        mDisplayController.startSeekAnimation(true);
        mAppService.seek(true);
    }

    private void onSwitchToPlayState(@PlaybackState.State int newPlayState) {
        switch (newPlayState) {
            case PlaybackState.STATE_PLAYING:
                mAppService.setMuted(false);
                break;
            case PlaybackState.STATE_PAUSED:
            case PlaybackState.STATE_STOPPED:
                mAppService.setMuted(true);
                break;
            default:
                Log.e(TAG, "Invalid request to switch to play state " + newPlayState);
        }
    }

    private void onFavoriteToggled(boolean addFavorite) {
        synchronized (mLock) {
            if (mCurrentProgram == null) return;

            if (addFavorite) {
                mRadioStorage.addFavorite(Program.fromProgramInfo(mCurrentProgram));
            } else {
                mRadioStorage.removeFavorite(mCurrentProgram.getSelector());
            }
        }
    }

    private void onAnnouncementListUpdated(List<Announcement> list) {
        Log.e(TAG, "onAnnouncementListUpdated in Controller");
	ArrayList<String> annList = new ArrayList<>();
	mAnnouncementList = list;
	for (int i = 0; i<list.size(); i++) {	
	    Map<String, String> vendorInfo = list.get(i).getVendorInfo();
	    boolean isStop =  vendorInfo.get("com.announcement.stop") != null && vendorInfo.get("com.announcement.stop").equals("true");
	    boolean isStart =  vendorInfo.get("com.announcement.start") != null && vendorInfo.get("com.announcement.start").equals("true");
            Log.e(TAG, "Announcement vendor values isStop = "+isStop +" and isStart = "+isStart);
	    if (isStop || SystemProperties.get("sys.ann.value").equals("stop")) {
		stopAnnouncement();
		return;
	    }
	if (SystemProperties.get("sys.ann.value", "false").equals("start"))  return;
	    if (isStart) {
		    SystemProperties.set("sys.ann.value", "start");
		    switch(list.get(i).getType()) {
			case Announcement.TYPE_TRAFFIC:
			    annList.add("Traffic Announcement");
			    break;
			case Announcement.TYPE_EMERGENCY:
			    annList.add("Emergency Announcement");
			    break;
			case Announcement.TYPE_NEWS:
			    annList.add("News Announcement");
			    break;
			case Announcement.TYPE_SPORT:
			    annList.add("Sport Announcement");
			    break;
			case Announcement.TYPE_WARNING:
			    annList.add("Warning Announcement");
			    break;
			case Announcement.TYPE_WEATHER:
			    annList.add("Weather Announcement");
			    break;
			case Announcement.TYPE_MISC:
			    annList.add("Miscellaneous Announcement");
			    break;
			case Announcement.TYPE_EVENT:
			    annList.add("Event Announcement");
			    break;
			default:
			    break;
		     }
	    }
	}
	showAnnouncement(annList);
    }

    private void stopAnnouncement() {
        Log.e(TAG, "stopAnnouncement in Controller");
	if (mIsAnnouncementActive) {
            Log.e(TAG, "stopAnnouncement - Fallback to previous tune channel");
	    tune(mCurrentSelector);
	    mIsAnnouncementActive = false;
	} else {
	    if (dialog != null && dialog.isShowing()) {
                Log.e(TAG, "stopAnnouncement - Dismissing dialog");
		dialog.dismiss();
		dialog = null;
	    }	    
	}
	SystemProperties.set("sys.ann.value", "false");
    }


    private void showAnnouncement(final ArrayList<String> list) {
        Log.e(TAG, "showAnnouncement in Controller");
        new Handler(Looper.getMainLooper()).post(new Runnable() {
            @Override
            public void run() {
                Log.e(TAG, "showAnnouncement Dialog");
                openDialog(list);
            }
        });
    }

    public void openDialog(ArrayList<String> list) {
        Log.e(TAG, "openDialog in Controller");
        // setup the alert builder
        AlertDialog.Builder builder = new AlertDialog.Builder(mActivity);
        builder.setTitle("Choose an Announcement");

        // add a radio button list
        int checkedItem = 1;
        builder.setSingleChoiceItems(list.toArray
        (new String[list.size()]), checkedItem, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
        	Log.e(TAG, "onClick - "+which);
            }
        });

        // add OK and Cancel buttons
        builder.setPositiveButton("Play", new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
		ListView lw = ((AlertDialog)dialog).getListView();
        	Log.e(TAG, "Play button clicked - "+lw.getCheckedItemPosition());
		//Object checkedItem = lw.getAdapter().getItem(lw.getCheckedItemPosition());
		selectAnnouncement(lw.getCheckedItemPosition());	
            }
        });
        builder.setNegativeButton("Cancel", null);

        // create and show the alert dialog
        dialog = builder.create();
        dialog.show();
    }

    private void selectAnnouncement(int position) {
	Announcement selectedAnnouncement = mAnnouncementList.get(position);
	ProgramSelector sel = selectedAnnouncement.getSelector();
        Log.e(TAG, "Announcement Selected "+sel);
	tune(sel);
	mIsAnnouncementActive = true;
	SystemProperties.set("sys.ann.value", "false");
    }

}
