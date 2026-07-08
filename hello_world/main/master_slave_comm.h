typedef struct
{
    uint32_t counter;
} message_t;

typedef enum
{
    CMD_SET_POSITION = 1,
    CMD_GET_POSITION = 2,
    CMD_POSITION_RESPONSE = 3,
    CMD_ACK_POSITION = 4
} command_t;

typedef struct
{
    uint32_t id;
    uint8_t cmd;
    int32_t x;
    int32_t y;
} message_t;
