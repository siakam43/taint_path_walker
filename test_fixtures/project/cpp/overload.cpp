namespace isp { namespace util {

int convert(int v);
int convert(const char *s);
int convert(int v, int scale);

int convert(int v)
{
    return v * 2;
}

int convert(const char *s)
{
    return (int)s[0];
}

int convert(int v, int scale)
{
    return v * scale;
}

int entry_overload(const char *s, unsigned int n)
{
    int a = convert(s);            /* 重载: const char* */
    int b = convert((int)n);       /* 重载: int */
    int c = convert((int)n, a);    /* 重载: (int, int) */
    return a + b + c;
}

} }
