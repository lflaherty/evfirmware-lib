## TestTaskTimer
add_executable(TestTaskTimer TestTaskTimer.c)
# Test harness
target_sources(TestTaskTimer PRIVATE ${PROJECT_SOURCE_DIR}/Unity/src/unity.c)
target_sources(TestTaskTimer PRIVATE ${PROJECT_SOURCE_DIR}/Unity/extras/fixture/src/unity_fixture.c)
# Mocks for 3rd party
target_sources(TestTaskTimer PRIVATE ${PROJECT_SOURCE_DIR}/mock/FreeRTOS/mockFreeRTOS.c)
target_sources(TestTaskTimer PRIVATE ${PROJECT_SOURCE_DIR}/mock/FreeRTOS/mockQueue.c)
target_sources(TestTaskTimer PRIVATE ${PROJECT_SOURCE_DIR}/mock/FreeRTOS/mockStreamBuffer.c)
target_sources(TestTaskTimer PRIVATE ${PROJECT_SOURCE_DIR}/mock/FreeRTOS/mockTask.c)
target_sources(TestTaskTimer PRIVATE ${PROJECT_SOURCE_DIR}/mock/FreeRTOS/mockSemphr.c)
target_sources(TestTaskTimer PRIVATE ${PROJECT_SOURCE_DIR}/mock/std/MockStdio.c)
target_sources(TestTaskTimer PRIVATE ${PROJECT_SOURCE_DIR}/mock/stm32_hal/MockStm32f7xx_hal.c)
target_sources(TestTaskTimer PRIVATE ${PROJECT_SOURCE_DIR}/mock/stm32_hal/MockStm32f7xx_hal_tim.c)
# Mocks for 1st party
target_sources(TestTaskTimer PRIVATE ${PROJECT_SOURCE_DIR}/mock/lib/logging/MockLogging.c)
# Production code
target_sources(TestTaskTimer PRIVATE ${PROJECT_SOURCE_DIR}/systemlib-src/lib/depends/depends.c)
