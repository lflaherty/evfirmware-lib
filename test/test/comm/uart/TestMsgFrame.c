/*
 * TestMsgFrame.c
 * 
 *  Created on: 25 Apr 2022
 *      Author: Liam Flaherty
 */

#include "unity.h"
#include "unity_fixture.h"

// Mocks for code under test
#include "stm32_hal/MockStm32f7xx_hal.h"

// source code under test
#include "comm/uart/msgframe.c"

const uint16_t MSG_LEN = 11U;

CRC_HandleTypeDef hcrc;
MsgFrame_T mMsgFrame;

// Helper macro for changing endianness
#define BIG_TO_LITTLE_ENDIAN_U32(x) (((x >> 24) & 0xff) | \
                                      ((x << 8) & 0xff0000) | \
                                      ((x >> 8) & 0xff00) | \
                                      ((x << 24) & 0xff000000))

TEST_GROUP(COMM_MSGFRAME);

TEST_SETUP(COMM_MSGFRAME)
{
    hcrc.InputDataFormat = CRC_INPUTDATA_FORMAT_BYTES;

    memset(mMsgFrame.data, 0U, MSGFRAME_BUFFER_LEN * sizeof(uint8_t));
    mMsgFrame.msgLen = MSG_LEN;
    mMsgFrame.hcrc = &hcrc;
    bool succ = MsgFrame_Init(&mMsgFrame);

    TEST_ASSERT_TRUE(succ);
    TEST_ASSERT_EQUAL(MSG_LEN, mMsgFrame.msgLen);
    TEST_ASSERT_EQUAL(0U, mMsgFrame.start);
    TEST_ASSERT_EQUAL(0U, mMsgFrame.end);
    TEST_ASSERT_EQUAL(MSGFRAME_BUFFER_LEN, mMsgFrame.availableBytes);
}

TEST_TEAR_DOWN(COMM_MSGFRAME)
{
    // Empty
}

TEST(COMM_MSGFRAME, TestInitTooLong)
{
    MsgFrame_T mf2;
    mf2.msgLen = 1045U;
    mf2.hcrc = &hcrc;

    TEST_ASSERT_FALSE(MsgFrame_Init(&mf2));
}

TEST(COMM_MSGFRAME, TestMsgRecvBytes)
{
    mockSet_CRC(BIG_TO_LITTLE_ENDIAN_U32(0x4FCD556FU));
    bool succ;

    // Receive first message
    uint8_t msg1[] = {':', 0x01, 0x02, 0x03, 0x04, 0x4F, 0xCD, 0x55, 0x6F, '\r', '\n'};
    size_t msg1Len = sizeof(msg1) / sizeof(uint8_t);

    succ = MsgFrame_RecvBytes(&mMsgFrame, msg1, (uint16_t)msg1Len);

    uint16_t expectedStart = 0U;
    uint16_t expectedEnd = (uint16_t)msg1Len;
    uint16_t expectedBufferRemaining = MSGFRAME_BUFFER_LEN - (uint16_t)msg1Len;
    uint8_t expectedBuffer1[] = {':', 0x01, 0x02, 0x03, 0x04, 0x4F, 0xCD, 0x55, 0x6F, '\r', '\n'};
    size_t expectedBuffer1Len = sizeof(expectedBuffer1) / sizeof(uint8_t);
    TEST_ASSERT_TRUE(succ);
    TEST_ASSERT_EQUAL(MSG_LEN, mMsgFrame.msgLen);
    TEST_ASSERT_EQUAL(expectedStart, mMsgFrame.start);
    TEST_ASSERT_EQUAL(expectedEnd, mMsgFrame.end);
    TEST_ASSERT_EQUAL(expectedBufferRemaining, mMsgFrame.availableBytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedBuffer1, mMsgFrame.data, expectedBuffer1Len);

    // Receive some new bad data
    uint8_t msg2[] = {0x55, ':', '\r'};
    size_t msg2Len = sizeof(msg2) / sizeof(uint8_t);

    succ = MsgFrame_RecvBytes(&mMsgFrame, msg2, (uint16_t)msg2Len);

    expectedStart += 0U;
    expectedEnd += msg2Len;
    expectedBufferRemaining -= msg2Len;
    uint8_t expectedBuffer2[] = {':', 0x01, 0x02, 0x03, 0x04, 0x4F, 0xCD, 0x55, 0x6F, '\r', '\n',
                                 0x55, ':', '\r'};
    size_t expectedBuffer2Len = sizeof(expectedBuffer2) / sizeof(uint8_t);
    TEST_ASSERT_TRUE(succ);
    TEST_ASSERT_EQUAL(MSG_LEN, mMsgFrame.msgLen);
    TEST_ASSERT_EQUAL(expectedStart, mMsgFrame.start);
    TEST_ASSERT_EQUAL(expectedEnd, mMsgFrame.end);
    TEST_ASSERT_EQUAL(expectedBufferRemaining, mMsgFrame.availableBytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedBuffer2, mMsgFrame.data, expectedBuffer2Len);

    // Receive another valid message
    uint8_t msg3[] = {':', 0x01, 0x02, 0x03, 0x04, 0x4F, 0xCD, 0x55, 0x6F, '\r', '\n'};
    size_t msg3Len = sizeof(msg3) / sizeof(uint8_t);

    succ = MsgFrame_RecvBytes(&mMsgFrame, msg3, (uint16_t)msg3Len);

    expectedStart += 0U;
    expectedEnd += msg3Len;
    expectedBufferRemaining -= msg3Len;
    uint8_t expectedBuffer3[] = {':', 0x01, 0x02, 0x03, 0x04, 0x4F, 0xCD, 0x55, 0x6F, '\r', '\n',
                                 0x55, ':', '\r',
                                 ':', 0x01, 0x02, 0x03, 0x04, 0x4F, 0xCD, 0x55, 0x6F, '\r', '\n'};
    size_t expectedBuffer3Len = sizeof(expectedBuffer3) / sizeof(uint8_t);
    TEST_ASSERT_TRUE(succ);
    TEST_ASSERT_EQUAL(MSG_LEN, mMsgFrame.msgLen);
    TEST_ASSERT_EQUAL(expectedStart, mMsgFrame.start);
    TEST_ASSERT_EQUAL(expectedEnd, mMsgFrame.end);
    TEST_ASSERT_EQUAL(expectedBufferRemaining, mMsgFrame.availableBytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedBuffer3, mMsgFrame.data, expectedBuffer3Len);
}

TEST(COMM_MSGFRAME, TestMsgRecvMsg)
{
    mockSet_CRC(BIG_TO_LITTLE_ENDIAN_U32(0x4FCD556FU));

    // DMA might receive one message and some other bytes
    uint8_t msg1[] = {':', 0x01, 0x02, 0x03, 0x04, 0x4F, 0xCD, 0x55, 0x6F, '\r', '\n'};
    size_t msg1Len = sizeof(msg1) / sizeof(uint8_t);
    uint8_t msg2[] = {0x55, ':', '\r'};
    size_t msg2Len = sizeof(msg2) / sizeof(uint8_t);

    TEST_ASSERT_TRUE(MsgFrame_RecvBytes(&mMsgFrame, msg1, (uint16_t)msg1Len));
    TEST_ASSERT_TRUE(MsgFrame_RecvBytes(&mMsgFrame, msg2, (uint16_t)msg2Len));

    size_t offset;
    bool recv;

    // first attempt should work
    recv = MsgFrame_RecvMsg(&mMsgFrame, &offset);
    TEST_ASSERT_TRUE(recv);
    TEST_ASSERT_EQUAL(0U, offset);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(msg1, &mMsgFrame.data[offset], MSG_LEN);

    // second shouldn't give a valid message, but the bytes should be shifted down
    recv = MsgFrame_RecvMsg(&mMsgFrame, &offset);
    TEST_ASSERT_FALSE(recv);

    TEST_ASSERT_EQUAL(0U, mMsgFrame.start);
    TEST_ASSERT_EQUAL(msg2Len, mMsgFrame.end);
    TEST_ASSERT_EQUAL(MSGFRAME_BUFFER_LEN - (uint16_t)msg2Len, mMsgFrame.availableBytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(msg2, mMsgFrame.data, msg2Len);


    // now put another valid message after the random bytes
    TEST_ASSERT_TRUE(MsgFrame_RecvBytes(&mMsgFrame, msg1, (uint16_t)msg1Len));

    recv = MsgFrame_RecvMsg(&mMsgFrame, &offset);
    TEST_ASSERT_TRUE(recv);
    TEST_ASSERT_EQUAL(3U, offset);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(msg1, &mMsgFrame.data[offset], MSG_LEN);

    // buffer should be empty after another call
    recv = MsgFrame_RecvMsg(&mMsgFrame, &offset);
    TEST_ASSERT_FALSE(recv);
    TEST_ASSERT_EQUAL(0U, mMsgFrame.start);
    TEST_ASSERT_EQUAL(0U, mMsgFrame.end);
    TEST_ASSERT_EQUAL(MSGFRAME_BUFFER_LEN, mMsgFrame.availableBytes);
}

TEST(COMM_MSGFRAME, TestMsgBadCrc)
{
    mockSet_CRC(BIG_TO_LITTLE_ENDIAN_U32(0x4FCD556FU));

    uint8_t msgBadCrc[] = {':', 0x01, 0x02, 0x03, 0x04, 0xFF, 0xFF, 0xFF, 0xFF, '\r', '\n'};
    size_t msgBadCrcLen = sizeof(msgBadCrc) / sizeof(uint8_t);
    uint8_t msgGoodCrc[] = {':', 0x01, 0x02, 0x03, 0x04, 0x4F, 0xCD, 0x55, 0x6F, '\r', '\n'};
    size_t msgGoodCrcLen = sizeof(msgGoodCrc) / sizeof(uint8_t);

    TEST_ASSERT_TRUE(MsgFrame_RecvBytes(&mMsgFrame, msgBadCrc, (uint16_t)msgBadCrcLen));

    size_t offset;
    bool recv;

    // message with bad CRC should ignore message
    recv = MsgFrame_RecvMsg(&mMsgFrame, &offset);
    TEST_ASSERT_FALSE(recv);

    TEST_ASSERT_TRUE(MsgFrame_RecvBytes(&mMsgFrame, msgGoodCrc, (uint16_t)msgGoodCrcLen));

    // message with ok CRC should work
    recv = MsgFrame_RecvMsg(&mMsgFrame, &offset);
    TEST_ASSERT_TRUE(recv);
    TEST_ASSERT_EQUAL(11U, offset);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(msgGoodCrc, &mMsgFrame.data[offset], MSG_LEN);

    // check cleanup works ok
    recv = MsgFrame_RecvMsg(&mMsgFrame, &offset);
    TEST_ASSERT_FALSE(recv);
    TEST_ASSERT_EQUAL(0U, mMsgFrame.start);
    TEST_ASSERT_EQUAL(0U, mMsgFrame.end);
    TEST_ASSERT_EQUAL(MSGFRAME_BUFFER_LEN, mMsgFrame.availableBytes);
}

TEST(COMM_MSGFRAME, TestRecvTooManyBytes)
{
    uint8_t testByte1 = 0xAB;
    uint8_t testByte2 = 0xEF;

    // Fill up buffer ok
    for (uint16_t i = 0; i < MSGFRAME_BUFFER_LEN; ++i) {
        TEST_ASSERT_TRUE(MsgFrame_RecvBytes(&mMsgFrame, &testByte1, sizeof(uint8_t)));
    }

    // Buffer should all be the correct bytes
    for (uint16_t i = 0; i < MSGFRAME_BUFFER_LEN; ++i) {
        TEST_ASSERT_EQUAL(testByte1, mMsgFrame.data[i]);
    }

    // Shouldn't be able to receive any more bytes
    for (uint16_t i = 0; i < MSGFRAME_BUFFER_LEN; ++i) {
        TEST_ASSERT_FALSE(MsgFrame_RecvBytes(&mMsgFrame, &testByte2, sizeof(uint8_t)));
    }

    // Buffer shouldn't have changed
    for (uint16_t i = 0; i < MSGFRAME_BUFFER_LEN; ++i) {
        TEST_ASSERT_EQUAL(testByte1, mMsgFrame.data[i]);
    }
}

TEST_GROUP_RUNNER(COMM_MSGFRAME)
{
    RUN_TEST_CASE(COMM_MSGFRAME, TestInitTooLong);
    RUN_TEST_CASE(COMM_MSGFRAME, TestMsgRecvBytes);
    RUN_TEST_CASE(COMM_MSGFRAME, TestMsgRecvMsg);
    RUN_TEST_CASE(COMM_MSGFRAME, TestMsgBadCrc);
    RUN_TEST_CASE(COMM_MSGFRAME, TestRecvTooManyBytes);
}
