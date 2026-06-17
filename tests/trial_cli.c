/*

    Compiled, executes and collects results from tests.

*/


#include "trial.h"






void main(int argc, const char* argv) {

    // no options -> run all tests

    test_sample();

}






void test_sample() {

    compile_test();

    run_test();

    collect_result();    

}

