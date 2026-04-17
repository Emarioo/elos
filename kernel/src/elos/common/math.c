
static unsigned long xorshift_state = 2463534242;

unsigned int xorshift32(void) {
    unsigned x = xorshift_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return xorshift_state = x;
}

void srandx(unsigned long s) {
    xorshift_state = s ? s : 2463534242;
}
