package ca.bcit.rps;

import java.io.IOException;
import java.io.InputStream;
import java.io.PrintStream;
import java.net.Socket;
import java.net.UnknownHostException;
import java.nio.ByteBuffer;

import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;
import android.os.Looper;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

public class MainActivity extends AppCompatActivity {

    Button btnRock, btnPaper, btnScissors, btnStart;
    TextView p1Choice, p2Choice, result;

    int myChoice = 0;

    // As per protocol
    private static final int ROCK = 1;
    private static final int PAPER = 2;
    private static final int SCISSORS = 3;

    private static final int WIN = 1;
    private static final int LOSS = 2;
    private static final int TIE = 3;

    private int oppChoice = 0;
    private int res;
    private Socket socket;
    private PrintStream toServ;
    private InputStream fromServ;
    private Player player;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        btnRock = findViewById(R.id.btnRock);
        btnPaper = findViewById(R.id.btnPaper);
        btnScissors = findViewById(R.id.btnScissors);
        btnStart = findViewById(R.id.start);

        p1Choice = findViewById(R.id.p1Choice);
        p2Choice = findViewById(R.id.p2Choice);
        result = findViewById(R.id.result);

        // Rock button
        btnRock.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick (View v){
                // TODO: Replace with image?
                p1Choice.setText("ROCK");
                myChoice = 1;

                playerChoice(ROCK, "ROCK");
            }
        });

        // Paper button
        btnPaper.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick (View v){
                // TODO: Replace with image?
                p1Choice.setText("PAPER");
                myChoice = 2;

                playerChoice(PAPER, "PAPER");
            }
        });

        // Scissors button
        btnScissors.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick (View v){
                // TODO: Replace with image?
                p1Choice.setText("SCISSORS");
                myChoice = 3;

                playerChoice(SCISSORS, "SCISSORS");
            }
        });

        btnStart.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick (View v){
                btnStart.setEnabled(false);

                Runnable runnable = new Runnable() {
                    @Override
                    public void run() {
                        Looper.prepare();
                        connect();
                        init();
                        initServerResp();
                    }
                };

                Thread thread = new Thread(runnable);
                thread.start();
            }
        });
    }

    private void connect() {
        Log.d("CONNECT", "Attempting to connect to " + Environment.HOST
                + ":" + Environment.PORT);
        try{
            socket = new Socket(Environment.HOST, Environment.PORT);
            toServ = new PrintStream(socket.getOutputStream());
            fromServ = socket.getInputStream();

            Log.d("CONNECT", "Successfully connected");
        } catch (UnknownHostException e) {
            e.printStackTrace();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private void init(){
        final int[] uid = new int[]{0, 0, 0, 0};  // Initially 0 until server response
        final int CONFIRMATION = DataTypes.RequestType.CONFIRMATION.getVal();
        final int CONTEXT = 1;
        final int length = 2;
        final int ver = 1;
        final int gId = 2;
//        final int gId = 1;

        final int[] payload = {ver, gId};

        ReqPacket req = new ReqPacket(uid, CONFIRMATION, CONTEXT, length, payload);
        final byte[] packet = req.toByteArray();

        Log.d("INIT", req.toString());
        Log.d("INIT", packet.toString());

        try {
            toServ.write(packet);
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private void initServerResp() {
        byte[] packet = new byte[7];
        try {
            int bytesRead = fromServ.read(packet);
            Log.d("PACKET", String.valueOf(packet));

            if (bytesRead == 0) {
                Log.e("ERROR", "No bytes received from server");
            }

            ByteBuffer bb = ByteBuffer.wrap(packet);

            int status = bb.get(0);
            int type = bb.get(1);
            int length = bb.get(2);
            int[] uid = new int[]{bb.get(3), bb.get(4), bb.get(5), bb.get(6)};

            // bb.get(2) is 2nd index = length
//            int[] recvPayload = new int[length];
//            recvPayload[0] = uid;  // uid

            Packet recvPacket = new Packet(status, type, length, uid);

            System.out.println("PAYLOAD: " + recvPacket.getPayload());
            player = new Player(recvPacket.getPayload());

            Log.d("PLAYER", "Player ID: + " + player.getUid());
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private void sendMove() {
        final int GAME_ACTION = DataTypes.RequestType.GAME_ACTION.getVal();
        final int CONTEXT = DataTypes.RequestContext.MAKE_MOVE.getVal();
        final int LENGTH = 1;

        final int[] payload = { myChoice };

        Log.d("SEND CHOICE", String.valueOf(myChoice));

        ReqPacket move = new ReqPacket(player.getUid(), GAME_ACTION, CONTEXT, LENGTH, payload);

        Log.d("SEND MOVE", move.toString());
        final byte[] movePacket = move.toByteArray();

        try {
            toServ.write(movePacket);
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    public static String byteArrayToString(byte[] arr) {
        String finalString = "";

        for (byte curByte : arr) {
            finalString += (int) curByte;
        }

        return finalString;
    }

    private void initAcknowledge() {
        byte[] ackPacket = new byte[3];

        try {
            int bytesRead = fromServ.read(ackPacket);

            Log.d("ACKNOWLEDGEMENT RESPONSE", byteArrayToString(ackPacket));

            if (bytesRead == 0) {
                Log.e("ERROR", "No bytes received from server");
            }
        } catch (IOException e) {
            e.printStackTrace();
        }

        if(ackPacket[0] == 20) {
            Log.d("ACKNOWLEDGEMENT RESPONSE", "Success");
        } else {
            Log.e("ERROR", "Server error");
        }
    }

    private void moveAcknowledge() {
        byte[] ackPacket = new byte[3];

        try {
            int bytesRead = fromServ.read(ackPacket);

            Log.d("ACKNOWLEDGEMENT RESPONSE", byteArrayToString(ackPacket));

            if (bytesRead == 0) {
                Log.e("ERROR", "No bytes received from server");
            }
        } catch (IOException e) {
            e.printStackTrace();
        }

        if(ackPacket[0] == 10) {
            Log.d("ACKNOWLEDGEMENT RESPONSE", "Success");
        } else {
            Log.e("ERROR", "Server error");
        }
    }

    private void process(){
        byte[] endgamePacket = new byte[5];

        try {
            int bytesRead = fromServ.read(endgamePacket);
            Log.d("ENDGAME", "from Server " + byteArrayToString(endgamePacket));

            if (bytesRead == 0) {
                Log.e("ERROR", "No bytes received from server");
            }
        } catch (IOException e) {
            e.printStackTrace();
        }

        int[] payload = new int[endgamePacket[2]];

        int i = 0;
        // build the payload - everything after length
        for (int j = 3; j < 3 + endgamePacket[2]; j++){
            payload[i] = endgamePacket[j];
            i++;
        }

        Log.d("ENDGAME", "Player " + player.getUid());
        Log.d("ENDGAME", "Result: " + payload[0]);

        oppChoice = payload[0];
        Log.d("ENDGAME", "Opponent Choice: " + oppChoice);

        switch(oppChoice) {
            case 1:
                res = WIN;
                runOnUiThread(new Runnable() {

                    @Override
                    public void run() {

                        // Stuff that updates the UI
                        result.setText("YOU WON");
                    }
                });

                break;
            case 2:
                res = LOSS;

                runOnUiThread(new Runnable() {

                    @Override
                    public void run() {

                        // Stuff that updates the UI
                        result.setText("YOU LOST");
                    }
                });

                break;
            case 3:
                res = TIE;

                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        // Stuff that updates the UI
                        result.setText("DRAW");
                    }
                });
                break;
        }
    }

    private void playerChoice(final int choiceInt, final String choiceStr){
        Log.d("CHOICE", choiceStr);

        btnRock.setEnabled(false);
        btnPaper.setEnabled(false);
        btnScissors.setEnabled(false);

        Thread thread1 = new Thread(new Runnable() {
            @Override
            public void run() {
                Looper.prepare();
//                initAcknowledge();
                sendMove();
                initAcknowledge();
                moveAcknowledge();
                process();
            }
        });
        thread1.start();
    }
}