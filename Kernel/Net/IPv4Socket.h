#pragma once

#include <AK/HashMap.h>
#include <AK/SinglyLinkedList.h>
#include <Kernel/DoubleBuffer.h>
#include <Kernel/Lock.h>
#include <Kernel/Net/IPv4.h>
#include <Kernel/Net/Socket.h>

class IPv4SocketHandle;
class TCPSocketHandle;
class NetworkAdapter;
class TCPPacket;
class TCPSocket;

class IPv4Socket : public Socket {
public:
    static NonnullRefPtr<IPv4Socket> create(int type, int protocol);
    virtual ~IPv4Socket() override;

    static Lockable<HashTable<IPv4Socket*>>& all_sockets();

    virtual KResult bind(const sockaddr*, socklen_t) override;
    virtual KResult connect(FileDescription&, const sockaddr*, socklen_t, ShouldBlock = ShouldBlock::Yes) override;
    virtual bool get_local_address(sockaddr*, socklen_t*) override;
    virtual bool get_peer_address(sockaddr*, socklen_t*) override;
    virtual void attach(FileDescription&) override;
    virtual void detach(FileDescription&) override;
    virtual bool can_read(FileDescription&) const override;
    virtual ssize_t read(FileDescription&, u8*, ssize_t) override;
    virtual ssize_t write(FileDescription&, const u8*, ssize_t) override;
    virtual bool can_write(FileDescription&) const override;
    virtual ssize_t sendto(FileDescription&, const void*, size_t, int, const sockaddr*, socklen_t) override;
    virtual ssize_t recvfrom(FileDescription&, void*, size_t, int flags, sockaddr*, socklen_t*) override;

    void did_receive(const IPv4Address& peer_address, u16 peer_port, ByteBuffer&&);

    const IPv4Address& local_address() const;
    u16 local_port() const { return m_local_port; }
    void set_local_port(u16 port) { m_local_port = port; }

    const IPv4Address& peer_address() const { return m_peer_address; }
    u16 peer_port() const { return m_peer_port; }
    void set_peer_port(u16 port) { m_peer_port = port; }

protected:
    IPv4Socket(int type, int protocol);
    virtual const char* class_name() const override { return "IPv4Socket"; }

    int allocate_local_port_if_needed();

    virtual KResult protocol_bind() { return KSuccess; }
    virtual int protocol_receive(const ByteBuffer&, void*, size_t, int) { return -ENOTIMPL; }
    virtual int protocol_send(const void*, int) { return -ENOTIMPL; }
    virtual KResult protocol_connect(FileDescription&, ShouldBlock) { return KSuccess; }
    virtual int protocol_allocate_local_port() { return 0; }
    virtual bool protocol_is_disconnected() const { return false; }

private:
    virtual bool is_ipv4() const override { return true; }

    bool m_bound { false };
    int m_attached_fds { 0 };

    IPv4Address m_local_address;
    IPv4Address m_peer_address;

    DoubleBuffer m_for_client;
    DoubleBuffer m_for_server;

    struct ReceivedPacket {
        IPv4Address peer_address;
        u16 peer_port;
        ByteBuffer data;
    };

    SinglyLinkedList<ReceivedPacket> m_receive_queue;

    u16 m_local_port { 0 };
    u16 m_peer_port { 0 };

    u32 m_bytes_received { 0 };

    bool m_can_read { false };
};

class IPv4SocketHandle : public SocketHandle {
public:
    IPv4SocketHandle() {}

    IPv4SocketHandle(RefPtr<IPv4Socket>&& socket)
        : SocketHandle(move(socket))
    {
    }

    IPv4SocketHandle(IPv4SocketHandle&& other)
        : SocketHandle(move(other))
    {
    }

    IPv4SocketHandle(const IPv4SocketHandle&) = delete;
    IPv4SocketHandle& operator=(const IPv4SocketHandle&) = delete;

    IPv4Socket* operator->() { return &socket(); }
    const IPv4Socket* operator->() const { return &socket(); }

    IPv4Socket& socket() { return static_cast<IPv4Socket&>(SocketHandle::socket()); }
    const IPv4Socket& socket() const { return static_cast<const IPv4Socket&>(SocketHandle::socket()); }
};
