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
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.RadioGroup;
import android.widget.TextView;
import android.widget.ToggleButton;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.Arrays;

public class MainActivity extends Activity implements SerialListener {
    private final byte MOV = 1;
    private final byte ALIGN = 2;
    private final byte GOTO = 3;
    private final byte ZERO = 4;
    private final byte ANGLE_DEC = 1;
    private final byte ANGLE_ASC = 2;
    private final byte GET_THETA_PHI = 5;
    private SerialSocket serialSocket;
    private TextView connectionStatusView;
    private Connected connectionStatus = Connected.False;
    private String KEY_STATE = "KEY_STATE";
    private ToggleButton speedToogle;
    private Listener cmdListener;
    private byte[] star1timePhiTheta;
    private byte[] star2timePhiTheta;
    private int selectedCommand;
    long epoch = 0;

    interface Listener {
        void onSerialRead (byte[] data);
    }

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
        if (cmdListener != null) {
            cmdListener.onSerialRead(data);
        }
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

    private void writeCmd(byte[] cmd) {
        try {
            serialSocket.write(cmd);
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private byte[] textToAngle(EditText v, byte angleType) {
        int sign;
        String s = v.getText().toString();
        if (s.startsWith("-")) {
            sign = -1;
            s = s.substring(1);
        } else {
            sign = 1;
        }

        String[] ss = s.split("\\+");
        if (ss.length < 2) {
            v.setError("invalid");
            throw new NumberFormatException();
        }
        double angle, c, b, a = 0;
        try {
            c = Double.parseDouble(ss[0]);
            b = Double.parseDouble(ss[1]);
            if (ss.length == 3)
                a = Double.parseDouble(ss[2]);
            if (angleType == ANGLE_DEC) {
                if (c < 0 || c > 90 || b < 0 || b > 60 || a < 0 || a > 60)
                    throw new NumberFormatException();
            }
            if (angleType == ANGLE_ASC) {
                if (c < 0 || c > 24 || b < 0 || b > 60 || a < 0 || a > 60)
                    throw new NumberFormatException();
            }
            angle = sign * (c + b / 60.0 + a / 3600.0);
            if (angleType == ANGLE_ASC)
                angle = angle / 24 * 360;
            Log.e("DOKILOG", String.valueOf(angle));
        } catch (NumberFormatException e) {
            v.setError("invalid");
            throw e;
        }
        int k = (int) Math.round(angle / 360 * 65535);
        return Arrays.copyOfRange(ByteBuffer.allocate(4).putInt(k).array(), 2, 4);
    }

    @SuppressLint("ClickableViewAccessibility")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        connectionStatusView = findViewById(R.id.connection_status);
        speedToogle = findViewById(R.id.speed);
        Button enter = findViewById(R.id.cmd_enter);
        RadioGroup cmdGroup = findViewById(R.id.cmd_group);
        View cmdStar1GetOrientation = findViewById(R.id.star1_get_orientation);
        View cmdStar2GetOrientation = findViewById(R.id.star2_get_orientation);

        if (savedInstanceState != null) {
            this.updateStatus((Connected) savedInstanceState.getSerializable(KEY_STATE));
        }

        int[][] cmdOptionMap = {
            {R.id.cmd_zero, -1},
            {R.id.cmd_align, R.id.cmd_align_options},
            {R.id.cmd_goto, R.id.cmd_goto_options},
        };

        cmdGroup.setOnCheckedChangeListener((RadioGroup group, int checkedId) -> {
            selectedCommand = checkedId;
            for (int[] optionMap: cmdOptionMap) {
                if (optionMap[1] == -1)
                    continue;
                int id = optionMap[1];
                View v = findViewById(id);
                v.setVisibility(checkedId == optionMap[0] ? View.VISIBLE : View.INVISIBLE);
            }
        });

        initBlueToothConnection();
        setBtnDirectionListener();

        View.OnClickListener clickListener = (View view) -> {
            ByteBuffer b = ByteBuffer.allocate(1);
            b.put(GET_THETA_PHI);
            try {
                serialSocket.write(b.array());
            } catch (IOException e) {
                e.printStackTrace();
                return;
            }
            this.cmdListener = (byte[] data) -> {
                if (data[0] != GET_THETA_PHI)
                    return;
                if (view.equals(cmdStar1GetOrientation)) {
                    star1timePhiTheta = Arrays.copyOfRange(data, 1, 9);
                } else if (view.equals(cmdStar2GetOrientation)) {
                    star2timePhiTheta = Arrays.copyOfRange(data, 1, 9);
                }
                cmdListener = null;
                ByteBuffer replyByteBuffer = ByteBuffer.wrap(data);
                ((TextView) view).setText(String.format("%d %d", (int)replyByteBuffer.getChar(5), (int)replyByteBuffer.getChar(7)));

                if (epoch == 0) {
                    long time = System.currentTimeMillis() / 1000;
                    int remoteTime = ByteBuffer.wrap(data, 1, 4).getInt();
                    epoch = time - remoteTime;
                }
            };
        };

        cmdStar1GetOrientation.setOnClickListener(clickListener);
        cmdStar2GetOrientation.setOnClickListener(clickListener);

        enter.setOnClickListener((View view) -> {
            switch (selectedCommand) {
                case R.id.cmd_zero:
                    writeCmd(new byte[] {ZERO});
                    break;
                case R.id.cmd_goto:
                    EditText v;

                    ByteBuffer b = ByteBuffer.allocate(9);
                    b.put(GOTO);
                    b.putInt((int)(System.currentTimeMillis() / 1000 - epoch));
                    try {
                        v = findViewById(R.id.cmd_goto_options_asc);
                        b.put(textToAngle(v, ANGLE_ASC));
                        v = findViewById(R.id.cmd_goto_options_dec);
                        b.put(textToAngle(v, ANGLE_DEC));
                    } catch (NumberFormatException e) {
                        return;
                    }
                    writeCmd(b.array());
                    break;
                case R.id.cmd_align:
                    b = ByteBuffer.allocate(25);
                    if (star1timePhiTheta == null || star2timePhiTheta == null)
                        return;

                    try {
                        b.put(ALIGN);
                        b.put(star1timePhiTheta);
                        v = findViewById(R.id.star1asc);
                        b.put(textToAngle(v, ANGLE_ASC));
                        v = findViewById(R.id.star1dec);
                        b.put(textToAngle(v, ANGLE_DEC));
                        b.put(star2timePhiTheta);
                        v = findViewById(R.id.star2asc);
                        b.put(textToAngle(v, ANGLE_ASC));
                        v = findViewById(R.id.star2dec);
                        b.put(textToAngle(v, ANGLE_DEC));
                    } catch (NumberFormatException e) {
                        return;
                    }
                    writeCmd(b.array());
                    break;
            }
        });
    }
}