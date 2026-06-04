package com.runrev.android.billing;

import android.app.*;
import android.content.*;
import android.content.IntentSender.SendIntentException;

public abstract class PurchaseObserver
{
	private static final String TAG = "PurchaseObserver";
	private final Activity mActivity;
	
	public PurchaseObserver(Activity activity)
	{
		mActivity = activity;
	}
	
    
    // Sent to the observer to indicate a change in the purchase state
    public abstract void onPurchaseStateChanged(String productId, int state);
    
	// Sent to the observer once product details have been successfully received
    public abstract void onProductDetailsReceived(String productId);
    
    // Sent to the observer once product details have NOT been successfully received
    public abstract void onProductDetailsError(String productId, String error);

	void startBuyPageActivity(PendingIntent pendingIntent, Intent intent)
	{
		try
		{
			mActivity.startIntentSender(pendingIntent.getIntentSender(), intent, 0, 0, 0);
		}
		catch (SendIntentException e)
		{
		}
	}
}
