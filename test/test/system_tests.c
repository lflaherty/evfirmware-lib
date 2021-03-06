/*
 * system_tests.c
 *
 *  Created on: 1 May 2022
 *      Author: Liam Flaherty
 */

#include "unity.h"
#include "unity_fixture.h"

static void RunSystemTests(void)
{
    RUN_TEST_GROUP(COMM_CAN);
    RUN_TEST_GROUP(COMM_UART);
    RUN_TEST_GROUP(COMM_MSGFRAMEDECODE);
    RUN_TEST_GROUP(COMM_MSGFRAMEENCODE);
    RUN_TEST_GROUP(IO_ADC);
    RUN_TEST_GROUP(IO_GPIO);
    RUN_TEST_GROUP(TIME_TASKTIMER);
}
