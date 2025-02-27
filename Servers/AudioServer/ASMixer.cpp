#include <AK/BufferStream.h>
#include <AudioServer/ASClientConnection.h>
#include <AudioServer/ASMixer.h>
#include <LibCore/CThread.h>
#include <limits>

ASMixer::ASMixer()
    : m_device("/dev/audio")
{
    if (!m_device.open(CIODevice::WriteOnly)) {
        dbgprintf("Can't open audio device: %s\n", m_device.error_string());
        return;
    }

    CThread sound_thread([](void* context) -> int {
        ASMixer* mixer = (ASMixer*)context;
        mixer->mix();
        return 0;
    },
        this);
}

NonnullRefPtr<ASBufferQueue> ASMixer::create_queue(ASClientConnection& client)
{
    LOCKER(m_lock);
    auto queue = adopt(*new ASBufferQueue(client));
    m_pending_mixing.append(*queue);
    return queue;
}

void ASMixer::mix()
{
    decltype(m_pending_mixing) active_mix_queues;

    for (;;) {
        {
            LOCKER(m_lock);
            active_mix_queues.append(move(m_pending_mixing));
        }

        // ### use a wakeup of some kind rather than this garbage
        if (active_mix_queues.size() == 0) {
            // nothing to mix yet
            usleep(10000);
            continue;
        }

        ASample mixed_buffer[1024];
        auto mixed_buffer_length = (int)(sizeof(mixed_buffer) / sizeof(ASample));

        // Mix the buffers together into the output
        for (auto& queue : active_mix_queues) {
            if (!queue->client()) {
                queue->clear();
                continue;
            }

            for (int i = 0; i < mixed_buffer_length; ++i) {
                auto& mixed_sample = mixed_buffer[i];
                ASample sample;
                if (!queue->get_next_sample(sample))
                    break;
                mixed_sample += sample;
            }
        }

        // output the mixed stuff to the device
        u8 raw_buffer[4096];
        auto buffer = ByteBuffer::wrap(raw_buffer, sizeof(raw_buffer));
        BufferStream stream(buffer);

        for (int i = 0; i < mixed_buffer_length; ++i) {
            auto& mixed_sample = mixed_buffer[i];

            mixed_sample.scale(m_main_volume);
            mixed_sample.clip();

            i16 out_sample;
            out_sample = mixed_sample.left * std::numeric_limits<i16>::max();
            stream << out_sample;

            ASSERT(!stream.at_end()); // we should have enough space for both channels in one buffer!
            out_sample = mixed_sample.right * std::numeric_limits<i16>::max();
            stream << out_sample;
        }

        if (stream.offset() != 0) {
            buffer.trim(stream.offset());
            m_device.write(buffer);
        }
    }
}

ASBufferQueue::ASBufferQueue(ASClientConnection& client)
    : m_client(client.make_weak_ptr())
{
}

void ASBufferQueue::enqueue(NonnullRefPtr<ABuffer>&& buffer)
{
    m_queue.enqueue(move(buffer));
}
