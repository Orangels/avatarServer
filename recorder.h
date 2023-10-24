#ifndef RECORDER_H
#define RECORDER_H

#include <portaudio.h>
#include <iostream>
#include <vector>
#include "ring_buffer.h"

class Recorder
{
    bool use_ringbuffer_;

    // Ring buffer wrapper.
    RingBuffer ringbuffer_;

    // Pointer to PortAudio stream.
    PaStream *pa_stream_;

    // Number of lost samples at each Read() due to ring buffer overflow.
    int num_lost_samples_;

    // Wait for this number of samples in each Read() call.
    int min_read_samples_;

public:
    Recorder(bool use_ringbuffer = true);
    ~Recorder();

    bool Open(int id, int sample_rate, int channels, int bits_per_sample);
    void Close();
    void Read(std::vector<unsigned char> *data);

private:
    static int PortAudioCallback(const void *input,
                                 void *output,
                                 unsigned long frame_count,
                                 const PaStreamCallbackTimeInfo *time_info,
                                 PaStreamCallbackFlags status_flags,
                                 void *user_data);

    int Callback(const void *input, void *output,
                 unsigned long frame_count,
                 const PaStreamCallbackTimeInfo *time_info,
                 PaStreamCallbackFlags status_flags);
};

#endif
