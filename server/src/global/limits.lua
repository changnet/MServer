local LIMIT = {
    --[[ those number define in /usr/include/stdint.h ]]
    INT8_MIN = (-128),
    INT16_MIN = (-32767 - 1),
    INT32_MIN = (-2147483647 - 1),
    INT64_MIN = ((9223372036854775807) - 1),
    --[[ Maximum of signed integral types.  ]]
    INT8_MAX = (127),
    INT16_MAX = (32767),
    INT32_MAX = (2147483647),
    INT64_MAX = ((9223372036854775807)),
    --[[ Maximum of unsigned integral types.  ]]
    UINT8_MAX = (255),
    UINT16_MAX = (65535),
    UINT32_MAX = (4294967295),
    UINT64_MAX = ((18446744073709551615)),

    DOUBLE_MAX = 1.79769e+308,
    DOUBLE_MIN = 2.22507e-308
}

return LIMIT
