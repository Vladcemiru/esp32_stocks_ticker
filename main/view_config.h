#ifndef STOCKS_VIEW_CONFIG_H
#define STOCKS_VIEW_CONFIG_H

/*
 * Display mode — change the value and rebuild.
 * SLIDES: rotate one stock every few seconds + Fear & Greed slide.
 * HSCROLL: title line + horizontal scroll of all tickers (price, change).
 * VSLOW: slow vertical scroll of the full list (+ F&G).
 */
#define STOCKS_VIEW_SLIDES   0
#define STOCKS_VIEW_HSCROLL  1
#define STOCKS_VIEW_VSLOW    2

#ifndef STOCKS_DISPLAY_VIEW
#define STOCKS_DISPLAY_VIEW STOCKS_VIEW_SLIDES
#endif

/* Horizontal scroll: delay between steps (ms), duration of one full text cycle (ms) */
#define VIEW_HSCROLL_STEP_MS    280
#define VIEW_HSCROLL_CYCLE_MS   45000

/* Vertical scroll: delay between lines (ms) */
#define VIEW_VSLOW_LINE_MS      900

#endif
