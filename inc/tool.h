#pragma once

#include <stdbool.h>

#include "vec.h"
#include "path.h"

// TODO: Dont hardcode all tools, there should be some array to which tools can
// be added, and the order of this array would also indicate the order in the
// toolbar. (maybe)
typedef enum tools {
    TOOLS_pencil,
    TOOLS_count,
} Tools;

typedef struct tool_ctx Tool;

// TODO: Add ToolType type to differentiate between selection, deletion and
// creation tools (and more)
struct tool_ctx {
    void (*mousePosCb)(Tool *tool, Vec2 *mouse_pos, int mouse_states[]);
    void (*mouseBtnCb)(Tool *tool, Vec2 *mouse_pos, int button, int action);
    Path *(*update)(Tool *tool, double scale);

    Path *tmp_path;
    bool tmp_path_ready;
};

Tool *pencil_init();
void pencil_deinit(Tool *tool);
