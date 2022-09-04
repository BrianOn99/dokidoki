package com.github.brianon99.dokidoki;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.View;
import android.widget.TextView;
import android.widget.ToggleButton;

import java.io.IOException;
import java.nio.ByteBuffer;

public class MainActivity extends Activity implements SerialListener {
    private final byte MOV = 1;
    private SerialSocket serialSocket;
    private TextView connectionStatusView;

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

    @SuppressLint("ClickableViewAccessibility")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        connectionStatusView = findViewById(R.id.connection_status);
        ((Application) getApplication()).setSerialListener(this);
        this.serialSocket = ((Application) getApplication()).serialSocket;
        ToggleButton tb = findViewById(R.id.speed);

        for (int[] btnDirection: btnDirections) {
            View v = findViewById(btnDirection[0]);
            v.setOnTouchListener((View _v, MotionEvent event) -> {
                ByteBuffer b = ByteBuffer.allocate(9);
                b.put(MOV);
                int speed = Integer.MAX_VALUE;
                if (tb.isChecked()) {
                    speed = speed / 5;
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
}