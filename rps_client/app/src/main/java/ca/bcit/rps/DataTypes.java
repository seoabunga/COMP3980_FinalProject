package ca.bcit.rps;

public class DataTypes {
    public enum RequestType {
        CONFIRMATION(1), INFORMATION(2), META_ACTION(3), GAME_ACTION(4);

        private final int value;

        RequestType(int value) {
            this.value = value;
        }

        public byte getVal() {
            return (byte) value;
        }
    };

    public enum RequestContext {
        RULESET(1), MAKE_MOVE(1), QUIT(1);

        private final int value;

        RequestContext(int value) {
            this.value = value;
        }

        public byte getVal() {
            return (byte) value;
        }
    };

    public enum ResponseType {
        SUCCESS(10), UPDATE(20), INVALID_REQUEST(30), INVALID_UID(31), INVALID_TYPE(32), INVALID_CONTEXT(33),
        INVALID_PAYLOAD(34), SERVER_ERROR(40), INVALID_ACTION(50), ACTION_OUT_OF_TURN(51);

        private final int value;

        ResponseType(int value) {
            this.value = value;
        }

        public byte getVal() {
            return (byte) value;
        }
    };
}