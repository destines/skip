/**
 ******************************************************************************
 * @Date         : 2024-02-25 03:44:32
 * @LastEditors  : xis 2684219495@qq.com
 * @LastEditTime : 2024-02-25 03:47:02
 * @FilePath     : /CodePlayers/skip.c
 * @Description  : http downloader
 ******************************************************************************
 **/
#include "skip.h"

int main(int argc, char *const argv[])
{
    // 2.命令行选项解析函数(C语言)：getopt()和getopt_long()
    skip::maps headers;
    headers["User-Agent"] = "Mozilla/5.0 (Windows NT 6.1; WOW64)"
                            " AppleWebKit/537.36 (KHTML, like Gecko)"
                            " Chrome/63.0.3239.132 Safari/537.36 QIHU 360SE";
    skip::Skip skip;
    skip::Data *pdata = skip.get(argv[1], &headers, NULL);

    FILE *fp = fopen(argv[2], "wb");
    if (fp)
    {
        if (pdata->ptr)
            fwrite(pdata->ptr, pdata->size, 1, fp);
    }
    fclose(fp);
    return 0;
}