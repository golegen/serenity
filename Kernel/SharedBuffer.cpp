#include <Kernel/Process.h>
#include <Kernel/SharedBuffer.h>

Lockable<HashMap<int, NonnullOwnPtr<SharedBuffer>>>& shared_buffers()
{
    static Lockable<HashMap<int, NonnullOwnPtr<SharedBuffer>>>* map;
    if (!map)
        map = new Lockable<HashMap<int, NonnullOwnPtr<SharedBuffer>>>;
    return *map;
}

void SharedBuffer::sanity_check(const char* what)
{
    LOCKER(shared_buffers().lock());

    unsigned found_refs = 0;
    for (const auto& ref : m_refs)
        found_refs += ref.count;

    if (found_refs != m_total_refs) {
        dbgprintf("%s sanity -- SharedBuffer{%p} id: %d has total refs %d but we found %d\n", what, this, m_shared_buffer_id, m_total_refs, found_refs);
        for (const auto& ref : m_refs) {
            dbgprintf("    ref from pid %d: refcnt %d\n", ref.pid, ref.count);
        }
        ASSERT_NOT_REACHED();
    }
}

bool SharedBuffer::is_shared_with(pid_t peer_pid)
{
    LOCKER(shared_buffers().lock());
    if (m_global)
        return true;
    for (auto& ref : m_refs) {
        if (ref.pid == peer_pid) {
            return true;
        }
    }

    return false;
}

void* SharedBuffer::ref_for_process_and_get_address(Process& process)
{
    LOCKER(shared_buffers().lock());
    ASSERT(is_shared_with(process.pid()));
    if (m_global) {
        bool found = false;
        for (auto& ref : m_refs) {
            if (ref.pid == process.pid()) {
                found = true;
                break;
            }
        }
        if (!found)
            m_refs.append(Reference(process.pid()));
    }

    for (auto& ref : m_refs) {
        if (ref.pid == process.pid()) {
            ref.count++;
            m_total_refs++;
            if (ref.region == nullptr) {
                ref.region = process.allocate_region_with_vmo(VirtualAddress(), size(), m_vmo, 0, "SharedBuffer", PROT_READ | (m_writable ? PROT_WRITE : 0));
                ref.region->set_shared(true);
            }
            sanity_check("ref_for_process_and_get_address");
            return ref.region->vaddr().as_ptr();
        }
    }
    ASSERT_NOT_REACHED();
}

void SharedBuffer::share_with(pid_t peer_pid)
{
    LOCKER(shared_buffers().lock());
    for (auto& ref : m_refs) {
        if (ref.pid == peer_pid) {
            // don't increment the reference count yet; let them get_shared_buffer it first.
            sanity_check("share_with (old ref)");
            return;
        }
    }

    m_refs.append(Reference(peer_pid));
    sanity_check("share_with (new ref)");
}

void SharedBuffer::deref_for_process(Process& process)
{
    LOCKER(shared_buffers().lock());
    for (int i = 0; i < m_refs.size(); ++i) {
        auto& ref = m_refs[i];
        if (ref.pid == process.pid()) {
            ref.count--;
            m_total_refs--;
            if (ref.count == 0) {
#ifdef SHARED_BUFFER_DEBUG
                dbgprintf("Releasing shared buffer reference on %d of size %d by PID %d\n", m_shared_buffer_id, size(), process.pid());
#endif
                process.deallocate_region(*ref.region);
                m_refs.remove(i);
#ifdef SHARED_BUFFER_DEBUG
                dbgprintf("Released shared buffer reference on %d of size %d by PID %d\n", m_shared_buffer_id, size(), process.pid());
#endif
                sanity_check("deref_for_process");
                destroy_if_unused();
                return;
            }
            return;
        }
    }

    ASSERT_NOT_REACHED();
}

void SharedBuffer::disown(pid_t pid)
{
    LOCKER(shared_buffers().lock());
    for (int i = 0; i < m_refs.size(); ++i) {
        auto& ref = m_refs[i];
        if (ref.pid == pid) {
#ifdef SHARED_BUFFER_DEBUG
            dbgprintf("Disowning shared buffer %d of size %d by PID %d\n", m_shared_buffer_id, size(), pid);
#endif
            m_total_refs -= ref.count;
            m_refs.remove(i);
#ifdef SHARED_BUFFER_DEBUG
            dbgprintf("Disowned shared buffer %d of size %d by PID %d\n", m_shared_buffer_id, size(), pid);
#endif
            destroy_if_unused();
            return;
        }
    }
}

void SharedBuffer::destroy_if_unused()
{
    LOCKER(shared_buffers().lock());
    sanity_check("destroy_if_unused");
    if (m_total_refs == 0) {
#ifdef SHARED_BUFFER_DEBUG
        kprintf("Destroying unused SharedBuffer{%p} id: %d\n", this, m_shared_buffer_id);
#endif
        auto count_before = shared_buffers().resource().size();
        shared_buffers().resource().remove(m_shared_buffer_id);
        ASSERT(count_before != shared_buffers().resource().size());
    }
}

void SharedBuffer::seal()
{
    LOCKER(shared_buffers().lock());
    m_writable = false;
    for (auto& ref : m_refs) {
        if (ref.region) {
            ref.region->set_writable(false);
            MM.remap_region(*ref.region->page_directory(), *ref.region);
        }
    }
}
