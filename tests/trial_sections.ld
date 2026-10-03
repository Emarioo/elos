
SECTIONS {
    

    .trial_asserts ALIGN(16) : {
        __trial_asserts_start = .;
        *(.trial_asserts)
        __trial_asserts_end = .;
    }


}
INSERT AFTER .data;
