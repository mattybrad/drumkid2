class Sample {
    public:
        float speed = 1.0;
        float position = 0.0;
        int length = 0;
        int value = 0;
        int* sampleData;
        void update() {
            int intPosition = (int) position; // naiive function, no lerping, temporary
            if(intPosition < length) {
                value = sampleData[intPosition];
            } else {
                value = 0;
            }
            position += speed;
        }
};