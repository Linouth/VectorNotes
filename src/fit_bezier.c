// Implementation of the algorithm described in 'An Algorithm for Automatically
// Fitting Digitized Curves' by Philip J. Schneider

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>

#include "fit_bezier.h"
#include "vec.h"

// Temp for debugging
typedef struct path Path;
void path_addNode(Path *path, Vec2 node, double timestamp);
extern Path *dbg;

BezierFitCtx *fit_initCtx(Vec2 points[], size_t count) {
    BezierFitCtx *fit = malloc(sizeof(BezierFitCtx));
    assert(fit != NULL);

    fit->count = count;
    fit->points = points;
    fit->timestamps = NULL;
    fit->params = malloc(sizeof(double) * count);
    fit->coeffs = malloc(sizeof(BezierCoeffs) * count);

    // Sane defaults
    fit->tangent_range = 30.0;
    fit->epsilon = 8.0;
    fit->psi = 30.0;
    fit->max_iter = 3;

    fit->new = malloc(sizeof(Vec2) * count);
    fit->new_ts = malloc(sizeof(double) * count);
    fit->new_cnt = 0;
    fit->new_capacity = count;

    assert(fit->params != NULL);
    assert(fit->coeffs != NULL);
    assert(fit->new != NULL);
    assert(fit->new_ts != NULL);

    return fit;
}

void fit_deinitCtx(BezierFitCtx *fit) {
    free(fit->params);
    free(fit->coeffs);
    free(fit->new);
    free(fit->new_ts);
    free(fit);
}

void fit_addToNewPath(BezierFitCtx *fit, Vec2 point, int ts_index) {
    // Make sure that the capacity of the new list is large enough for all
    // points. The list is initialized with the same size as the input points
    // list.
    if (fit->new_cnt >= fit->new_capacity) {
        fit->new = realloc(fit->new, sizeof(Vec2) * fit->new_capacity * 2);
        fit->new_ts = realloc(fit->new_ts, sizeof(double) * fit->new_capacity * 2);

        assert(fit->new != NULL);
        assert(fit->new_ts != NULL);
    }

    fit->new[fit->new_cnt] = point;

    if (fit->timestamps && ts_index >= 0) {
        fit->new_ts[fit->new_cnt] = fit->timestamps[ts_index];
    } else {
        fit->new_ts[fit->new_cnt] = 0;
    }

    fit->new_cnt++;
}

Vec2 calcBezier(BezierFitCtx *fit, unsigned index, Vec2 v0, Vec2 v1, Vec2 v2, Vec2 v3) {
    Vec2 p = vec2_scalarMult(v0, fit->coeffs[index].B0);
    p = vec2_add(p, vec2_scalarMult(v1, fit->coeffs[index].B1));
    p = vec2_add(p, vec2_scalarMult(v2, fit->coeffs[index].B2));
    p = vec2_add(p, vec2_scalarMult(v3, fit->coeffs[index].B3));
    return p;
}

Vec2 fit_calcTangent(BezierFitCtx *fit, size_t i_start, size_t i_end, FitDir dir) {
    assert(i_start != i_end);

    int negate;
    size_t i_endpoint;
    if (dir == FIT_DIR_RIGHT) {
        assert(i_start != fit->count-1);
        negate = 1;
        i_endpoint = i_start;
    } else {
        assert(i_end != 0);
        negate = -1;
        i_endpoint = i_end;
    }

    Vec2 avg = fit->points[i_endpoint + negate];

    if (i_end - i_start > 1) {
        // More than two points available, check them all

        size_t ii;
        for (ii = 2; ii < i_end - i_start; ii++) {
            if (vec2_dist(fit->points[i_endpoint], fit->points[i_endpoint+negate*ii])
                    > fit->tangent_range) {
                break;
            }
            avg = vec2_add(avg, fit->points[i_endpoint + negate*ii]);
        }
        avg = vec2_scalarMult(avg, 1.0 / (ii-1));
    }

    return vec2_norm(vec2_sub(avg, fit->points[i_endpoint]));
}

void fit_chordLengthParameterization(BezierFitCtx *fit, size_t i_start, size_t i_end) {
    fit->params[i_start] = 0.0;
    for (size_t i = i_start+1; i <= i_end; i++) {
        fit->params[i] = fit->params[i-1] + vec2_dist(fit->points[i], fit->points[i-1]);
    }
    for (size_t i = i_start+1; i <= i_end; i++) {
        fit->params[i] = fit->params[i] / fit->params[i_end];
    }
}

void reparameterize(BezierFitCtx *fit, Vec2 v0, Vec2 v1, Vec2 v2, Vec2 v3, size_t i_start, size_t i_end) {
    assert(i_end < fit->count);
    for (size_t i = i_start; i <= i_end; i++) {
        double u = fit->params[i];
        Vec2 d = fit->points[i];

        double omu = 1-u;

        double dB0 = 3 * omu*omu;
        double dB1 = 6*u * omu;
        double dB2 = 3*u*u;

        double ddB0 = 6 * omu;
        double ddB1 = 6*u;

        Vec2 Q = calcBezier(fit, i, v0, v1, v2, v3);

        Vec2 dQ = vec2_scalarMult(vec2_sub(v1, v0), dB0);
        dQ = vec2_add(dQ, vec2_scalarMult(vec2_sub(v2, v1), dB1));
        dQ = vec2_add(dQ, vec2_scalarMult(vec2_sub(v3, v2), dB2));

        Vec2 ddQ = vec2_scalarMult(
                vec2_add(
                    vec2_sub(v2, vec2_scalarMult(v1, 2)),
                    v0),
                ddB0);

        ddQ = vec2_add(ddQ, vec2_scalarMult(
                vec2_add(
                    vec2_sub(v3, vec2_scalarMult(v1, 2)),
                    v1),
                ddB1));

        double num = vec2_dot(vec2_sub(Q, d), dQ);
        double denom = vec2_dot(dQ, dQ) + vec2_dot(vec2_sub(Q, d), ddQ);

        if (denom == 0.0f) {
            fit->params[i] = u;
        } else {
            fit->params[i] = u - (num / denom);
        }
    }
    assert(fit->params[i_start] == 0.0f);
    assert(fit->params[i_end] == 1.0f);
}

// TODO still:
//  - Map t0 and tend to the final path.
void fitBezier(BezierFitCtx *fit, Vec2 t1, Vec2 t2, unsigned level, size_t i_start, size_t i_end) {
    printf("fitBezier called; i_start=%ld, i_end=%ld\n", i_start, i_end);

    assert(i_end < fit->count);
    Vec2 v0 = fit->points[i_start];
    Vec2 v3 = fit->points[i_end];

    if (i_end - i_start == 1) {
        // Only two points

        double dist = vec2_dist(v0, v3) / 3.0;
        printf("Only two points! %f\n", dist);
        fit_addToNewPath(fit, vec2_add(v0, vec2_scalarMult(t1, dist)), -1);
        fit_addToNewPath(fit, vec2_add(v3, vec2_scalarMult(t2, dist)), -1);
        fit_addToNewPath(fit, v3, i_end);
        return;
    }

    double c11, c1221, c22, x1, x2;
    c11 = c1221 = c22 = x1 = x2 = 0;

    // Fit a bezier to a set of points
    for (size_t i = i_start; i <= i_end; i++) {
        double u = fit->params[i];
        Vec2 d = fit->points[i];

        BezierCoeffs *c = &fit->coeffs[i];

        double omu = 1-u;

        // Calculate and cache binomial coefficients
        c->B0 = omu*omu*omu;
        double B1 = c->B1 = 3*u * omu*omu;
        double B2 = c->B2 = 3*u*u * omu;
        c->B3 = u*u*u;

        Vec2 A1 = vec2_scalarMult(t1, B1);
        Vec2 A2 = vec2_scalarMult(t1, B2);

        c11 += vec2_dot(A1, A1);
        c1221 += vec2_dot(A1, A2);
        c22 += vec2_dot(A2, A2);

        Vec2 bisum = calcBezier(fit, i, v0, v0, v3, v3);

        Vec2 sub = vec2_sub(d, bisum);
        x1 += vec2_dot(sub, A1);
        x2 += vec2_dot(sub, A2);
    }

    double a1 = (x1*c22 - c1221*x2) / (c11*c22 - c1221*c1221);
    double a2 = (c11*x2 - x1*c1221) / (c11*c22 - c1221*c1221);

    Vec2 v1 = vec2_add(v0, vec2_scalarMult(t1, a1));
    Vec2 v2 = vec2_add(v3, vec2_scalarMult(t2, a2));
    //printf("a1=%f, a2=%f\n", a1, a2);

    // Calculate the error (distance) between the curve and the points
    double max_err = 0;
    double max_err_t = 0;
    size_t max_err_i = 0;
    Vec2 max_err_d = {0, 0};
    for (size_t i = i_start; i <= i_end; i++) {
        double t = fit->params[i]; // Only needed for Bn calcs, so can be removed now
        Vec2 d = fit->points[i];

        Vec2 p = calcBezier(fit, i, v0, v1, v2, v3);

        double err = vec2_distSqr(d, p);
        if (err > max_err) {
            max_err = err;
            max_err_t = t;
            max_err_i = i;
            max_err_d = d;
        }
    }
    printf("Level %d; Max distance error is %f at t=%f\n", level, sqrt(max_err), max_err_t);

    if (max_err > fit->psi*fit->psi) {
        // Error is very large, split the curve into multiple paths and try on
        // these paths separately.

        printf("Splitting! err=%f, i=%ld\n", sqrt(max_err), max_err_i);

        //Vec2 t_split = vec2_norm(vec2_sub(fit->points[max_err_i-1], fit->points[max_err_i+1]));

        Vec2 t_split = vec2_norm(
                vec2_scalarMult(vec2_add(
                    vec2_sub(fit->points[max_err_i-1], fit->points[max_err_i]),
                    vec2_sub(fit->points[max_err_i], fit->points[max_err_i+1])),
                0.5));

        fit_chordLengthParameterization(fit, i_start, max_err_i);
        fitBezier(fit, t1, t_split, level, i_start, max_err_i);

        t_split = vec2_scalarMult(t_split, -1);
        fit_chordLengthParameterization(fit, max_err_i, i_end);
        fitBezier(fit, t_split, t2, level, max_err_i, i_end);
        return;
    } else if (max_err > fit->epsilon*fit->epsilon && level < fit->max_iter) {
        // The error is fairly small but still too large, try to improve by
        // reparameterizing.

        reparameterize(fit, v0, v1, v2, v3, i_start, i_end);
        fitBezier(fit, t1, t2, level+1, i_start, i_end);
        return;
    };
    // Error is small enough; continue

    Vec2 p = vec2_scalarMult(v0, fit->coeffs[max_err_i].B0);
    p = vec2_add(p, vec2_scalarMult(v1, fit->coeffs[max_err_i].B1));
    p = vec2_add(p, vec2_scalarMult(v2, fit->coeffs[max_err_i].B2));
    p = vec2_add(p, vec2_scalarMult(v3, fit->coeffs[max_err_i].B3));
    path_addNode(dbg, max_err_d, 0);
    path_addNode(dbg, p, 0);

    //printf("{%f, %f}, {%f, %f},\n", v1.x, v1.y, v2.x, v2.y);
    fit_addToNewPath(fit, v1, -1);
    fit_addToNewPath(fit, v2, -1);
    fit_addToNewPath(fit, v3, i_end);
}
