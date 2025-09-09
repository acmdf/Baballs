/**
 * @file mjpeg_stream.h
 * @brief Simple HTTP MJPEG stream decoder with minimal dependencies
 *
 * This header provides functions to connect to an HTTP MJPEG stream
 * and decode frames using libjpeg-turbo. It uses standard socket APIs
 * (Winsock on Windows, POSIX sockets on Linux/Unix).
 */

 #ifndef MJPEG_STREAM_H
 #define MJPEG_STREAM_H
 
 #ifdef __cplusplus
 extern "C" {
 #endif
 
     /**
     * @brief Opaque handle for an MJPEG stream
     */
     typedef struct MJPEGStream MJPEGStream;
 
     /**
     * @brief Connect to an MJPEG stream over HTTP
     *
     * @param url The URL of the MJPEG stream (must start with "http://")
     * @return MJPEGStream* Handle to the stream, or NULL if connection failed
     */
     MJPEGStream* GetStreamHandle(const char* url);
 
     /**
     * @brief Decode a single frame from the MJPEG stream
     *
     * @param stream The stream handle obtained from GetStreamHandle()
     * @param width Pointer to an int that will receive the frame width
     * @param height Pointer to an int that will receive the frame height
     * @return int* Array of RGBA pixel data (4 bytes per pixel), or NULL on error.
     *              The caller is responsible for freeing this memory with free().
     */
     unsigned char* DecodeFrame(MJPEGStream* stream, int* width, int* height, uint64_t* timestamp, size_t* return_size);
 
     /**
     * @brief Close the stream and free all associated resources
     *
     * @param stream The stream handle to close
     */
     void CloseStream(MJPEGStream* stream);
 
 #ifdef __cplusplus
 }
 #endif
 
 #endif /* MJPEG_STREAM_H */