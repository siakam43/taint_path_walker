namespace isp { namespace util {

template <typename T>
class RingBuf {
public:
    void put(T v) { m_v = v; }
    T get(void) const { return m_v; }
private:
    T m_v;
};

template <typename T>
T clamp_val(T v) { return v > (T)0 ? v : (T)0; }

int entry_template(int raw)
{
    RingBuf<int> rb;
    rb.put(raw);                  /* 模板成员 put */
    int c = clamp_val(raw);       /* 函数模板 */
    return rb.get() + c;          /* 模板成员 get */
}

} }
