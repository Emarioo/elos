
int get() {
    return 5;
}

int __attribute__ ((section (".entry"))) start(int x) {
    return x + get();
}
