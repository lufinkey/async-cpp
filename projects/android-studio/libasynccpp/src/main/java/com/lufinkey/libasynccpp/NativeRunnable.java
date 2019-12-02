package com.lufinkey.libasynccpp;

public class NativeRunnable implements Runnable {
	private long func;

	NativeRunnable(long func) {
		this.func = func;
	}

	@Override
	public void run() {
		callNativeFunction(func);
	}

	private native static void callNativeFunction(long func);
	private native static void destroyNativeFunction(long func);
}
