#include "vec.h"
#include "path.h"

typedef struct tool_ctx {
    void (*mousePosCb)(Vec2 *mouse_pos, int mouse_states[]);
    void (*mouseClickCb)(Vec2 *mouse_pos, int mouse_states[]);

    Path *tmp_path;
} Tool;
