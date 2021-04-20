#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "h264_to_ps.h"

typedef struct
{
    char *h264_data;
    int size;
    int nal_head_pos;
}H264Data_t;


static int h264_get_nal(H264Data_t *h264, H264Nalu_t *nal)
{
    int nal_head_pos = h264->nal_head_pos;
    int file_size = h264->size;
    char *data = h264->h264_data;
    int nal_tail_pos = 0;
    int nal_size = 0;
    int nal_type = 0;
    int start_code = 0;

    while (nal_head_pos < file_size)
    {
        if (data[nal_head_pos++] == 0x00 && data[nal_head_pos++] == 0x00)           /*找起始码*/
        {
            if (data[nal_head_pos] == 0x01)                                         /*起始码 00 00 01*/
            {
                nal_head_pos++;                                                     /*nal_head_pos指向nal type地址*/
                start_code = 3;
                goto get_nal;
            }
            else if (data[nal_head_pos++] == 0x00 && data[nal_head_pos++] == 0x01)  /*起始码00 00 00 01 nal_head_pos指向nal type地址*/
            {
                start_code = 4;
                goto get_nal;
            }
            else
            {
                continue;                                                           /*找到00 00,但后面不对,跳过继续找*/
            }
        }
        else
        {
            continue;                                                               /*00 00都没有找到,跳过继续找*/
        }

get_nal:
        nal_tail_pos = nal_head_pos;
        while (nal_tail_pos < file_size)
        {
            if (data[nal_tail_pos++] == 0x00 && data[nal_tail_pos++] == 0x00)           /*找起始码*/
            {
                if (data[nal_tail_pos] == 0x01)                                         /*起始码 00 00 01*/
                {
                    nal_tail_pos++;                                                     /*nal_tail_pos指向nal type地址*/
                    nal_size = nal_tail_pos - nal_head_pos - 3;
                    nal_tail_pos -= 3;
                    break;
                }
                else if (data[nal_tail_pos++] == 0x00 && data[nal_tail_pos++] == 0x01)  /*起始码00 00 00 01 nal_tail_pos指向nal type地址*/
                {
                    nal_size = nal_tail_pos - nal_head_pos - 4;
                    nal_tail_pos -= 4;
                    break;
                }
            }
        }

//        printf("  %s : nal_tail_pos = [%d].\n", __func__, nal_tail_pos);
        nal_size = nal_size == 0 ? nal_tail_pos - nal_head_pos : nal_size;
        nal_type = data[nal_head_pos] & 0x1F;
//        printf("%s : nal_type = [%d].\n", __func__, nal_type);

        nal->start = &data[nal_head_pos-start_code];
        nal->length = nal_size+start_code;
        nal->type = nal_type;

        h264->nal_head_pos = nal_tail_pos;
        break;
    }

    return 0;
}


static int h264_get_frame(H264Data_t *h264, H264Stream_t *h264_stream)
{
    int i = 0;

    if (h264 == NULL || h264_stream == NULL)
    {
        return -1;
    }

    for (i = 0; i < sizeof(h264_stream->nalu) / sizeof(h264_stream->nalu[0]); i++)
    {
        h264_get_nal(h264, &h264_stream->nalu[i]);
        if (h264_stream->nalu[i].type == 5)
        {
            h264_stream->is_iframe = 1;
            h264_stream->nalu_cnt = i + 1;
            break;
        }
        else if (h264_stream->nalu[i].type == 1)
        {
            h264_stream->is_iframe = 0;            
            h264_stream->nalu_cnt = i + 1;
            break;
        }
    }

    return 0;
}

int main(int argc, const char *argv[])
{
    int file_size = 0;
    FILE *in = NULL;
    FILE *out = NULL;
    H264Data_t h264;
    int pts = 0;
    int incremental = 3600;     /*时基90000,fps = 25:根据实际情况修改*/

    if (argc < 3)
    {
        printf("Usage:%s <input *.h264> <output *.vob>\n", argv[0]);
        return -1;
    }

    in = fopen(argv[1], "rb");
    out = fopen(argv[2], "wb");
    if (in == NULL || out == NULL)
    {
        printf("fopen [%s] or [%s] failed.\n", argv[1], argv[2]);
        return -1;
    }

    fseek(in, 0, SEEK_END);
    file_size = ftell(in);
    fseek(in, 0, SEEK_SET);

    memset(&h264, 0, sizeof(H264Data_t));
    h264.h264_data = (char *)malloc(file_size);
    if (h264.h264_data == NULL)
    {
        printf("malloc for file data failed.\n");
        return -1;
    }
    h264.size = fread(h264.h264_data, 1, file_size, in);

    while (h264.nal_head_pos < h264.size)
    {
        int i = 0;
        int ps_stream_len = 0;
        char *ps_stream = NULL;
        H264Stream_t h264_stream;
        memset(&h264_stream, 0, sizeof(H264Stream_t));

        h264_get_frame(&h264, &h264_stream);
        h264_stream.pts = pts;
        h264_stream.dts = pts;
        pts += incremental;

        ps_stream_len += PS_HDR_LEN;
        if (h264_stream.is_iframe)
        {
            ps_stream_len += SYS_HDR_LEN + PSM_HDR_LEN;
        }
        for (i = 0; i < h264_stream.nalu_cnt; i++)
        {
            ps_stream_len += PES_HDR_LEN + h264_stream.nalu[i].length;
        }

        ps_stream = (char *)malloc(ps_stream_len);
        if (ps_stream == NULL)
        {
            printf("malloc for ps stream failed.\n");
            return -1;
        }

        gb28181_package_for_h264(&h264_stream, ps_stream);

        fwrite(ps_stream, 1, ps_stream_len, out);
        free(ps_stream);
    }


    fclose(in);
    fclose(out);
    free(h264.h264_data);
    return 0;
}




