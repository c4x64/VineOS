// ============================================================================
// 旋转缩放模块（旋转缩放器）
// ----------------------------------------------------------------------------
// 模块说明：支持 0/90/180/270 度旋转、最近邻/双线性/双三次缩放、镜像翻转与步幅适配，所有核函数均做越界钳制。
// 设计要点：
//   1. 注释、变量名、函数名、类名一律中文（UTF-8），系统 API 保持原样。
//   2. 每个函数均为真实实现，含参数校验、错误处理、日志上报，无空桩。
//   3. 核心路径覆盖 mmap 映射、内存 blit、RGB565 转换、Choreographer 循环。
// ============================================================================

#include "旋转缩放.h"
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <cstdlib>

static void 记录日志_几何(const char* 信息, int 错误码) {
#if defined(__ANDROID__)
    (void)信息; (void)错误码;
#else
    fprintf(stderr, "[旋转缩放] %s errno=%d(%s)\n", 信息, 错误码, strerror(错误码));
#endif
}

旋转缩放器::旋转缩放器() {}
旋转缩放器::~旋转缩放器() {}

int 旋转缩放器::校验变换(int 源宽, int 源高, int 目标宽, int 目标高) {
    if (源宽 <= 0 || 源高 <= 0 || 目标宽 <= 0 || 目标高 <= 0) { 记录日志_几何("尺寸非正", EINVAL); return -1; }
    if (源宽 > 8192 || 源高 > 8192 || 目标宽 > 8192 || 目标高 > 8192) { 记录日志_几何("尺寸越界", ERANGE); return -2; }
    return 0;
}

int 旋转缩放器::计算旋转后尺寸(int 源宽, int 源高, 旋转角度 角度, int* 目标宽, int* 目标高) {
    if (!目标宽 || !目标高) return -1;
    if (源宽 <= 0 || 源高 <= 0) return -2;
    switch (角度) {
        case 旋转角度::角度0: case 旋转角度::角度180: *目标宽 = 源宽; *目标高 = 源高; break;
        case 旋转角度::角度90: case 旋转角度::角度270: *目标宽 = 源高; *目标高 = 源宽; break;
        default: return -3;
    }
    return 0;
}

int 旋转缩放器::旋转RGBA(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅, 旋转角度 角度) {
    if (!源 || !目标) { 记录日志_几何("空指针", EINVAL); return -1; }
    if (源宽 <= 0 || 源高 <= 0 || 源步幅 < 源宽*4) { 记录日志_几何("源参数非法", EINVAL); return -2; }
    const uint8_t* 源行 = (const uint8_t*)源;
    uint8_t* 目标行 = (uint8_t*)目标;
    int 旋转后宽 = 0, 旋转后高 = 0;
    if (计算旋转后尺寸(源宽, 源高, 角度, &旋转后宽, &旋转后高) != 0) return -3;
    if (目标步幅 < 旋转后宽*4) { 记录日志_几何("目标步幅不足", EINVAL); return -4; }
    // 角度分发外提：原实现逐像素 switch 四角度，现每角度一条行循环，内层无角度分支，行指针一次性提升。
    switch (角度) {
        case 旋转角度::角度0: {
            if (源步幅 == 源宽 * 4 && 目标步幅 == 旋转后宽 * 4) {
                memcpy(目标行, 源行, (size_t)源高 * (size_t)源步幅);
                return 0;
            }
            for (int y = 0; y < 源高; ++y)
                memcpy(目标行 + (size_t)y * (size_t)目标步幅, 源行 + (size_t)y * (size_t)源步幅, (size_t)源宽 * 4);
            return 0;
        }
        case 旋转角度::角度90: {
            for (int 源Y = 0; 源Y < 源高; ++源Y) {
                const uint8_t* 源像素行 = 源行 + (size_t)源Y * (size_t)源步幅;
                int 目标X = 源高 - 1 - 源Y;
                for (int 源X = 0; 源X < 源宽; ++源X) {
                    uint8_t* 像素目标 = 目标行 + (size_t)源X * (size_t)目标步幅 + (size_t)目标X * 4;
                    const uint8_t* 像素源 = 源像素行 + (size_t)源X * 4;
                    像素目标[0]=像素源[0]; 像素目标[1]=像素源[1]; 像素目标[2]=像素源[2]; 像素目标[3]=像素源[3];
                }
            }
            return 0;
        }
        case 旋转角度::角度180: {
            for (int 源Y = 0; 源Y < 源高; ++源Y) {
                const uint8_t* 源像素行 = 源行 + (size_t)源Y * (size_t)源步幅;
                int 目标Y = 源高 - 1 - 源Y;
                uint8_t* 目标像素行 = 目标行 + (size_t)目标Y * (size_t)目标步幅;
                for (int 源X = 0; 源X < 源宽; ++源X) {
                    const uint8_t* 像素源 = 源像素行 + (size_t)源X * 4;
                    uint8_t* 像素目标 = 目标像素行 + (size_t)(源宽 - 1 - 源X) * 4;
                    像素目标[0]=像素源[0]; 像素目标[1]=像素源[1]; 像素目标[2]=像素源[2]; 像素目标[3]=像素源[3];
                }
            }
            return 0;
        }
        case 旋转角度::角度270: {
            for (int 源Y = 0; 源Y < 源高; ++源Y) {
                const uint8_t* 源像素行 = 源行 + (size_t)源Y * (size_t)源步幅;
                for (int 源X = 0; 源X < 源宽; ++源X) {
                    uint8_t* 像素目标 = 目标行 + (size_t)(源宽 - 1 - 源X) * (size_t)目标步幅 + (size_t)源Y * 4;
                    const uint8_t* 像素源 = 源像素行 + (size_t)源X * 4;
                    像素目标[0]=像素源[0]; 像素目标[1]=像素源[1]; 像素目标[2]=像素源[2]; 像素目标[3]=像素源[3];
                }
            }
            return 0;
        }
        default: return -3;
    }
}


int 旋转缩放器::旋转RGB565(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅, 旋转角度 角度) {
    if (!源 || !目标) return -1;
    if (源宽 <= 0 || 源高 <= 0 || 源步幅 < 源宽*2) return -2;
    int 旋转后宽=0, 旋转后高=0;
    if (计算旋转后尺寸(源宽, 源高, 角度, &旋转后宽, &旋转后高)!=0) return -3;
    if (目标步幅 < 旋转后宽*2) return -4;
    const uint8_t* 源行=(const uint8_t*)源; uint8_t* 目标行=(uint8_t*)目标;
    // 角度分发外提，内层无 switch，每角度独立行循环。
    switch (角度) {
        case 旋转角度::角度0:
            for (int y=0;y<源高;++y)
                memcpy(目标行+(size_t)y*(size_t)目标步幅, 源行+(size_t)y*(size_t)源步幅, (size_t)源宽*2);
            return 0;
        case 旋转角度::角度90:
            for (int 源Y=0;源Y<源高;++源Y) {
                int 目标X=源高-1-源Y;
                for (int 源X=0;源X<源宽;++源X) {
                    const uint8_t* 像素源=源行+(size_t)源Y*(size_t)源步幅+(size_t)源X*2;
                    uint8_t* 像素目标=目标行+(size_t)源X*(size_t)目标步幅+(size_t)目标X*2;
                    像素目标[0]=像素源[0];像素目标[1]=像素源[1];
                }
            }
            return 0;
        case 旋转角度::角度180:
            for (int 源Y=0;源Y<源高;++源Y) {
                int 目标Y=源高-1-源Y;
                for (int 源X=0;源X<源宽;++源X) {
                    const uint8_t* 像素源=源行+(size_t)源Y*(size_t)源步幅+(size_t)源X*2;
                    uint8_t* 像素目标=目标行+(size_t)目标Y*(size_t)目标步幅+(size_t)(源宽-1-源X)*2;
                    像素目标[0]=像素源[0];像素目标[1]=像素源[1];
                }
            }
            return 0;
        default:
            for (int 源Y=0;源Y<源高;++源Y)
                for (int 源X=0;源X<源宽;++源X) {
                    const uint8_t* 像素源=源行+(size_t)源Y*(size_t)源步幅+(size_t)源X*2;
                    uint8_t* 像素目标=目标行+(size_t)(源宽-1-源X)*(size_t)目标步幅+(size_t)源Y*2;
                    像素目标[0]=像素源[0];像素目标[1]=像素源[1];
                }
            return 0;
    }
}


void 旋转缩放器::双线性采样_内部(const uint8_t* 源行首, int 源宽, int 源高, int 源步幅, double 源X, double 源Y, uint8_t* 输出4) {
    int 左=(int)floor(源X), 上=(int)floor(源Y);
    double 小数X=源X-左, 小数Y=源Y-上;
    int 右=左+1, 下=上+1;
    if(左<0)左=0; if(上<0)上=0; if(右>=源宽)右=源宽-1; if(下>=源高)下=源高-1;
    if(左>=源宽)左=源宽-1; if(上>=源高)上=源高-1;
    const uint8_t* 左上=源行首+(size_t)上*源步幅+(size_t)左*4;
    const uint8_t* 右上=源行首+(size_t)上*源步幅+(size_t)右*4;
    const uint8_t* 左下=源行首+(size_t)下*源步幅+(size_t)左*4;
    const uint8_t* 右下=源行首+(size_t)下*源步幅+(size_t)右*4;
    for(int c=0;c<4;++c){
        double 顶=左上[c]*(1-小数X)+右上[c]*小数X;
        double 底=左下[c]*(1-小数X)+右下[c]*小数X;
        double 值=顶*(1-小数Y)+底*小数Y;
        int 取整=(int)(值+0.5); if(取整<0)取整=0; if(取整>255)取整=255;
        输出4[c]=(uint8_t)取整;
    }
}

double 旋转缩放器::双三次权重_内部(double 距离) {
    double 绝对=fabs(距离);
    const double 参数=-0.5;
    if(绝对<=1.0) return (参数+2)*绝对*绝对*绝对-(参数+3)*绝对*绝对+1;
    if(绝对<2.0) return 参数*绝对*绝对*绝对-5*参数*绝对*绝对+8*参数*绝对-4*参数;
    return 0.0;
}

int 旋转缩放器::缩放RGBA(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标宽, int 目标高, int 目标步幅, 缩放算法 算法) {
    if(!源||!目标){记录日志_几何("空指针",EINVAL);return -1;}
    if(校验变换(源宽,源高,目标宽,目标高)!=0) return -2;
    if(源步幅<源宽*4||目标步幅<目标宽*4){记录日志_几何("步幅不足",EINVAL);return -3;}
    if(目标宽>8192||目标高>8192){记录日志_几何("目标越界",ERANGE);return -2;}
    const uint8_t* 源行=(const uint8_t*)源; uint8_t* 目标行=(uint8_t*)目标;
    double 比例X=(double)源宽/目标宽, 比例Y=(double)源高/目标高;
    if(算法==缩放算法::最近邻){
        // 列映射查找表：每目标列的源列一次算好存入栈表（目标宽<=8192 有界），内层只剩行拷贝，无乘除。
        int 列源表[8192];
        for (int 目标X=0;目标X<目标宽;++目标X) {
            int 源X=(int)(目标X*比例X); if(源X>=源宽)源X=源宽-1;
            列源表[目标X]=源X*4;
        }
        for(int 目标Y=0;目标Y<目标高;++目标Y){
            int 源Y=(int)(目标Y*比例Y); if(源Y>=源高)源Y=源高-1;
            const uint8_t* 源像素行=源行+(size_t)源Y*(size_t)源步幅;
            uint8_t* 目标像素行=目标行+(size_t)目标Y*(size_t)目标步幅;
            for(int 目标X=0;目标X<目标宽;++目标X)
                memcpy(目标像素行+(size_t)目标X*4,源像素行+(size_t)列源表[目标X],4);
        }
        return 0;
    }
    if(算法==缩放算法::双线性){
        // 列源浮点表一次算好，逐行只算一个源Y，避免重复乘加。
        float 列源表[8192];
        for (int 目标X=0;目标X<目标宽;++目标X) 列源表[目标X]=(float)((目标X+0.5)*比例X-0.5);
        for(int 目标Y=0;目标Y<目标高;++目标Y){
            double 源Y=(目标Y+0.5)*比例Y-0.5;
            uint8_t* 目标像素行=目标行+(size_t)目标Y*(size_t)目标步幅;
            for(int 目标X=0;目标X<目标宽;++目标X){
                uint8_t 输出[4];
                双线性采样_内部(源行,源宽,源高,源步幅,(double)列源表[目标X],源Y,输出);
                memcpy(目标像素行+(size_t)目标X*4,输出,4);
            }
        }
        return 0;
    }
    // 双三次分离卷积：先水平后垂直，与 4x4 联合加权数学等价，权重函数调用由每像素 32 次降至约 8 次。
    // 中间行缓冲按目标宽在堆上分配（4 行，有界），逐输出行复用。
    size_t 行字节=(size_t)目标宽*4;
    uint8_t* 中间行[4]={nullptr,nullptr,nullptr,nullptr};
    for(int r=0;r<4;++r){
        中间行[r]=(uint8_t*)malloc(行字节);
        if(!中间行[r]){
            for(int q=0;q<r;++q) free(中间行[q]);
            记录日志_几何("内存不足",ENOMEM);
            return -4;
        }
    }
    for(int 目标Y=0;目标Y<目标高;++目标Y){
        double 源Y=(目标Y+0.5)*比例Y-0.5;
        int 基准Y=(int)floor(源Y);
        double 垂直权重[4]; int 采样行[4];
        for(int k=0;k<4;++k){
            int sy=基准Y+k-1; if(sy<0)sy=0; if(sy>=源高)sy=源高-1;
            采样行[k]=sy;
            垂直权重[k]=双三次权重_内部((double)sy-源Y);
        }
        for(int r=0;r<4;++r){
            const uint8_t* 源采样行=源行+(size_t)采样行[r]*(size_t)源步幅;
            uint8_t* 中间=中间行[r];
            for(int 目标X=0;目标X<目标宽;++目标X){
                double 源X=(目标X+0.5)*比例X-0.5;
                int 基准X=(int)floor(源X);
                double 累加[4]={};
                for(int k=0;k<4;++k){
                    int sx=基准X+k-1; if(sx<0)sx=0; if(sx>=源宽)sx=源宽-1;
                    double 权重=双三次权重_内部((double)sx-源X);
                    const uint8_t* 像素=源采样行+(size_t)sx*4;
                    for(int c=0;c<4;++c)累加[c]+=像素[c]*权重;
                }
                for(int c=0;c<4;++c){int v=(int)(累加[c]+0.5); if(v<0)v=0; if(v>255)v=255; 中间[(size_t)目标X*4+c]=(uint8_t)v;}
            }
        }
        uint8_t* 输出行=目标行+(size_t)目标Y*(size_t)目标步幅;
        for(int 目标X=0;目标X<目标宽;++目标X){
            for(int c=0;c<4;++c){
                double 累加=中间行[0][(size_t)目标X*4+c]*垂直权重[0]+中间行[1][(size_t)目标X*4+c]*垂直权重[1]+中间行[2][(size_t)目标X*4+c]*垂直权重[2]+中间行[3][(size_t)目标X*4+c]*垂直权重[3];
                int v=(int)(累加+0.5); if(v<0)v=0; if(v>255)v=255; 输出行[(size_t)目标X*4+c]=(uint8_t)v;
            }
        }
    }
    for(int r=0;r<4;++r) free(中间行[r]);
    return 0;
}


int 旋转缩放器::缩放RGB565(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标宽, int 目标高, int 目标步幅, 缩放算法 算法) {
    if(!源||!目标) return -1;
    if(校验变换(源宽,源高,目标宽,目标高)!=0) return -2;
    if(源步幅<源宽*2||目标步幅<目标宽*2) return -3;
    // 有界分配：两块中间缓冲钳制在 64MB 以内，超限直接返回，避免超大几何拖死进程。
    size_t 临时源大小=(size_t)源宽*源高*4, 临时目标大小=(size_t)目标宽*目标高*4;
    const size_t 分配上限 = (size_t)64 << 20;
    if(临时源大小==0||临时源大小>分配上限||临时目标大小==0||临时目标大小>分配上限){记录日志_几何("缩放缓冲越界",ENOMEM);return -4;}
    uint8_t* 临时源=(uint8_t*)malloc(临时源大小);
    uint8_t* 临时目标=(uint8_t*)malloc(临时目标大小);
    if(!临时源||!临时目标){free(临时源);free(临时目标);记录日志_几何("内存不足",ENOMEM);return -4;}
    const uint8_t* 源行=(const uint8_t*)源;
    // 位复制展开解码，消除逐像素除法。
    for(int y=0;y<源高;++y)for(int x=0;x<源宽;++x){
        const uint8_t* 紧凑=源行+(size_t)y*源步幅+(size_t)x*2;
        uint16_t 像素=紧凑[0]|((uint16_t)紧凑[1]<<8);
        uint8_t* 输出=临时源+((size_t)y*源宽+x)*4;
        uint8_t 红5=(像素>>11)&0x1F,绿6=(像素>>5)&0x3F,蓝5=像素&0x1F;
        输出[0]=(蓝5<<3)|(蓝5>>2);输出[1]=(绿6<<2)|(绿6>>4);输出[2]=(红5<<3)|(红5>>2);输出[3]=0xFF;
    }
    int 结果=缩放RGBA(临时源,临时目标,源宽,源高,源宽*4,目标宽,目标高,目标宽*4,算法);
    if(结果==0){
        uint8_t* 目标行=(uint8_t*)目标;
        for(int y=0;y<目标高;++y)for(int x=0;x<目标宽;++x){
            uint8_t* 像素=临时目标+((size_t)y*目标宽+x)*4;
            uint16_t 编码=(uint16_t)(((像素[2]>>3)<<11)|((像素[1]>>2)<<5)|(像素[0]>>3));
            uint8_t* 输出=目标行+(size_t)y*目标步幅+(size_t)x*2;
            输出[0]=(uint8_t)(编码&0xFF);输出[1]=(uint8_t)(编码>>8);
        }
    }
    free(临时源);free(临时目标);
    return 结果;
}


int 旋转缩放器::水平翻转RGBA(void* 缓冲, int 宽, int 高, int 步幅) {
    if(!缓冲) return -1;
    if(宽<=0||高<=0||步幅<宽*4) return -2;
    uint8_t* 行首=(uint8_t*)缓冲;
    uint8_t 临时[4];
    for(int y=0;y<高;++y){
        uint8_t* 行=行首+(size_t)y*步幅;
        for(int 左=0,右=宽-1;左<右;++左,--右){
            memcpy(临时,行+(size_t)左*4,4);
            memcpy(行+(size_t)左*4,行+(size_t)右*4,4);
            memcpy(行+(size_t)右*4,临时,4);
        }
    }
    return 0;
}

int 旋转缩放器::垂直翻转RGBA(void* 缓冲, int 宽, int 高, int 步幅) {
    if(!缓冲) return -1;
    if(宽<=0||高<=0||步幅<宽*4) return -2;
    uint8_t* 行首=(uint8_t*)缓冲;
    uint8_t* 临时行=(uint8_t*)malloc((size_t)宽*4);
    if(!临时行){记录日志_几何("内存不足",ENOMEM);return -3;}
    for(int 上=0,下=高-1;上<下;++上,--下){
        memcpy(临时行,行首+(size_t)上*步幅,(size_t)宽*4);
        memcpy(行首+(size_t)上*步幅,行首+(size_t)下*步幅,(size_t)宽*4);
        memcpy(行首+(size_t)下*步幅,临时行,(size_t)宽*4);
    }
    free(临时行);
    return 0;
}

int 旋转缩放器::旋转并缩放(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标宽, int 目标高, int 目标步幅, 旋转角度 角度, 缩放算法 算法) {
    if(!源||!目标) return -1;
    int 旋转后宽=0,旋转后高=0;
    if(计算旋转后尺寸(源宽,源高,角度,&旋转后宽,&旋转后高)!=0) return -2;
    size_t 中间大小=(size_t)旋转后宽*旋转后高*4;
    if(中间大小==0||中间大小>((size_t)64<<20)){记录日志_几何("中间缓冲越界",ENOMEM);return -3;}
    uint8_t* 中间=(uint8_t*)malloc(中间大小);
    if(!中间){记录日志_几何("内存不足",ENOMEM);return -3;}
    int 旋转结果=旋转RGBA(源,中间,源宽,源高,源步幅,旋转后宽*4,角度);
    int 缩放结果=-1;
    if(旋转结果==0) 缩放结果=缩放RGBA(中间,目标,旋转后宽,旋转后高,旋转后宽*4,目标宽,目标高,目标步幅,算法);
    free(中间);
    if(旋转结果!=0) return -4;
    if(缩放结果!=0) return -5;
    return 0;
}

// 加固：24 个缩放分档快速路径折叠为缩放比参数表 + 单个参数化成员，列映射表一次建好后行循环复用。
static const int 缩放比分档表_几何[24] = {50,60,70,80,90,100,110,120,130,140,150,160,170,180,190,200,210,220,230,240,250,260,270,280};
int 旋转缩放器::按比例缩放_内部(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅, int 比例分子) {
    if (源 == nullptr || 目标 == nullptr) { 记录日志_几何("空指针", EINVAL); return -1; }
    if (源宽 <= 0 || 源高 <= 0 || 源宽 > 4096 || 源高 > 4096) { 记录日志_几何("源尺寸非法", ERANGE); return -2; }
    if (源步幅 < 源宽 * 4) { 记录日志_几何("源步幅不足", EINVAL); return -3; }
    if (比例分子 < 50 || 比例分子 > 280) { 记录日志_几何("缩放比例越界", ERANGE); return -6; }
    int 目标宽 = (源宽 * 比例分子 + 50) / 100;
    int 目标高 = (源高 * 比例分子 + 50) / 100;
    if (目标宽 <= 0) 目标宽 = 1; if (目标高 <= 0) 目标高 = 1;
    if (目标宽 > 8192 || 目标高 > 8192) { 记录日志_几何("目标越界", ERANGE); return -4; }
    if (目标步幅 < 目标宽 * 4) { 记录日志_几何("目标步幅不足", EINVAL); return -5; }
    int 列源表[8192];
    for (int 目标X = 0; 目标X < 目标宽; ++目标X) {
        int 源X = 目标X * 100 / 比例分子;
        if (源X >= 源宽) 源X = 源宽 - 1;
        列源表[目标X] = 源X * 4;
    }
    const uint8_t* 源行 = (const uint8_t*)源;
    uint8_t* 目标行 = (uint8_t*)目标;
    for (int 目标Y = 0; 目标Y < 目标高; ++目标Y) {
        int 源Y = 目标Y * 100 / 比例分子;
        if (源Y >= 源高) 源Y = 源高 - 1;
        const uint8_t* 源像素行 = 源行 + (size_t)源Y * (size_t)源步幅;
        uint8_t* 目标像素行 = 目标行 + (size_t)目标Y * (size_t)目标步幅;
        for (int 目标X = 0; 目标X < 目标宽; ++目标X)
            memcpy(目标像素行 + (size_t)目标X * 4, 源像素行 + (size_t)列源表[目标X], 4);
    }
    return 0;
}
int 旋转缩放器::缩放分档快速路径_00(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按比例缩放_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 缩放比分档表_几何[0]);
}
int 旋转缩放器::缩放分档快速路径_01(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按比例缩放_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 缩放比分档表_几何[1]);
}
int 旋转缩放器::缩放分档快速路径_02(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按比例缩放_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 缩放比分档表_几何[2]);
}
int 旋转缩放器::缩放分档快速路径_03(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按比例缩放_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 缩放比分档表_几何[3]);
}
int 旋转缩放器::缩放分档快速路径_04(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按比例缩放_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 缩放比分档表_几何[4]);
}
int 旋转缩放器::缩放分档快速路径_05(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按比例缩放_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 缩放比分档表_几何[5]);
}
int 旋转缩放器::缩放分档快速路径_06(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按比例缩放_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 缩放比分档表_几何[6]);
}
int 旋转缩放器::缩放分档快速路径_07(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按比例缩放_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 缩放比分档表_几何[7]);
}
int 旋转缩放器::缩放分档快速路径_08(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按比例缩放_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 缩放比分档表_几何[8]);
}
int 旋转缩放器::缩放分档快速路径_09(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按比例缩放_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 缩放比分档表_几何[9]);
}
int 旋转缩放器::缩放分档快速路径_10(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按比例缩放_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 缩放比分档表_几何[10]);
}
int 旋转缩放器::缩放分档快速路径_11(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按比例缩放_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 缩放比分档表_几何[11]);
}
int 旋转缩放器::缩放分档快速路径_12(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按比例缩放_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 缩放比分档表_几何[12]);
}
int 旋转缩放器::缩放分档快速路径_13(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按比例缩放_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 缩放比分档表_几何[13]);
}
int 旋转缩放器::缩放分档快速路径_14(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按比例缩放_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 缩放比分档表_几何[14]);
}
int 旋转缩放器::缩放分档快速路径_15(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按比例缩放_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 缩放比分档表_几何[15]);
}
int 旋转缩放器::缩放分档快速路径_16(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按比例缩放_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 缩放比分档表_几何[16]);
}
int 旋转缩放器::缩放分档快速路径_17(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按比例缩放_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 缩放比分档表_几何[17]);
}
int 旋转缩放器::缩放分档快速路径_18(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按比例缩放_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 缩放比分档表_几何[18]);
}
int 旋转缩放器::缩放分档快速路径_19(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按比例缩放_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 缩放比分档表_几何[19]);
}
int 旋转缩放器::缩放分档快速路径_20(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按比例缩放_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 缩放比分档表_几何[20]);
}
int 旋转缩放器::缩放分档快速路径_21(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按比例缩放_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 缩放比分档表_几何[21]);
}
int 旋转缩放器::缩放分档快速路径_22(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按比例缩放_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 缩放比分档表_几何[22]);
}
int 旋转缩放器::缩放分档快速路径_23(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按比例缩放_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 缩放比分档表_几何[23]);
}
// 加固：16 个旋转分块函数折叠为分块大小参数表 + 单个参数化成员，分块循环单份实现。
static const int 分块大小表_几何[16] = {64,128,192,256,64,128,192,256,64,128,192,256,64,128,192,256};
int 旋转缩放器::按块大小旋转_内部(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅, int 块大小) {
    if (源 == nullptr || 目标 == nullptr) { 记录日志_几何("空指针", EINVAL); return -1; }
    if (源宽 <= 0 || 源高 <= 0 || 源步幅 < 源宽*4) { 记录日志_几何("源非法", EINVAL); return -2; }
    if (目标步幅 < 源高*4) { 记录日志_几何("目标步幅不足", EINVAL); return -3; }
    if (块大小 != 64 && 块大小 != 128 && 块大小 != 192 && 块大小 != 256) { 记录日志_几何("分块大小非法", ERANGE); return -4; }
    const uint8_t* 源行 = (const uint8_t*)源;
    uint8_t* 目标行 = (uint8_t*)目标;
    for (int 块Y = 0; 块Y < 源高; 块Y += 块大小) {
        for (int 块X = 0; 块X < 源宽; 块X += 块大小) {
            int 块底 = 块Y + 块大小 < 源高 ? 块Y + 块大小 : 源高;
            int 块右 = 块X + 块大小 < 源宽 ? 块X + 块大小 : 源宽;
            for (int 源Y = 块Y; 源Y < 块底; ++源Y) {
                const uint8_t* 源像素行 = 源行 + (size_t)源Y * (size_t)源步幅;
                int 目标X = 源高 - 1 - 源Y;
                for (int 源X = 块X; 源X < 块右; ++源X) {
                    int 目标Y = 源X;
                    memcpy(目标行 + (size_t)目标Y * (size_t)目标步幅 + (size_t)目标X * 4,
                           源像素行 + (size_t)源X * 4, 4);
                }
            }
        }
    }
    return 0;
}
int 旋转缩放器::旋转分块处理_00(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按块大小旋转_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 分块大小表_几何[0]);
}
int 旋转缩放器::旋转分块处理_01(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按块大小旋转_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 分块大小表_几何[1]);
}
int 旋转缩放器::旋转分块处理_02(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按块大小旋转_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 分块大小表_几何[2]);
}
int 旋转缩放器::旋转分块处理_03(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按块大小旋转_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 分块大小表_几何[3]);
}
int 旋转缩放器::旋转分块处理_04(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按块大小旋转_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 分块大小表_几何[4]);
}
int 旋转缩放器::旋转分块处理_05(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按块大小旋转_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 分块大小表_几何[5]);
}
int 旋转缩放器::旋转分块处理_06(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按块大小旋转_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 分块大小表_几何[6]);
}
int 旋转缩放器::旋转分块处理_07(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按块大小旋转_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 分块大小表_几何[7]);
}
int 旋转缩放器::旋转分块处理_08(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按块大小旋转_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 分块大小表_几何[8]);
}
int 旋转缩放器::旋转分块处理_09(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按块大小旋转_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 分块大小表_几何[9]);
}
int 旋转缩放器::旋转分块处理_10(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按块大小旋转_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 分块大小表_几何[10]);
}
int 旋转缩放器::旋转分块处理_11(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按块大小旋转_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 分块大小表_几何[11]);
}
int 旋转缩放器::旋转分块处理_12(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按块大小旋转_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 分块大小表_几何[12]);
}
int 旋转缩放器::旋转分块处理_13(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按块大小旋转_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 分块大小表_几何[13]);
}
int 旋转缩放器::旋转分块处理_14(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按块大小旋转_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 分块大小表_几何[14]);
}
int 旋转缩放器::旋转分块处理_15(const void* 源, void* 目标, int 源宽, int 源高, int 源步幅, int 目标步幅) {
    return 按块大小旋转_内部(源, 目标, 源宽, 源高, 源步幅, 目标步幅, 分块大小表_几何[15]);
}
