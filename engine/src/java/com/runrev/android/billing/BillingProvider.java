package com.runrev.android.billing;

import java.util.*;
import android.content.*;
import android.app.*;
import android.util.*;

public interface BillingProvider
{
    // Determine whether the store is available and purchases can be made
    boolean canMakePurchase();
    
    // Allow the store to send purchase updates to the engine
    boolean enableUpdates();
    
    // Prevent the store from sending purchase state updates to the engine
    boolean disableUpdates();
    
    // Begin a purchase restoration operation to reenable previously purchased items
    boolean restorePurchases();
    
    // Create and send a new purchase request identified by the given id and using the given properties
    //boolean sendRequest(int purchaseId, String productId, Map<String, String> properties);
    boolean sendRequest(int purchaseId, String productId, String developerPayload);
    
    // Consume a purchased item -- only for Google API
    boolean consumePurchase(String productId);
    
    boolean requestProductDetails(String productId);
    String receiveProductDetails(String productId);
    
    
    boolean makePurchase(String productId, String quantity, String payload);
    
    // type is subscription or non-subscription
    boolean productSetType(String productId, String productType);
    
    boolean setPurchaseProperty(String productId, String propertyName, String propertyValue);
    
    String getPurchaseProperty(String productId, String propName);
    
    String getPurchaseList();
    
    // Notify the store that the item has been delivered
    boolean confirmDelivery(int purchaseId);
    
    // Register an observer for any updates to purchase requests
    void setPurchaseObserver(PurchaseObserver observer);
    
    void setActivity(Activity activity);

    // Initialize the store
    void initBilling();
    
    //Dispose any helper objects
    void onDestroy();
    
    // helper
    void onActivityResult (int requestCode, int resultCode, Intent data);
    
    // set LOG var to false when in production
    public class Log
    {
        static final boolean LOG = false;
        
        public static void i(String tag, String string)
        {
            if (LOG) android.util.Log.i(tag, string);
        }
        
        public static void d(String tag, String string)
        {
            if (LOG) android.util.Log.d(tag, string);
        }
        
        public static void v(String tag, String string)
        {
            if (LOG) android.util.Log.v(tag, string);
        }
        
        public static void e(String tag, String string)
        {
            if (LOG) android.util.Log.e(tag, string);
        }
    }
}
