package com.runrev.android;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

public class EngineReceiver extends BroadcastReceiver
{
    public static final String TAG = "revandroid.EngineReceiver";
    
    public void onReceive(Context context, Intent intent)
    {
        if (!NotificationModule.onReceive(context, intent))
        {
            Log.i(TAG, "unhandled intent: " + intent);
        }
    }
}

