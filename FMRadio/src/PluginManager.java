package com.mediatek.pluginmanager;

import android.content.Context;
import android.content.pm.Signature;

/* No operator plugins on CM7: create() returns null, callers use defaults. */
public class PluginManager<T> {
    public static <T> PluginManager<T> create(Context c, String action, Signature[] sigs) { return null; }
    public int getPluginCount() { return 0; }
    public Plugin<T> getPlugin(int index) { return null; }
}
