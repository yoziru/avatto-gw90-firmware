#include <unity.h>
#include <CircularBuffer.hpp>
#include <string.h>
#include <string>

static const size_t LOG_BUF_SIZE = 8024;

static void test_log_empty_buffer(void) {
    CircularBuffer<char, LOG_BUF_SIZE> buf;
    TEST_ASSERT_TRUE(buf.isEmpty());
    TEST_ASSERT_EQUAL_INT(0, buf.size());
}

static void test_log_push_single(void) {
    CircularBuffer<char, LOG_BUF_SIZE> buf;
    buf.push('A');
    TEST_ASSERT_FALSE(buf.isEmpty());
    TEST_ASSERT_EQUAL_INT(1, buf.size());
    TEST_ASSERT_EQUAL_CHAR('A', buf[0]);
}

static void test_log_push_multiple(void) {
    CircularBuffer<char, LOG_BUF_SIZE> buf;
    const char *msg = "hello world\n";
    for (size_t i = 0; i < strlen(msg); i++) {
        buf.push(msg[i]);
    }
    TEST_ASSERT_EQUAL_INT((int)strlen(msg), buf.size());

    std::string collected;
    for (size_t i = 0; i < buf.size(); i++) {
        collected += buf[i];
    }
    TEST_ASSERT_EQUAL_STRING("hello world\n", collected.c_str());
}

static void test_log_push_clear(void) {
    CircularBuffer<char, LOG_BUF_SIZE> buf;
    for (int i = 0; i < 100; i++) {
        buf.push('X');
    }
    TEST_ASSERT_EQUAL_INT(100, buf.size());
    buf.clear();
    TEST_ASSERT_TRUE(buf.isEmpty());
    TEST_ASSERT_EQUAL_INT(0, buf.size());
}

static void test_log_circular_wraparound(void) {
    CircularBuffer<char, 16> buf;
    for (int i = 0; i < 20; i++) {
        buf.push('A' + (char)(i % 26));
    }
    TEST_ASSERT_EQUAL_INT(16, buf.size());
    TEST_ASSERT_EQUAL_CHAR('E', buf[0]);
    TEST_ASSERT_EQUAL_CHAR('T', buf[15]);
}

static void test_log_multi_message_sequence(void) {
    CircularBuffer<char, LOG_BUF_SIZE> buf;

    const char *msg1 = "[100] | first message\n";
    const char *msg2 = "[200] | second message\n";
    const char *msg3 = "[300] | third message\n";

    for (size_t i = 0; i < strlen(msg1); i++) buf.push(msg1[i]);
    for (size_t i = 0; i < strlen(msg2); i++) buf.push(msg2[i]);
    for (size_t i = 0; i < strlen(msg3); i++) buf.push(msg3[i]);

    std::string collected;
    for (size_t i = 0; i < buf.size(); i++) {
        collected += buf[i];
    }

    TEST_ASSERT_TRUE(strstr(collected.c_str(), "first message") != nullptr);
    TEST_ASSERT_TRUE(strstr(collected.c_str(), "second message") != nullptr);
    TEST_ASSERT_TRUE(strstr(collected.c_str(), "third message") != nullptr);
}

static void test_log_clear_after_full(void) {
    CircularBuffer<char, 64> buf;
    for (int i = 0; i < 64; i++) {
        buf.push('Z');
    }
    TEST_ASSERT_EQUAL_INT(64, buf.size());
    TEST_ASSERT_TRUE(buf.isFull());
    buf.clear();
    TEST_ASSERT_TRUE(buf.isEmpty());
    TEST_ASSERT_EQUAL_INT(0, buf.size());
}

static void test_log_push_after_clear(void) {
    CircularBuffer<char, LOG_BUF_SIZE> buf;
    for (int i = 0; i < 50; i++) buf.push('Q');
    buf.clear();
    buf.push('N');
    TEST_ASSERT_EQUAL_INT(1, buf.size());
    TEST_ASSERT_EQUAL_CHAR('N', buf[0]);
}

void run_log_tests(void) {
    RUN_TEST(test_log_empty_buffer);
    RUN_TEST(test_log_push_single);
    RUN_TEST(test_log_push_multiple);
    RUN_TEST(test_log_push_clear);
    RUN_TEST(test_log_circular_wraparound);
    RUN_TEST(test_log_multi_message_sequence);
    RUN_TEST(test_log_clear_after_full);
    RUN_TEST(test_log_push_after_clear);
}
