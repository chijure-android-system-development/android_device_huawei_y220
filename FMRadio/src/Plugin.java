package com.mediatek.pluginmanager;

public class Plugin<T> {
    public static class ObjectCreationException extends Exception {
    }

    public T createObject() throws ObjectCreationException { throw new ObjectCreationException(); }
}
