#include <string.h>

struct hal_req {
    unsigned int cmd;
    unsigned int size;
    char data[64];
};

extern int shm_read_block(void *dst, unsigned int len);  /* 信道读取（项目外实现） */

static int hal_process_cmd(const char *p, unsigned int n)
{
    return (int)(p[0] + n);
}

int hal_poll_shared(void)
{
    struct hal_req req;
    if (shm_read_block(&req, sizeof(req)) != 0) {
        return -1;                       /* 信道读取: req 成为初始污点 */
    }
    return hal_process_cmd(req.data, req.size);
}

int hal_dispatch(void *ctx, struct hal_req *req)
{
    (void)ctx;                           /* ctx: 框架上下文指针，仅透传 */
    if (req->size > sizeof(((struct hal_req *)0)->data)) {
        return -1;                       /* 校验：长度上限 */
    }
    return hal_process_cmd(req->data, req->size);
}
