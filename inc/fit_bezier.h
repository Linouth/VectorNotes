#pragma once

#include <stdlib.h>

#include "vec.h"

typedef enum fit_dir {
    FIT_DIR_LEFT,
    FIT_DIR_RIGHT,
} FitDir;

typedef struct bezier_coeffs {
    double B0;
    double B1;
    double B2;
    double B3;
} BezierCoeffs;

typedef struct bezier_fit_ctx {
    size_t  count;

    Vec2    *points;        // Not malloc'd
    double  *timestamps;    // Not malloc'd
    double  *params;

    BezierCoeffs *coeffs;

    double tangent_range;   // Range for point averaging for tangent calcs
    double epsilon;         // Max allowed error (distance between point and curve)
    double psi;             // Threshold at which to split curive into multiple
    unsigned max_iter;      // Max depth for Newton-Raphson iteration

    Vec2   *new;
    double *new_ts;
    size_t new_cnt;
    size_t new_capacity;
} BezierFitCtx;

BezierFitCtx *fit_initCtx(Vec2 points[], size_t count);
void fit_deinitCtx(BezierFitCtx *fit);
void fit_addToNewPath(BezierFitCtx *fit, Vec2 point, int ts_index);
Vec2 fit_calcTangent(BezierFitCtx *fit, size_t i_start, size_t i_end, FitDir dir);
void fit_chordLengthParameterization(BezierFitCtx *fit, size_t i_start, size_t i_end);
void fitBezier(BezierFitCtx *fit, Vec2 t1, Vec2 t2, unsigned level, size_t i_start, size_t i_end);
