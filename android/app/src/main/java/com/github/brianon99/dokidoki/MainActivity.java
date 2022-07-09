package com.github.brianon99.dokidoki;

import android.Manifest;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothSocket;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;
import android.widget.Toast;

import com.github.brianon99.dokidoki.view.JoystickView;

import java.nio.ByteBuffer;

public class MainActivity extends Activity {
    private final static int REQUEST_ENABLE_BT = 1;
    private final static String TAG = "DOKI_debug_MainActivity";
    private final byte MOV = 1;
    private Application.BluetoothConnection bluetoothConnection;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        JoystickView joystick = findViewById(R.id.joystick);
        bluetoothConnection = ((Application) getApplication()).getBluetoothConnection();

        joystick.setOnMoveListener((double x, double y) -> {
            ByteBuffer b = ByteBuffer.allocate(9);
            b.put(MOV);
            b.putInt((int) Math.round(x * (1L<<31)));
            b.putInt((int) Math.round(y * (1L<<31)));
            bluetoothConnection.write(b.array());
        }, 500);
    }
}