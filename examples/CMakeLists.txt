badger_add_executable(
    NAME usable_size
    SRCS mimalloc/usable_size.cc
    DEPS mimalloc
    ENABLE_WARNINGS
    ENABLE_SANITIZERS
)