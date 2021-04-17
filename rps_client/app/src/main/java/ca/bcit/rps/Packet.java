package ca.bcit.rps;

import android.app.Activity;

import java.util.Arrays;

public class Packet extends Activity {
    protected int type;
    protected int context;
    protected int length;
    protected int[] payload;

    // default constructor
    public Packet(){}

    public Packet(final int type, final int context, final int length,
                  final int[] payload){
        this.type = type;
        this.context = context;
        this.length = length;
        this.payload = payload;
    }

    public int[] getPayload(){
        return payload;
    }

    @Override
    public String toString() {
        return "Packet {" +
                "messageType=" + type +
                ", messageContext=" + context +
                ", payloadLength=" + length +
                ", payloadInfo=" + Arrays.toString(payload) +
                '}';
    }
}