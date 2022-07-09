package com.github.brianon99.dokidoki;

import android.Manifest;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothSocket;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.util.Log;
import android.widget.Toast;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Set;
import java.util.UUID;

import io.reactivex.rxjava3.core.Single;
import io.reactivex.rxjava3.schedulers.Schedulers;

public class Application extends android.app.Application {
    private final static String TAG = "DOKI_debug_Application";
    private BluetoothConnection bluetoothConnection;
    private final static UUID myUUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB");

    @Override
    public void onCreate() {
        super.onCreate();
        BluetoothManager bluetoothManager = getSystemService(BluetoothManager.class);
        BluetoothAdapter bluetoothAdapter = bluetoothManager.getAdapter();
        if (bluetoothAdapter == null) {
            return;
        }
        if (!bluetoothAdapter.isEnabled()) {
            Intent enableBtIntent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
            if (checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
                return;
            }
        } else {
            Set<BluetoothDevice> pairedDevices = bluetoothAdapter.getBondedDevices();

            if (pairedDevices.size() > 0) {
                // There are paired devices. Get the name and address of each paired device.
                for (BluetoothDevice device : pairedDevices) {
                    String deviceName = device.getName();
                    if (deviceName.equals("DOKIDOKI")) {
                        BluetoothSocket s;
                        try {
                            s = device.createRfcommSocketToServiceRecord(myUUID);// MAC address
                        } catch (IOException e) {
                            Toast.makeText(this, "Error on bluetooth connection", Toast.LENGTH_SHORT).show();
                            return;
                        }
                        bluetoothConnection = new BluetoothConnection(this, s);
                        bluetoothConnection.connect();
                        Log.d(TAG, "bluetooth connected");
                    }
                }
            }
        }
    }



    public BluetoothConnection getBluetoothConnection() {
        return bluetoothConnection;
    }

    public static class BluetoothConnection {
        private final BluetoothSocket mmSocket;
        private final InputStream mmInStream;
        private final OutputStream mmOutStream;

        public BluetoothConnection(Context ctx, BluetoothSocket socket) {
            mmSocket = socket;
            InputStream tmpIn = null;
            OutputStream tmpOut = null;

            try {
                tmpIn = socket.getInputStream();
            } catch (IOException e) {
                Log.e(TAG, "Error occurred when creating input stream", e);
                Toast.makeText(ctx, "Error occurred when creating input stream", Toast.LENGTH_SHORT).show();
            }
            try {
                tmpOut = socket.getOutputStream();
            } catch (IOException e) {
                Log.e(TAG, "Error occurred when creating output stream", e);
                Toast.makeText(ctx, "Error occurred when creating output stream", Toast.LENGTH_SHORT).show();
            }

            mmInStream = tmpIn;
            mmOutStream = tmpOut;
        }

        public void connect() {
            Single.fromCallable(() -> {
                        try {
                            Log.d(TAG, "connecting");
                            mmSocket.connect();
                            Log.d(TAG, "connected");
                        } catch (IOException connectException) {
                            Log.d(TAG, "Unable to connect");
                            // Unable to connect; close the socket and return.
                            try {
                                mmSocket.close();
                            } catch (IOException closeException) {
                                Log.e(TAG, "Could not close the client socket", closeException);
                            }
                            return false;
                        }
                        Log.d(TAG, "connected");
                        return true;
                    })
                    .subscribeOn(Schedulers.io())
                    .subscribe();
        }

        // Call this from the main activity to send data to the remote device.
        public void write(byte[] bytes) {
            Log.d(TAG, mmSocket.isConnected() ? "connected..." : "not connected");
            if (!mmSocket.isConnected()) {
                Log.d(TAG, "not connected");
            }
            try {
                mmOutStream.write(bytes);
            } catch (IOException e) {
                Log.e(TAG, "Error occurred when sending data", e);
            }
        }

        // Call this method from the main activity to shut down the connection.
        public void cancel() {
            try {
                mmSocket.close();
            } catch (IOException e) {
                Log.e(TAG, "Could not close the connect socket", e);
            }
        }
    }
}