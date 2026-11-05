// GmSocket / GmPlug / GmConnection / GmIterator / GmSocketIterator — recovered types.
//
// Reference build: knd-xbox-usa-5454008a-v1 (docs/provenance.md).
// Evidence classes:
//   ORIGINAL  GmSocket, GmPlug, GmIterator, GmSocketIterator: class names from MSVC RTTI type descriptors.
//   OBSERVED  field offsets, vftable contents and slot order: instructions and vftables cited below.
//   INFERRED  GmConnection (no RTTI name survives) and every method/member name.
//
// Model: a GmConnection links one GmSocket to one GmPlug. A plug keeps a doubly linked list of its connections
// (GmPlug::m_connections, GmConnection::m_plugNext/m_plugPrev), so destroying either end severs the link.
#pragma once

class GmSocket;
class GmPlug;

// vftable 0x002793D0: [0] 0x0007CEB0 scalar deleting dtor, [1] pure.
class GmConnection {
public:
    GmConnection();                               // 0x0007CE30
    virtual ~GmConnection();                      // 0x0007CE50
    virtual void Disconnect() = 0;                // slot 1
    void Connect(GmSocket* socket, GmPlug* plug); // 0x0007CE70
    void Unlink();                                // 0x0007CDF0 removes this from the plug's list only

    GmConnection* m_plugNext;  // +0x04
    GmConnection* m_plugPrev;  // +0x08
    GmSocket* m_socket;        // +0x0C
    GmPlug* m_plug;            // +0x10
    int m_active;              // +0x14
};

// vftable 0x0027855C: [0] 0x00076CE0 scalar deleting dtor. Base of every item a socket can hold.
class GmPlug {
public:
    GmPlug();                              // 0x00076C10
    virtual ~GmPlug();                     // 0x00076C50
    void DisconnectAll();                  // 0x00076C20
    void Disconnect(GmSocket* socket);     // 0x00076C80 severs the first connection to `socket` only
    int IsConnectedTo(GmSocket* socket);   // 0x00076CB0

    GmConnection* m_connections;           // +0x04
};

// vftable 0x00278418 (abstract): [0] pure dtor, [1] pure, [2] 0x00075FF0, [3] 0x00075FE0, [4] pure.
class GmSocket {
public:
    explicit GmSocket(void* context);      // 0x00075FB0
    virtual ~GmSocket() = 0;               // 0x00075FD0 (body exists; slot 0 is __purecall)
    virtual void Append(GmPlug* item) = 0; // slot 1
    virtual void Remove(GmPlug* item);     // slot 2  0x00075FF0
    virtual int Contains(GmPlug* item);    // slot 3  0x00075FE0
    virtual int IsEmpty() = 0;             // slot 4
    void SetContext(void* context);        // 0x000DF720 (address shared with unrelated code: linker folding)

    void* m_context;                       // +0x04 role unknown; constructor argument, cleared by ~GmLinkedList
};

// vftable 0x0027842C (abstract): [0] 0x00076000 scalar deleting dtor, [1..9] pure.
class GmIterator {
public:
    virtual ~GmIterator();                 // 0x00076040
    virtual void First() = 0;              // slot 1
    virtual void Last() = 0;               // slot 2
    virtual void Next() = 0;               // slot 3
    virtual void Prev() = 0;               // slot 4
    virtual void* NextData() = 0;          // slot 5
    virtual void* PrevData() = 0;          // slot 6
    virtual void* Current() = 0;           // slot 7
    virtual void* At(int index) = 0;       // slot 8
    virtual int Count() = 0;               // slot 9
};

// vftable 0x00278454 (abstract): [0] 0x00076050 scalar deleting dtor, [1..9] pure. The destructor is implicit: its
// vptr store is dead and optimised away, which is why derived destructors show only two vptr stores.
class GmSocketIterator : public GmIterator {
public:
    explicit GmSocketIterator(GmSocket* socket);  // 0x00076020

    GmSocket* m_socket;                    // +0x04
};
