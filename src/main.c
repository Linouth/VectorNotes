// Next up: Determine rate at which the direction is changing. Calc the current
// tangent vector and project the new r vector on it. Use that projection as
// some tolerance / weight for how frequent points should be placed. (e.g. If
// projection is 100%, only place every x pixels. If projection is only 60% or
// so, place very frequently as we are probably going around a curve right now)

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "nanovg/nanovg.h"
//#define NANOVG_GL3_IMPLEMENTATION
//#include "nanovg/nanovg_gl.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <string.h>

#include "vec.h"
#include "gl.h"
#include "fit_bezier.h"
#include "path.h"
#include "ui.h"

#define WIDTH 800
#define HEIGHT 600

Path *g_path;
Path *dbg;
Path *new;

static void glfwError(int id, const char* desc) {
    fprintf(stderr, "Error(GLFW): %s\n", desc);
}

int main(void) {
    g_path = path_init(0);
    dbg = path_init(0);

    glfwSetErrorCallback(&glfwError);
    glfwInit();

    UI *ui = ui_init(WIDTH, HEIGHT);
    if (ui == NULL) {
        glfwTerminate();
        return -1;
    }

    Vec2 test[] = {
        //{400.0, 200.0},
        //{200.0, 250.0},
        //{220.0, 400.0},
        //{400.0, 350.0},

        {465.000000, 323.000000},
        {463.000000, 313.000000},
        {461.000000, 303.000000},
        {459.000000, 293.000000},
        {457.000000, 283.000000},
        {457.000000, 272.000000},
        {457.000000, 260.000000},
        {457.000000, 249.000000},
        {458.000000, 239.000000},
        {460.000000, 229.000000},
        {463.000000, 219.000000},
        {467.000000, 209.000000},
        {472.000000, 199.000000},
        {479.000000, 191.000000},
        {487.000000, 183.000000},
        {496.000000, 177.000000},
        {507.000000, 173.000000},
        {517.000000, 171.000000},
        {529.000000, 171.000000},
        {539.000000, 172.000000},
        {549.000000, 175.000000},
        {559.000000, 179.000000},
        {570.000000, 183.000000},
        {580.000000, 188.000000},
        {591.000000, 194.000000},
        {600.000000, 199.000000},
        {609.000000, 205.000000},
        {618.000000, 211.000000},
        {626.000000, 218.000000},
        {634.000000, 226.000000},
        {641.000000, 234.000000},
        {647.000000, 243.000000},
        {652.000000, 252.000000},
        {657.000000, 262.000000},
        {661.000000, 273.000000},
        {663.000000, 283.000000},
        {665.000000, 293.000000},
        {666.000000, 303.000000},
        {667.000000, 313.000000},
        {667.000000, 324.000000},
        {667.000000, 335.000000},
        {665.000000, 345.000000},
    };

    for (size_t i = 0; i < sizeof(test) / sizeof(Vec2); i++) {
        path_addNode(g_path, test[i], -1);
    }

    new = path_fitBezier(g_path, ui->view_scale);
    printf("New has %ld items\n", new->node_cnt);
    for (size_t i = 0; i < new->node_cnt-1; i+=3) {
        printf("{%f, %f}, {%f, %f}, {%f, %f}, {%f, %f},\n",
                new->nodes[i+0].x, new->nodes[i+0].y,
                new->nodes[i+1].x, new->nodes[i+1].y,
                new->nodes[i+2].x, new->nodes[i+2].y,
                new->nodes[i+3].x, new->nodes[i+3].y);
    }

    glfwSetTime(0);

    Path *paths[16];
    size_t path_cnt = 0;

    NVGcontext *vg = ui->vg;

    ui->view_scale = 1.0;

    while (!glfwWindowShouldClose(ui->window)) {
        if (ui->tmp_path_ready && path_cnt < 16) {
            paths[path_cnt] = path_fitBezier(ui->tmp_path, ui->view_scale);
            ui->tmp_path_ready = false;

            path_cnt += 1;
        }

        glClear(GL_COLOR_BUFFER_BIT);

        nvgBeginFrame(vg, ui->view_width, ui->view_height, 1.0);
        nvgSave(vg);
        {
            nvgLineCap(vg, NVG_ROUND);
            nvgLineJoin(vg, NVG_MITER);
            nvgStrokeWidth(vg, 2.0f);
            nvgStrokeColor(vg, nvgRGBA(82, 144, 242, 255));

            Vec2 *node = NULL;
            node = &g_path->nodes[0];

            ui_drawLines(ui, g_path);

            ui_drawPath(ui, new);

            if (ui->tmp_path && ui->tmp_path->node_cnt >= 2) {
                ui_drawLines(ui, ui->tmp_path);
            }

            for (size_t i = 0; i < path_cnt; i++) {
                ui_drawPath(ui, paths[i]);
            }
        }
        nvgRestore(vg);
        nvgEndFrame(vg);

        if (ui->debug) {
            ui_drawCtrlPoints(ui, new);
            for (size_t i = 0; i < path_cnt; i++) {
                ui_drawCtrlPoints(ui, paths[i]);
            }

            {
                Rgb rgb = {255.0f/255, 200.0f/255, 64.0f/255};
                ui_drawDbgLines(ui, dbg->nodes, dbg->node_cnt, rgb, 1.0);
            }
        }

        // TODO: Adjacency is currently hacked in by setting the last+1 element
        // equal to the last. This will buffer overflow.
        //glDrawArrays(GL_LINE_STRIP_ADJACENCY, 0, 4);
        //glDrawArrays(GL_LINE_STRIP_ADJACENCY, 0, g_path.node_cnt+1);

        //glEnable(GL_LINE_SMOOTH);
        //glLineWidth(16.0f);
        //glPatchParameteri(GL_PATCH_VERTICES, 4);
        //glDrawArrays(GL_PATCHES, 0, 4);

        //glEnable(GL_LINE_SMOOTH);
        //glLineWidth(5.0f);
        //glDrawArrays(GL_LINE_STRIP, 0, g_path.node_cnt);

        //printf("x: %f, y: %f; bl: %d, br: %d; origin x: %f, y: %f; scale: %f\n", ui->mouse_pos.x, ui->mouse_pos.y,
        //        ui->mouse_states[GLFW_MOUSE_BUTTON_LEFT],
        //        ui->mouse_states[GLFW_MOUSE_BUTTON_RIGHT],
        //        ui->view_origin.x, ui->view_origin.y,
        //        ui->view_scale);

        glfwSwapBuffers(ui->window);
        glfwWaitEventsTimeout(0.016666);
    }
    path_deinit(g_path);
    path_deinit(dbg);
    path_deinit(new);

    ui_deinit(ui);
    return 0;
}
