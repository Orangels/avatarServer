#include "recorder.h"
#include "Misc.h"
#include "StringUtil.h"
#include <string.h>

Recorder::Recorder(bool use_ringbuffer)
{
    use_ringbuffer_ = use_ringbuffer;
    num_lost_samples_ = 0;
    min_read_samples_ = 0;
    num_lost_samples_ = 0;
    pa_stream_ = nullptr;
}

Recorder::~Recorder()
{
    if (pa_stream_)
    {
        Pa_StopStream(pa_stream_);
        Pa_CloseStream(pa_stream_);
        pa_stream_ = nullptr;
    }
}

bool Recorder::Open(int id, int sample_rate, int channels, int bits_per_sample)
{
    num_lost_samples_ = 0;
    min_read_samples_ = sample_rate * 0.1;

    if (use_ringbuffer_)
    {
        // Allocates ring buffer memory.
        int ringbuffer_size = 16384;

        // Initializes ring buffer.
        if (-1 == ringbuffer_.Initialize(bits_per_sample / 8, ringbuffer_size))
        {
            logger::Log(L"Initialize ring buffer failed.");
            return false;
        }
    }

    const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(id);;

    PaSampleFormat sampleFormat = 0;
    if (bits_per_sample == 8)
        sampleFormat = paInt8;
    else if (bits_per_sample == 16)
        sampleFormat = paInt16;
    else if (bits_per_sample == 32)
        sampleFormat = paInt32;
    else
    {
        logger::Log(L"Unsupported BitsPerSample: %d", bits_per_sample);
        return false;
    }

    PaStreamParameters inputParameters = {0};
    inputParameters.device = id;
    inputParameters.channelCount = channels;
    inputParameters.sampleFormat = sampleFormat;
    inputParameters.suggestedLatency = deviceInfo->defaultLowInputLatency;

    PaError err = paNoError;
    if (use_ringbuffer_)
        err = Pa_OpenStream(&pa_stream_, &inputParameters, nullptr, sample_rate, paFramesPerBufferUnspecified, paNoFlag, PortAudioCallback, this);
    else
        err = Pa_OpenStream(&pa_stream_, &inputParameters, nullptr, sample_rate, 640, paNoFlag, nullptr, nullptr);
    if (err)
    {
        logger::Log(L"Can not open the audio stream! %s", util::Utf8ToUnicode(Pa_GetErrorText(err)).c_str());
        return false;
    }

    err = Pa_StartStream(pa_stream_);
    if (err)
    {
        logger::Log(L"Fail to start PortAudio stream! %s", util::Utf8ToUnicode(Pa_GetErrorText(err)).c_str());
        return false;
    }
    return true;
}

void Recorder::Close()
{
    if (pa_stream_)
    {
        Pa_StopStream(pa_stream_);
        Pa_CloseStream(pa_stream_);
        pa_stream_ = nullptr;
    }
}

void Recorder::Read(std::vector<unsigned char> *data)
{
    if (!use_ringbuffer_)
    {
        data->resize(1280);
        Pa_ReadStream(pa_stream_, data->data(), 640);
        return;
    }

    // Checks ring buffer overflow.
    if (num_lost_samples_ > 0)
    {
        logger::Log(L"Lost %d samples due to ring buffer overflow.", num_lost_samples_);
        num_lost_samples_ = 0;
    }

    ring_buffer_size_t num_available_samples = 0;
    while (true)
    {
        num_available_samples = ringbuffer_.GetReadAvailable();
        if (num_available_samples >= min_read_samples_)
        {
            break;
        }
        Pa_Sleep(5);
    }

    // Reads data.
    num_available_samples = ringbuffer_.GetReadAvailable();
    data->resize(num_available_samples * 2);
    ring_buffer_size_t num_read_samples = ringbuffer_.Read(data->data(), num_available_samples);
    if (num_read_samples != num_available_samples)
    {
        logger::Log(L"%d samples were available, but only %d samples were read.",
            num_available_samples, num_read_samples);
    }
}

int Recorder::PortAudioCallback(const void *input,
                                void *output,
                                unsigned long frame_count,
                                const PaStreamCallbackTimeInfo *time_info,
                                PaStreamCallbackFlags status_flags,
                                void *user_data)
{
    Recorder *self = reinterpret_cast<Recorder *>(user_data);
    self->Callback(input, output, frame_count, time_info, status_flags);
    return paContinue;
}

int Recorder::Callback(const void *input, void *output,
                       unsigned long frame_count,
                       const PaStreamCallbackTimeInfo *time_info,
                       PaStreamCallbackFlags status_flags)
{
    // Input audio.
    ring_buffer_size_t num_written_samples = ringbuffer_.Write(input, frame_count);
    num_lost_samples_ += frame_count - num_written_samples;
    return paContinue;
}
