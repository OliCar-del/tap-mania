/*
 * hardcoded_sequences.h
 *
 *  Created on: Oct 20, 2025
 *      Author: olica
 */

#ifndef INC_HARDCODED_SEQUENCES_H_
#define INC_HARDCODED_SEQUENCES_H_

// Test sequences - GRBY order
// Example sequences for testing different patterns
extern const uint8_t test_sequence_simple[][4] = {
    { 1, 0, 0, 0 }, // Beat 1: Green press
    { 0, 1, 0, 0 }, // Beat 2: Red press
    { 0, 0, 1, 0 }, // Beat 3: Blue press
    { 0, 0, 0, 1 }, // Beat 4: Yellow press
    { 1, 1, 0, 0 }, // Beat 5: Green + Red press
    { 0, 0, 1, 1 }, // Beat 6: Blue + Yellow press
    { 1, 1, 1, 1 }, // Beat 7: All buttons press
    { 0, 0, 0, 0 }, // Beat 8: Rest
};

extern const uint8_t test_sequence_holds[][4] = {
    { 0, 0, 0, 0 }, // Beat 1: Rest
    { 2, 0, 0, 0 }, // Beat 2: Green hold start
    { 2, 0, 0, 0 }, // Beat 3: Green hold continue
    { 2, 0, 0, 0 }, // Beat 4: Green hold end
    { 0, 2, 2, 0 }, // Beat 5: Red + Blue hold start
    { 0, 2, 2, 0 }, // Beat 6: Red + Blue hold continue
    { 1, 0, 0, 1 }, // Beat 7: Green + Yellow press
    { 0, 0, 0, 0 }, // Beat 8: Rest
};

extern const uint8_t test_sequence_complex[][4] = {
    { 0, 0, 0, 0 }, // Beat 1: Rest
    { 1, 0, 1, 0 }, // Beat 2: Green + Blue press
    { 0, 0, 0, 0 }, // Beat 3: Rest
    { 0, 1, 0, 1 }, // Beat 4: Red + Yellow press
    { 2, 0, 0, 0 }, // Beat 5: Green hold start
    { 2, 1, 0, 0 }, // Beat 6: Green hold + Red press
    { 2, 0, 1, 0 }, // Beat 7: Green hold + Blue press
    { 0, 0, 2, 2 }, // Beat 8: Blue + Yellow hold start (Green released)
    { 0, 1, 2, 2 }, // Beat 9: Red press + Blue/Yellow hold
    { 1, 1, 0, 0 }, // Beat 10: Green + Red press (Blue/Yellow released)
    { 0, 0, 0, 0 }, // Beat 11: Rest
    { 1, 1, 1, 1 }, // Beat 12: All buttons press
    { 1, 1, 1, 1 }, // Beat 13: All buttons press
    { 0, 0, 0, 0 }, // Beat 14: Rest
    { 0, 0, 0, 0 }, // Beat 15: Rest
    { 0, 0, 0, 0 }, // Beat 16: Rest
    { 1, 1, 1, 1 }, // Beat 17: All buttons press
    { 1, 1, 1, 1 }, // Beat 18: All buttons press
    { 1, 1, 1, 1 }, // Beat 19: All buttons press
    { 2, 0, 0, 0 }, // Beat 20: Green hold start
    { 2, 1, 0, 0 }, // Beat 21: Green hold + Red press
    { 2, 0, 0, 0 }, // Beat 22: Green hold
    { 2, 1, 0, 0 }, // Beat 23: Green hold + Red press
    { 2, 0, 0, 0 }, // Beat 24: Green hold
    { 2, 1, 0, 0 }, // Beat 25: Green hold + Red press
    { 2, 0, 0, 0 }, // Beat 26: Green hold
    { 2, 1, 0, 0 }, // Beat 27: Green hold + Red press
};

extern const uint8_t test_sequence_all[][4] = {
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest	{1, 1, 1, 1},  // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest	{1, 1, 1, 1},  // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest	{1, 1, 1, 1},  // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest	{1, 1, 1, 1},  // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest	{1, 1, 1, 1},  // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest	{1, 1, 1, 1},  // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
    { 1, 1, 1, 1 }, // Beat 1: Rest
};

#endif /* INC_HARDCODED_SEQUENCES_H_ */
