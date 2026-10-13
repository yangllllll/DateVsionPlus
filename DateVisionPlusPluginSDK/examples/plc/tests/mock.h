#ifndef PLC_TEST_MOCK_H
#define PLC_TEST_MOCK_H

struct MockStats {
    int creates;
    int destroys;
    int connects;
    int disconnects;
    int statusQueries;
    int writes;
    int reads;
    int amount;
    int wordLen;
    int rack;
    int slot;
    unsigned char value;
};

#endif
