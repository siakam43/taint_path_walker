extern int vendor_crypto_sign(int v);   /* 实现在项目外，无源码 */

int entry_ext(int v)
{
    return vendor_crypto_sign(v);       /* 污点传到项目外 → 无法追踪 */
}
