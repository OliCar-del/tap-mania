/**
 ******************************************************************************
 * @file           : test_sequences.h
 * @brief          : Test Sequence Definitions and Loading
 ******************************************************************************
 */

#ifndef TEST_SEQUENCES_H
#define TEST_SEQUENCES_H

#include "main.h"
#include "sequence.h"
#include <stdint.h>

// Test sequence identifiers
#define TEST_SEQ_SIMPLE 1
#define TEST_SEQ_HOLDS 2
#define TEST_SEQ_COMPLEX 3
#define TEST_SEQ_ALL 4

/**
 * @brief Load a test sequence
 * @param sequence_num Sequence number (1-4)
 */
void LoadTestSequence(uint8_t sequence_num);

#endif /* TEST_SEQUENCES_H */
