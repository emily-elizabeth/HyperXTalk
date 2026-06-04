package com.runrev.android;

import android.content.*;
import android.content.pm.*;

public class Utils
{
    public static int[] splitIntegerList(String p_list)
    {
        try
        {
            String[] t_items = p_list.split(",");
            int[] t_list = new int[t_items.length];
            
            for (int i = 0; i < t_items.length; i++)
            {
                t_list[i] = Integer.parseInt(t_items[i].trim());
            }
            
            return t_list;
        }
        catch (Exception e)
        {
            return null;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    
    public static String getLabel(Context context)
    {
        PackageManager pm = context.getPackageManager();
        ApplicationInfo ai = context.getApplicationInfo();
        ai.loadLabel(pm);
        return pm.getApplicationLabel(ai).toString();
    }
    
}
