/* Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in) */
/* SPDX-License-Identifier: 0BSD */
#ifndef SERIALIZER_SWIFT_PROVIDER_H
#define SERIALIZER_SWIFT_PROVIDER_H
#include <stddef.h>
#include <stdint.h>
struct serializer_swift_provider;
/* Create one exclusive standard-frame context; all pointers and buffers remain caller-owned. */
struct serializer_swift_provider* serializer_swift_provider_create(int format, int encoding, int level, size_t size, int window_log);
/* Report exact bounded progress; 1 means finished, 0 means progress, negative values mean failure. */
int serializer_swift_provider_step(struct serializer_swift_provider* state, const uint8_t* input, size_t* consumed,
                                 uint8_t* output, size_t* produced, int finish);
/* Release native contexts and private workspace without publishing output. */
void serializer_swift_provider_close(struct serializer_swift_provider* state);
/* Return the selected native Zstandard encoder's supported level endpoints. */
int serializer_swift_provider_minimum_level(void);
int serializer_swift_provider_maximum_level(void);
/* Query the selected LZ4 frame encoder rather than duplicating its maximum level. */
int serializer_swift_provider_lz4_maximum_level(void);
/* Report the explicitly linked native provider versions. */
const char* serializer_swift_provider_zstd_version(void);
const char* serializer_swift_provider_lz4_version(void);
#endif
