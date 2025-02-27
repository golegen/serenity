#pragma once

#include <AK/HashMap.h>
#include <AK/RefPtr.h>
#include <AK/RefCounted.h>
#include <Kernel/VM/PhysicalPage.h>
#include <Kernel/VM/RangeAllocator.h>

class PageDirectory : public RefCounted<PageDirectory> {
    friend class MemoryManager;

public:
    static NonnullRefPtr<PageDirectory> create_for_userspace(const RangeAllocator* parent_range_allocator = nullptr) { return adopt(*new PageDirectory(parent_range_allocator)); }
    static NonnullRefPtr<PageDirectory> create_at_fixed_address(PhysicalAddress paddr) { return adopt(*new PageDirectory(paddr)); }
    ~PageDirectory();

    u32 cr3() const { return m_directory_page->paddr().get(); }
    PageDirectoryEntry* entries() { return reinterpret_cast<PageDirectoryEntry*>(cr3()); }

    void flush(VirtualAddress);

    RangeAllocator& range_allocator() { return m_range_allocator; }

private:
    explicit PageDirectory(const RangeAllocator* parent_range_allocator);
    explicit PageDirectory(PhysicalAddress);

    RangeAllocator m_range_allocator;
    RefPtr<PhysicalPage> m_directory_page;
    HashMap<unsigned, RefPtr<PhysicalPage>> m_physical_pages;
};
