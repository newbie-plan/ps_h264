#ifndef __H264_TO_PS_H__
#define __H264_TO_PS_H__

#define TRUE  1
#define FALSE 0
#define PS_HDR_LEN  14
#define SYS_HDR_LEN 18
#define PSM_HDR_LEN 24
#define PES_HDR_LEN 19

typedef struct
{
    int type;
    int length;
    char *start;
}H264Nalu_t;


typedef struct
{
    int is_iframe;
    unsigned long long pts;
    unsigned long long dts;
    int nalu_cnt;
    H264Nalu_t nalu[4];
}H264Stream_t;


int gb28181_package_for_h264(H264Stream_t *h264_stream, char *ps_stream);


#endif



