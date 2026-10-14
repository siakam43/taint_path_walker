#include "vtable.h"
#include <string.h>

namespace isp { namespace hal {

static int g_bus_sink;

int BusBase::transfer(const char *data, unsigned int len)
{
    (void)data;
    g_bus_sink += (int)len;          /* 基类默认实现 */
    return 0;
}

int SpiBus::transfer(const char *data, unsigned int len)
{
    char tmp[32];
    unsigned int n = len > 31 ? 31 : len;   /* 校验 */
    memcpy(tmp, data, n);                    /* data -> tmp */
    g_bus_sink += tmp[0];
    return (int)n;
}

int I2cBus::transfer(const char *data, unsigned int len)
{
    g_bus_sink += (int)(len + (unsigned int)data[0]);
    return 0;
}

int UartBus::transfer(const char *data, unsigned int len)
{
    g_bus_sink += (int)((unsigned int)data[0] * 2);
    (void)len;
    return 0;
}

int bus_send(BusBase *bus, const char *data, unsigned int len)
{
    return bus->transfer(data, len);   /* 分发点：不可静态确定 → 枚举全部覆写 + 基类默认实现 */
}

int bus_send_static(const char *data, unsigned int len)
{
    SpiBus spi;                        /* 局部具体对象 → 静态可确定 */
    return spi.transfer(data, len);    /* 仅解析 SpiBus::transfer */
}

} }
