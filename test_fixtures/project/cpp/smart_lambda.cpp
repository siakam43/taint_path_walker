#include <memory>

namespace isp { namespace hal {

struct Pkt {
    unsigned int len;
    char payload[32];
};

class Sink {
public:
    virtual int feed(const char *p, unsigned int n);
    virtual ~Sink() {}
};

class LogSink : public Sink {
public:
    int feed(const char *p, unsigned int n) override;
};

int Sink::feed(const char *p, unsigned int n)
{
    (void)p;
    return (int)n;
}

int LogSink::feed(const char *p, unsigned int n)
{
    return (int)(p[0] + n);
}

int entry_smart(const char *data, unsigned int len)
{
    std::shared_ptr<Sink> s = std::make_shared<LogSink>();  /* 构造来源可确定 → LogSink */
    std::shared_ptr<Pkt> pkt = std::make_shared<Pkt>();
    pkt->len = len;                       /* operator-> 穿透：成员赋值 */
    unsigned int n = len > 31 ? 31 : len;
    for (unsigned int i = 0; i < n; i++) {
        pkt->payload[i] = data[i];
    }
    return s->feed(pkt->payload, pkt->len);   /* 虚调用，静态可确定 */
}

int entry_lambda(const char *data, unsigned int len)
{
    auto f = [data, len](unsigned int i) {
        return (int)(data[i % len] + len);    /* lambda 体: 污点经捕获进入 */
    };
    return f(1);
}

} }
