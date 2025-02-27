#include <Kernel/Devices/BXVGADevice.h>
#include <Kernel/IO.h>
#include <Kernel/PCI.h>
#include <Kernel/Process.h>
#include <Kernel/VM/MemoryManager.h>
#include <LibC/errno_numbers.h>

#define VBE_DISPI_IOPORT_INDEX 0x01CE
#define VBE_DISPI_IOPORT_DATA 0x01CF

#define VBE_DISPI_INDEX_ID 0x0
#define VBE_DISPI_INDEX_XRES 0x1
#define VBE_DISPI_INDEX_YRES 0x2
#define VBE_DISPI_INDEX_BPP 0x3
#define VBE_DISPI_INDEX_ENABLE 0x4
#define VBE_DISPI_INDEX_BANK 0x5
#define VBE_DISPI_INDEX_VIRT_WIDTH 0x6
#define VBE_DISPI_INDEX_VIRT_HEIGHT 0x7
#define VBE_DISPI_INDEX_X_OFFSET 0x8
#define VBE_DISPI_INDEX_Y_OFFSET 0x9
#define VBE_DISPI_DISABLED 0x00
#define VBE_DISPI_ENABLED 0x01
#define VBE_DISPI_LFB_ENABLED 0x40

#define BXVGA_DEV_IOCTL_SET_Y_OFFSET 1982
#define BXVGA_DEV_IOCTL_SET_RESOLUTION 1985
struct BXVGAResolution {
    int width;
    int height;
};

static BXVGADevice* s_the;

BXVGADevice& BXVGADevice::the()
{
    return *s_the;
}

BXVGADevice::BXVGADevice()
    : BlockDevice(82, 413)
{
    s_the = this;
    m_framebuffer_address = PhysicalAddress(find_framebuffer_address());
}

void BXVGADevice::set_register(u16 index, u16 data)
{
    IO::out16(VBE_DISPI_IOPORT_INDEX, index);
    IO::out16(VBE_DISPI_IOPORT_DATA, data);
}

void BXVGADevice::set_resolution(int width, int height)
{
    m_framebuffer_width = width;
    m_framebuffer_height = height;

    set_register(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    set_register(VBE_DISPI_INDEX_XRES, (u16)width);
    set_register(VBE_DISPI_INDEX_YRES, (u16)height);
    set_register(VBE_DISPI_INDEX_VIRT_WIDTH, (u16)width);
    set_register(VBE_DISPI_INDEX_VIRT_HEIGHT, (u16)height * 2);
    set_register(VBE_DISPI_INDEX_BPP, 32);
    set_register(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
    set_register(VBE_DISPI_INDEX_BANK, 0);
}

void BXVGADevice::set_y_offset(int offset)
{
    ASSERT(offset <= m_framebuffer_height);
    set_register(VBE_DISPI_INDEX_Y_OFFSET, (u16)offset);
}

u32 BXVGADevice::find_framebuffer_address()
{
    // NOTE: The QEMU card has the same PCI ID as the Bochs one.
    static const PCI::ID bochs_vga_id = { 0x1234, 0x1111 };
    static const PCI::ID virtualbox_vga_id = { 0x80ee, 0xbeef };
    u32 framebuffer_address = 0;
    PCI::enumerate_all([&framebuffer_address](const PCI::Address& address, PCI::ID id) {
        if (id == bochs_vga_id || id == virtualbox_vga_id) {
            framebuffer_address = PCI::get_BAR0(address) & 0xfffffff0;
            kprintf("BXVGA: framebuffer @ P%x\n", framebuffer_address);
        }
    });
    return framebuffer_address;
}

KResultOr<Region*> BXVGADevice::mmap(Process& process, FileDescription&, VirtualAddress preferred_vaddr, size_t offset, size_t size, int prot)
{
    ASSERT(offset == 0);
    ASSERT(size == framebuffer_size_in_bytes());
    auto vmo = VMObject::create_for_physical_range(framebuffer_address(), framebuffer_size_in_bytes());
    auto* region = process.allocate_region_with_vmo(
        preferred_vaddr,
        framebuffer_size_in_bytes(),
        move(vmo),
        0,
        "BXVGA Framebuffer",
        prot);
    kprintf("BXVGA: %s(%u) created Region{%p} with size %u for framebuffer P%x with vaddr L%x\n",
        process.name().characters(), process.pid(),
        region, region->size(), framebuffer_address().as_ptr(), region->vaddr().get());
    ASSERT(region);
    return region;
}

int BXVGADevice::ioctl(FileDescription&, unsigned request, unsigned arg)
{
    switch (request) {
    case BXVGA_DEV_IOCTL_SET_Y_OFFSET:
        if (arg > (unsigned)m_framebuffer_height * 2)
            return -EINVAL;
        set_y_offset((int)arg);
        return 0;
    case BXVGA_DEV_IOCTL_SET_RESOLUTION: {
        auto* resolution = (const BXVGAResolution*)arg;
        if (!current->process().validate_read_typed(resolution))
            return -EFAULT;
        set_resolution(resolution->width, resolution->height);
        return 0;
    }
    default:
        return -EINVAL;
    };
}

bool BXVGADevice::can_read(FileDescription&) const
{
    ASSERT_NOT_REACHED();
}

bool BXVGADevice::can_write(FileDescription&) const
{
    ASSERT_NOT_REACHED();
}

ssize_t BXVGADevice::read(FileDescription&, u8*, ssize_t)
{
    ASSERT_NOT_REACHED();
}

ssize_t BXVGADevice::write(FileDescription&, const u8*, ssize_t)
{
    ASSERT_NOT_REACHED();
}
