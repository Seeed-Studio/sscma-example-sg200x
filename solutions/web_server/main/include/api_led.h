#ifndef API_LED_H
#define API_LED_H

#include "api_base.h"

class api_led : public api_base {
public:
    api_led()
        : api_base("led")
    {
        printf("%s,%d\n", __func__, __LINE__);
    }

    ~api_led()
    {
        printf("%s,%d\n", __func__, __LINE__);
    }
};

#endif // API_LED_H
