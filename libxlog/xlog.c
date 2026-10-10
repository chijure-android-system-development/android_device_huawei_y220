/*
 * MTK xlog. Stock liblog and libcutils export __xlog_buf_printf.
 * libpalwlan_mtk.so needs the liblog copy. Same layout as cutils/xlog.h
 * (tag, format, priority). bufid selects the Android log buffer.
 */
#include <stdio.h>
#include <stdarg.h>

#include <cutils/log.h>

struct xlog_record {
    const char *tag_str;
    const char *fmt_str;
    int prio;
};

int __xlog_buf_printf(int bufid, const struct xlog_record *rec, ...)
{
    char buf[1024];
    va_list ap;
    const char *tag;

    if (rec == NULL || rec->fmt_str == NULL)
        return 0;

    va_start(ap, rec);
    vsnprintf(buf, sizeof(buf), rec->fmt_str, ap);
    va_end(ap);

    tag = rec->tag_str != NULL ? rec->tag_str : "xlog";
    return __android_log_buf_write(bufid, rec->prio, tag, buf);
}
