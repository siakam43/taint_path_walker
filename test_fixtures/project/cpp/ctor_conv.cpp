namespace isp { namespace hal {

class Frame {
public:
    Frame(unsigned int id, const char *data, unsigned int len);
    Frame(const Frame &o);
    Frame &operator=(const Frame &o);
    char &operator[](unsigned int i);
    unsigned int m_id;
    char m_data[32];
};

Frame::Frame(unsigned int id, const char *data, unsigned int len)
{
    unsigned int n = len > 31 ? 31 : len;
    m_id = id;
    for (unsigned int i = 0; i < n; i++) {
        m_data[i] = data[i];      /* data/len 污点 -> 成员 */
    }
}

Frame::Frame(const Frame &o)
{
    m_id = o.m_id;
    for (int i = 0; i < 32; i++) {
        m_data[i] = o.m_data[i];
    }
}

Frame &Frame::operator=(const Frame &o)
{
    m_id = o.m_id;
    m_data[0] = o.m_data[0];
    return *this;
}

char &Frame::operator[](unsigned int i)
{
    return m_data[i % 32];        /* i 污点作索引 */
}

int entry_frame(unsigned int id, const char *data, unsigned int len, unsigned int sel)
{
    Frame f(id, data, len);       /* 构造 */
    Frame g = f;                  /* 拷贝构造 */
    Frame h(0, "", 0);
    h = g;                        /* operator= */
    return (int)h[sel];           /* operator[] */
}

class Tag {
public:
    Tag(int v) { m_v = v; }       /* 带参构造（可隐式转换） */
    int m_v;
};

int tag_sink(Tag t)
{
    return t.m_v;
}

int entry_implicit(int raw)
{
    return tag_sink(raw);         /* int -> Tag 隐式转换 + 按值传参 */
}

} }
