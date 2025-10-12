#define ASSERT_MESSAGE(condition, message)                                                         \
    if (!(condition))                                                                              \
    {                                                                                              \
        std::cout << message << std::endl;                                                         \
        assert(condition);                                                                         \
    }
