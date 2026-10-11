/*
 * Stock libutils exports these. libMali.so resolves them from libutils
 * and only uses them for atrace. Leaving the tag mask at 0 keeps it off.
 */
#include <stdint.h>

namespace android {

class Tracer {
public:
    static void init();
    static int sTraceFD;
    static volatile int32_t sIsReady;
    static uint64_t sEnabledTags;
};

int Tracer::sTraceFD = -1;
volatile int32_t Tracer::sIsReady = 0;
uint64_t Tracer::sEnabledTags = 0;

void Tracer::init()
{
    sIsReady = 1;
}

}
