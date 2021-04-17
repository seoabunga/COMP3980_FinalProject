package ca.bcit.rps;

import android.app.Activity;
import android.os.Bundle;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Arrays;

public class ReqPacket extends Packet {
    private int[] uid;

    // default constructor
    public ReqPacket(){}

    public ReqPacket(final int[] uid, final int type, final int context,
                     final int length, final int[] payload){
        super(type, context, length, payload);
        this.uid = uid;
    }

    public byte[] toByteArray(){
        ByteBuffer bb = ByteBuffer.allocate(9);
        bb.order(ByteOrder.LITTLE_ENDIAN);

        bb.put((byte)uid[0]);
        bb.put((byte)uid[1]);
        bb.put((byte)uid[2]);
        bb.put((byte)uid[3]);
        bb.put((byte) type);
        bb.put((byte) context);
        bb.put((byte) length);

        for (int payloadInfo : payload){
            bb.put((byte) payloadInfo);
        }

        return bb.array();
    }

    @Override
    public String toString() {
        return "RequestPacket {" +
                "uid=" + uid +
                ", messageType=" + type +
                ", messageContext=" + context +
                ", payloadLength=" + length +
                ", payloadInfo=" + Arrays.toString(payload) +
                '}';
    }
}