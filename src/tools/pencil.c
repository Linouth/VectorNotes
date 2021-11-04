#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "path.h"
#include "tool.h"
#include "vec.h"
#include "vectornotes.h"

// TODO: Get rid of global..?
Tool g_tool = {0};

static void mousePosCb(Tool *tool, Vec2 *mouse_pos, int mouse_states[]) {
    static Vec2 *prev_node = NULL;
    static double prev_len = 0;

    if (mouse_states[GLFW_MOUSE_BUTTON_LEFT] == GLFW_PRESS) {
        if (!prev_node) {
            prev_node = path_getNode(tool->tmp_path, -1);
        }

        // TODO: This should probably be in some 'pencil' tool module. Maybe
        // have callback functions per tool
        double cmp = 5.0;
        double curr_len = 0;
        Vec2 prev_node_screen_pos = canvasToScreen(*prev_node);
        if (tool->tmp_path->node_cnt > 1) {
            // The last two points, and a tangent vector determined from
            // these points.
            Vec2 *p1 = path_getNode(tool->tmp_path, -1);
            Vec2 *p0 = path_getNode(tool->tmp_path, -2);
            Vec2 tg = vec2_norm(vec2_sub(*p1, *p0));

            // Vector from the last node to the cursor position.
            Vec2 r = vec2_sub(*mouse_pos, prev_node_screen_pos);
            curr_len = vec2_len(r);

            // Indicator of angle between tangent vector and cursor vector.
            // NOTE: tg is unit length
            double alpha = vec2_dot(r, tg) / curr_len;

            // Determine the length required for a new node to be placed.
            // Line segments can be at most 'max_len' long, and min 'min_len' long.
            // The exponent determines how aggressive the node-placing is.
            const double max_len = 128.0;
            const double min_len = 7.0;
            const double exponent = 512.0;
            cmp = max_len*pow(alpha, exponent) + min_len;

            // Whenever the cursor moves back, place a node to capture this movement
            if (curr_len > prev_len)
                prev_len = curr_len;
        }

        if (curr_len < (prev_len - 5.0)
                || vec2_dist(prev_node_screen_pos, *mouse_pos) > cmp) {
            Vec2 p = screenToCanvas(*mouse_pos);
            path_addNode(tool->tmp_path, p, -1);

            prev_node = path_getNode(tool->tmp_path, -1);
            prev_len = 0;
        }
    }
}

static void mouseBtnCb(Tool *tool, Vec2 *mouse_pos, int button, int action) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            // Create a new path on the very first call.
            if (!tool->tmp_path)
                tool->tmp_path = path_init(0);

            // If the path is ready on update, the tmp_path is reset to 0 count
            // (So the path memory reused)
            assert(tool->tmp_path->node_cnt == 0);

            tool->tmp_path_ready = false;
        } else {
            // Button released
            if (tool->tmp_path->node_cnt > 1)
                tool->tmp_path_ready = true;
        }

        // Button pressed or released, place point at cursor pos.
        // Only if the prev node is not at the exact same position.
        Vec2 p = screenToCanvas(*mouse_pos);
        Vec2 *prev_node = path_getNode(tool->tmp_path, -1);
        if (prev_node->x != p.x || prev_node->y != p.y) {
            path_addNode(tool->tmp_path, p, -1);
            printf("Added a node at %f %f\n", p.x, p.y);
        }
    }
}

// TODO: Probably better to get rid of 'scale' as param here.
static Path *update(Tool *tool, double scale) {
    if (tool->tmp_path_ready) {
        Path *out = path_fitBezier(tool->tmp_path, scale);
        tool->tmp_path->node_cnt = 0;
        tool->tmp_path_ready = false;

        return out;
    }

    return NULL;
}

Tool *pencil_init() {
    Tool *tool = &g_tool;

    tool->mousePosCb = mousePosCb;
    tool->mouseBtnCb = mouseBtnCb;
    tool->update = update;

    tool->tmp_path = NULL;
    tool->tmp_path_ready = false;

    return tool;
}

void pencil_deinit(Tool *tool) {
    if (tool->tmp_path)
        path_deinit(tool->tmp_path);
    tool->tmp_path_ready = false;
}
