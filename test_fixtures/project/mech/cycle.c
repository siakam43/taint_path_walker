int cyc_a(int v);

int cyc_b(int v)
{
    return cyc_a(v + 1);     /* B -> A */
}

int cyc_a(int v)
{
    return cyc_b(v - 1);     /* A -> B：环 */
}

int entry_cycle(int v)
{
    return cyc_a(v);         /* E -> A -> B -> (A 重复，环终止) */
}
