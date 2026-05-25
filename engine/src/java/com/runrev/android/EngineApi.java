package com.runrev.android;

import java.lang.*;
import android.app.*;
import android.view.*;
import android.content.*;

public interface EngineApi
{
	public interface ActivityResultCallback
	{
		public abstract void handleActivityResult(int resultCode, Intent data);
	};
	
	// Runs the activity then waits for the result, invoking the callback
	// with the resultCode and data. (Must be run from script thread, callback will
	// be on script thread).
	public abstract void runActivity(Intent intent, ActivityResultCallback callback);
	
	public abstract Activity getActivity();
	public abstract ViewGroup getContainer();
};
