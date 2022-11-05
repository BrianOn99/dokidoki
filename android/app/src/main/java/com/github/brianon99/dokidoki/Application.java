package com.github.brianon99.dokidoki;

import android.Manifest;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.content.pm.PackageManager;

import java.util.Set;

public class Application extends android.app.Application {
    private final static String TAG = "DOKI_debug_Application";
    public SerialSocket serialSocket;
    private BluetoothDevice remoteDevice;

    public BluetoothDevice initializeBluetooth() {
        if (this.remoteDevice != null) {
            return this.remoteDevice;
        }

        BluetoothManager bluetoothManager = getSystemService(BluetoothManager.class);
        BluetoothAdapter bluetoothAdapter = bluetoothManager.getAdapter();
        if (bluetoothAdapter == null) {
            return null;
        }
        if (!bluetoothAdapter.isEnabled()) {
            if (checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
                return null;
            }
        } else {
            Set<BluetoothDevice> pairedDevices = bluetoothAdapter.getBondedDevices();

            if (pairedDevices.size() > 0) {
                for (BluetoothDevice device : pairedDevices) {
                    String deviceName = device.getName();
                    if (deviceName.equals("DOKIDOKI")) {
                        this.remoteDevice = device;
                        break;
                    }
                }
            }
        }

        return this.remoteDevice;
    }

    public void setSerialListener(SerialListener listener) {
        try {
            if (this.serialSocket == null) {
                listener.updateStatus(SerialListener.Connected.Connectting);
                this.serialSocket = new SerialSocket(getApplicationContext(), remoteDevice);
                this.serialSocket.setListener(listener);
                this.serialSocket.connect();
            } else {
                this.serialSocket.setListener(listener);
            }
        } catch (Exception e) {
            listener.onSerialConnectError(e);
        }
    }
}