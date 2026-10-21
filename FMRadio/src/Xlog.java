package com.mediatek.xlog;

import android.util.Log;

/* MediaTek Xlog is a framework class on stock; CM7 only has android.util.Log. */
public final class Xlog {
    public static int v(String t, String m) { return Log.v(t, m); }
    public static int v(String t, String m, Throwable e) { return Log.v(t, m, e); }
    public static int d(String t, String m) { return Log.d(t, m); }
    public static int d(String t, String m, Throwable e) { return Log.d(t, m, e); }
    public static int i(String t, String m) { return Log.i(t, m); }
    public static int i(String t, String m, Throwable e) { return Log.i(t, m, e); }
    public static int w(String t, String m) { return Log.w(t, m); }
    public static int w(String t, String m, Throwable e) { return Log.w(t, m, e); }
    public static int e(String t, String m) { return Log.e(t, m); }
    public static int e(String t, String m, Throwable e) { return Log.e(t, m, e); }
}
