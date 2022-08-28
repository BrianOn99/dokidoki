package com.github.brianon99.dokidoki;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Toast;
import android.widget.ToggleButton;

import java.nio.ByteBuffer;

public class MainActivity extends Activity {
    private final static int REQUEST_ENABLE_BT = 1;
    private final static String TAG = "DOKI_debug_MainActivity";
    private final byte MOV = 1;
    private Application.BluetoothConnection bluetoothConnection;

    private static int[][] btnDirections = {
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
        bluetoothConnection = ((Application) getApplication()).getBluetoothConnection();
        ToggleButton tb = findViewById(R.id.speed);

        for (int[] btnDirection: btnDirections) {
            View v = findViewById(btnDirection[0]);
            v.setOnTouchListener((View _v, MotionEvent event) -> {
                ByteBuffer b = ByteBuffer.allocate(9);
                b.put(MOV);
                int speed = Integer.MAX_VALUE;
                if (tb.isChecked()) {
                    speed = speed / 8;
                }
                switch (event.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        Toast.makeText(this, "down", Toast.LENGTH_SHORT).show();
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

                        bluetoothConnection.write(b.array());
                        break;

                    case MotionEvent.ACTION_UP:
                        b.putInt(0);
                        b.putInt(0);
                        _v.performClick();
                        bluetoothConnection.write(b.array());
                        break;
                    default:
                        break;
                }
                return true;
            });
        }
    }
}