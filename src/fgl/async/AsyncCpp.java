package com.lufinkey.libasynccpp;

public class AsyncCpp {
	static {
		loadLibraries();
	}

	public static void loadLibraries() {
		com.lufinkey.libdatacpp.DataCpp.loadLibraries();
		System.loadLibrary("AsyncCpp");
	}
}
