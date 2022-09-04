package com.github.brianon99.dokidoki;

interface SerialListener {
    enum Connected { False, Connectting, True }
    void updateStatus         (Connected c);
    void onSerialConnectError (Exception e);
    void onSerialRead         (byte[] data);
    void onSerialIoError      (Exception e);
}
