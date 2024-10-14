class clsScreamer {
public:
    clsScreamer();  // Constructor declaration
    static clsScreamer* instance;  // Static instance pointer

    void setupScreamer();
    void handleScreamer();
    void initializeMQTT();
    void reconnectMQTT();

    static void mqttCallbackWrapper(char* topic, byte* message, unsigned int length);
    void mqttCallback(char* topic, byte* message, unsigned int length);
    void blinkLed();
};
