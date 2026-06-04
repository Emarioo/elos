
#define TRIAL_IMPL

#include "trial.h"


void test_sample() {

    trial_start();

    trial_assert("sample", 1 == 1);

    trial_end();

}

