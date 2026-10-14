int m_leaf_c(int v)
{
    return v;
}

int m_b(int v)
{
    return m_leaf_c(v);
}

int m_entry(int v)
{
    int r = m_b(v);        /* 边 E->B */
    r += m_leaf_c(v);      /* 边 E->C（直调） */
    return r;
}
