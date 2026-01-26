#pragma once

#include <cstdint>

class Beat {
    public:
        void init();
        bool tempPattern[16] = {true, false, false, false,
                                true, false, false, false,
                                true, false, false, false,
                                true, true, true, false};

    private:
};