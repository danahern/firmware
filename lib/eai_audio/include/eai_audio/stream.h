/*
 * eai_audio stream lifecycle
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_AUDIO_STREAM_H
#define EAI_AUDIO_STREAM_H

#include <eai_audio/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Open a stream on a port.
 *
 * @param stream   Stream to initialize (caller-allocated).
 * @param port_id  Target port ID.
 * @param config   Desired stream configuration.
 * @return 0 on success, -EINVAL if args invalid, -ENODEV if port not found,
 *         -EBUSY if port already has an active stream (non-mixer mode).
 */
int eai_audio_stream_open(struct eai_audio_stream *stream, uint8_t port_id,
			  const struct eai_audio_config *config);

/**
 * Close a stream and release resources.
 *
 * @param stream  Stream to close.
 * @return 0 on success, -EINVAL if stream is NULL.
 */
int eai_audio_stream_close(struct eai_audio_stream *stream);

/**
 * Start audio playback or capture on a stream.
 *
 * @param stream  Opened stream.
 * @return 0 on success, -EINVAL if stream is NULL or not opened.
 */
int eai_audio_stream_start(struct eai_audio_stream *stream);

/**
 * Pause audio playback or capture.
 *
 * @param stream  Started stream.
 * @return 0 on success, -EINVAL if stream is NULL.
 */
int eai_audio_stream_pause(struct eai_audio_stream *stream);

/**
 * Write audio data to an output stream.
 *
 * @param stream      Output stream.
 * @param data        Audio data (format per stream config).
 * @param frames      Number of frames to write.
 * @param timeout_ms  Maximum wait time (0 = non-blocking).
 * @return Number of frames written on success, negative errno on error.
 *         -EINVAL if args invalid, -ENOTSUP if stream is input.
 */
int eai_audio_stream_write(struct eai_audio_stream *stream,
			   const void *data, uint32_t frames,
			   uint32_t timeout_ms);

/**
 * Read audio data from an input stream.
 *
 * @param stream      Input stream.
 * @param data        Buffer for audio data.
 * @param frames      Number of frames to read.
 * @param timeout_ms  Maximum wait time (0 = non-blocking).
 * @return Number of frames read on success, negative errno on error.
 *         -EINVAL if args invalid, -ENOTSUP if stream is output.
 */
int eai_audio_stream_read(struct eai_audio_stream *stream,
			  void *data, uint32_t frames,
			  uint32_t timeout_ms);

/**
 * Get the current stream position in frames.
 *
 * @param stream  Active stream.
 * @param frames  Output frame count.
 * @return 0 on success, -EINVAL if args invalid.
 */
int eai_audio_stream_get_position(struct eai_audio_stream *stream,
				  uint64_t *frames);

#ifdef __cplusplus
}
#endif

#endif /* EAI_AUDIO_STREAM_H */
