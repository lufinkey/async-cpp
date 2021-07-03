package com.lufinkey;

public class AsyncCpp {
	static {
		loadLibraries();
	}

	public static void loadLibraries() {
		com.lufinkey.DataCpp.loadLibraries();
		System.loadLibrary("AsyncCpp");
	}
}
