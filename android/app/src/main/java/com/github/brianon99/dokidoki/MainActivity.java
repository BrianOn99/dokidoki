package com.github.brianon99.dokidoki;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.widget.ToggleButton;

import java.io.IOException;
import java.nio.ByteBuffer;

public class MainActivity extends Activity implements SerialListener {
    private final byte MOV = 1;
    private final byte ALIGN = 2;
    private final byte GOTO = 3;
    private SerialSocket serialSocket;
    private TextView connectionStatusView;
    private Connected connectionStatus = Connected.False;
    private String KEY_STATE = "KEY_STATE";
    private ToggleButton speedToogle;
    private Button cmdEnter;
    private Button cmdGoto;

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putSerializable(KEY_STATE, connectionStatus);
    }

    @Override
    public void onSerialConnectError(Exception e) {
        runOnUiThread(() ->{
            new AlertDialog.Builder(this)
                .setTitle("Connection Error")
                .setMessage(e.getMessage())
                .setPositiveButton("OK", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialogInterface, int i) {
                        dialogInterface.cancel();
                    }
                })
                .show();
        });
    }

    @Override
    public void onSerialRead(byte[] data) {
    }

    @Override
    public void onSerialIoError(Exception e) {
    }

    @Override
    public void updateStatus(Connected c) {
        this.connectionStatus = c;
        switch (c) {
            case False:
                connectionStatusView.setText(R.string.connection_disconnected);
                break;
            case True:
                connectionStatusView.setText(R.string.connection_connected);
                break;
            case Connectting:
                connectionStatusView.setText(R.string.connection_connecting);
                break;
        }
    }

    private static final int[][] btnDirections = {
            {R.id.btn1, -1, 1},
            {R.id.btn2, 0, 1},
            {R.id.btn3, 1, 1},
            {R.id.btn4, -1, 0},
            {R.id.btn6, 1, 0},
            {R.id.btn7, -1, -1},
            {R.id.btn8, 0, -1},
            {R.id.btn9, 1, -1},
    };

    private void initBlueToothConnection() {
        Boolean flag = false;
        if (checkSelfPermission(Manifest.permission.BLUETOOTH) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(
                    new String[]{Manifest.permission.BLUETOOTH},
                    2);
            flag = true;
        }
        if (checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                requestPermissions(
                        new String[]{Manifest.permission.BLUETOOTH_CONNECT},
                        1
                );
            }
            flag = true;
        }
        if (flag)
            return;

        Application app = ((Application) getApplication());
        if (app.initializeBluetooth() == null) {
            new AlertDialog.Builder(this)
                    .setTitle("Initialization Error")
                    .setMessage("Cannot find remote device")
                    .show();
        }
        app.setSerialListener(this);
        this.serialSocket = app.serialSocket;
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == 1 && resultCode == RESULT_OK) {
            initBlueToothConnection();
        }
    }

    private void setBtnDirectionListener() {
        for (int[] btnDirection: btnDirections) {
            View v = findViewById(btnDirection[0]);
            v.setOnTouchListener((View _v, MotionEvent event) -> {
                ByteBuffer b = ByteBuffer.allocate(9);
                b.put(MOV);
                int speed = Integer.MAX_VALUE;
                if (speedToogle.isChecked()) {
                    speed = speed / 3;
                }
                switch (event.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        if (btnDirection[1] == 1)
                            b.putInt(speed);
                        else if (btnDirection[1] == -1)
                            b.putInt(-speed);
                        else
                            b.putInt(0);

                        if (btnDirection[2] == 1)
                            b.putInt(speed);
                        else if (btnDirection[2] == -1)
                            b.putInt(-speed);
                        else
                            b.putInt(0);

                        try {
                            serialSocket.write(b.array());
                        } catch (IOException e) {
                            e.printStackTrace();
                        }
                        break;

                    case MotionEvent.ACTION_UP:
                        b.putInt(0);
                        b.putInt(0);
                        _v.performClick();
                        try {
                            serialSocket.write(b.array());
                        } catch (IOException e) {
                            e.printStackTrace();
                        }
                        break;
                    default:
                        break;
                }
                return true;
            });
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        connectionStatusView = findViewById(R.id.connection_status);
        speedToogle = findViewById(R.id.speed);
        cmdEnter = findViewById(R.id.cmd_enter);
        cmdGoto = findViewById(R.id.cmd_goto);

        if (savedInstanceState != null) {
            this.updateStatus((Connected) savedInstanceState.getSerializable(KEY_STATE));
        }

        initBlueToothConnection();
        setBtnDirectionListener();
        cmdEnter.setOnClickListener((View view) -> {
            ByteBuffer b = ByteBuffer.allocate(25);
            b.put(ALIGN);
            b.putInt(2352);
            b.putShort((short)7264);
            b.putShort((short)7264);
            b.putShort((short)14412);
            b.putShort((short)8374);
            b.putInt(2418);
            b.putShort((short)17221);
            b.putShort((short)6590);
            b.putShort((short)6910);
            b.putShort((short)16250);
            try {
                serialSocket.write(b.array());
            } catch (IOException e) {
                e.printStackTrace();
            }
        });

        cmdGoto.setOnClickListener((View view) -> {
            ByteBuffer b = ByteBuffer.allocate(9);
            b.put(GOTO);
            b.putInt(3720);
            b.putShort((short)13021);
            b.putShort((short)3107);
            try {
                serialSocket.write(b.array());
            } catch (IOException e) {
                e.printStackTrace();
            }
        });
    }
}