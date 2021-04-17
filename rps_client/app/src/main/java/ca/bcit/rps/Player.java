package ca.bcit.rps;

public class Player {
    private int[] uid;

    public Player(){}

    public Player(final int[] id){
        this.uid = id;
    }

    public int[] getUid(){
        return uid;
    }
}