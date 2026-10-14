namespace isp { namespace hal {

class TimerBase {
public:
    virtual int on_tick(unsigned int v) { return (int)v; }   /* 基类默认实现 */
    virtual ~TimerBase() {}
};

class CtrlBase {
public:
    virtual int on_ctrl(unsigned int v) { return (int)v; }   /* 基类默认实现 */
    virtual ~CtrlBase() {}
};

class DualUnit : public TimerBase, public CtrlBase {
public:
    int on_tick(unsigned int v) override;
    int on_ctrl(unsigned int v) override;
};

int DualUnit::on_tick(unsigned int v)
{
    return (int)(v * 3);
}

int DualUnit::on_ctrl(unsigned int v)
{
    return (int)(v + 7);
}

int entry_mi(CtrlBase *c, unsigned int v)
{
    return c->on_ctrl(v);        /* 分散点：候选 = DualUnit::on_ctrl + CtrlBase::on_ctrl */
}

} }
