/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <aster_imports.h>
#include <stdint.h>
void tyrian_yield(uint32_t delay_ms);
uint32_t tyrian_ticks(void);
void tyrian_input(const aster_event_t *event);
void tyrian_input_clear(void);
void tyrian_overlay(uint16_t *pixels, unsigned pitch, unsigned w, unsigned h);
int tyrian_video_init(void);
int tyrian_video_present(void);
void tyrian_video_invalidate(void);
void tyrian_video_destroy(void);
void tyrian_audio_tick(void);
void tyrian_audio_destroy(void);
uint32_t tyrian_heap_used(void);
uint32_t tyrian_heap_peak(void);
__attribute__((noreturn)) void tyrian_exit(int code);
