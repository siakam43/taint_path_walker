#ifndef VTABLE_H
#define VTABLE_H

namespace isp { namespace hal {

class BusBase {
public:
    virtual int transfer(const char *data, unsigned int len);
    virtual ~BusBase() {}
};

class SpiBus : public BusBase {
public:
    int transfer(const char *data, unsigned int len) override;
};

class I2cBus : public BusBase {
public:
    int transfer(const char *data, unsigned int len) override;
};

class UartBus : public BusBase {
public:
    int transfer(const char *data, unsigned int len) override;
};

int bus_send(BusBase *bus, const char *data, unsigned int len);
int bus_send_static(const char *data, unsigned int len);

} }

#endif
